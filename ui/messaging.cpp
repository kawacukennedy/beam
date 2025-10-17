#include "messaging.h"
#include "../crypto/crypto.h"
#include <cstring>
#include <chrono>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <functional>

class Messaging::Impl {
public:
    static constexpr int MAX_RETRIES = 3;
    static constexpr int BASE_BACKOFF_MS = 500;
    static constexpr int ACK_TIMEOUT_MS = 5000;
    uint32_t crc32_table[256];
    MessageCallback message_callback;
    std::function<bool(const std::string& device_id, const std::vector<uint8_t>& data)> bluetooth_sender;
    std::queue<PendingMessage> pending_messages;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::thread retry_thread;
    bool running = true;
    std::unordered_map<std::string, PendingMessage> ack_waiting;
    std::mutex ack_mutex;
    Crypto& crypto;

    Impl(Crypto& c) : crypto(c), retry_thread(&Impl::retry_worker, this) {
        // Initialize CRC32 table
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
            }
            crc32_table[i] = crc;
        }
    }

    ~Impl() {
        running = false;
        queue_cv.notify_one();
        if (retry_thread.joinable()) retry_thread.join();
    }

    uint32_t crc32(const uint8_t* data, size_t length) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < length; ++i) {
            crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFF;
    }

    void retry_worker() {
        while (running) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait_for(lock, std::chrono::milliseconds(2000), [this]() { return !running; }); // Reduce frequency for low power
            if (!running) break;

            auto now = std::chrono::steady_clock::now();

            // Handle pending retries (limit to 5 per cycle to reduce CPU)
            int processed = 0;
            while (!pending_messages.empty() && pending_messages.front().next_retry <= now && processed < 5) {
                PendingMessage msg = pending_messages.front();
                pending_messages.pop();
                lock.unlock();

                if (msg.retry_count < Impl::MAX_RETRIES) {
                    if (bluetooth_sender && bluetooth_sender(msg.receiver_id, msg.data)) {
                        msg.retry_count++;
                        msg.next_retry = now + std::chrono::milliseconds(Impl::BASE_BACKOFF_MS * (1 << msg.retry_count));
                        lock.lock();
                        pending_messages.push(msg);
                        lock.unlock();
                    } else {
                        msg.retry_count++;
                        if (msg.retry_count < Impl::MAX_RETRIES) {
                            msg.next_retry = now + std::chrono::milliseconds(Impl::BASE_BACKOFF_MS * (1 << msg.retry_count));
                            lock.lock();
                            pending_messages.push(msg);
                            lock.unlock();
                        } else {
                            if (message_callback) {
                                message_callback(msg.id, "", "", msg.receiver_id, {}, MessageStatus::SENT);
                            }
                        }
                    }
                } else {
                    if (message_callback) {
                        message_callback(msg.id, "", "", msg.receiver_id, {}, MessageStatus::SENT);
                    }
                }
                lock.lock();
                processed++;
            }

            // Check ack timeouts (limit iterations)
            int ack_processed = 0;
            for (auto it = ack_waiting.begin(); it != ack_waiting.end() && ack_processed < 10; ) {
                if (it->second.next_retry <= now) {
                    pending_messages.push(it->second);
                    it = ack_waiting.erase(it);
                } else {
                    ++it;
                }
                ack_processed++;
            }

            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Yield CPU
        }
    }
};

Messaging::Messaging(Crypto& crypto) : pimpl(std::make_unique<Impl>(crypto)) {}
Messaging::~Messaging() = default;

void Messaging::set_message_callback(MessageCallback callback) {
    pimpl->message_callback = callback;
}

void Messaging::set_bluetooth_sender(std::function<bool(const std::string& device_id, const std::vector<uint8_t>& data)> sender) {
    pimpl->bluetooth_sender = sender;
}

std::vector<uint8_t> Messaging::pack_message(const std::string& id, const std::string& conversation_id,
                                           const std::string& sender_id, const std::string& receiver_id,
                                           const std::vector<uint8_t>& content, uint64_t timestamp) {
    // Encrypt content
    auto encrypted_content = pimpl->crypto.encrypt_message(receiver_id, content);

    // Create frame
    MessageFrame frame;
    auto [id_high, id_low] = parse_id(id);
    frame.id[0] = id_high;
    frame.id[1] = id_low;
    auto [conv_high, conv_low] = parse_id(conversation_id);
    frame.conversation_id[0] = conv_high;
    frame.conversation_id[1] = conv_low;
    auto [send_high, send_low] = parse_id(sender_id);
    frame.sender_id[0] = send_high;
    frame.sender_id[1] = send_low;
    auto [recv_high, recv_low] = parse_id(receiver_id);
    frame.receiver_id[0] = recv_high;
    frame.receiver_id[1] = recv_low;
    frame.timestamp_unix_ms = timestamp;
    frame.content_size = encrypted_content.size();
    frame.length = sizeof(MessageFrame) + encrypted_content.size() + sizeof(uint32_t);

    // Pack into buffer
    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&frame), reinterpret_cast<uint8_t*>(&frame) + sizeof(frame));
    buffer.insert(buffer.end(), encrypted_content.begin(), encrypted_content.end());

    // Calculate CRC32 over buffer so far
    uint32_t crc = pimpl->crc32(buffer.data(), buffer.size());
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&crc), reinterpret_cast<uint8_t*>(&crc) + sizeof(crc));

    return buffer;
}

bool Messaging::unpack_message(const std::vector<uint8_t>& data, std::string& id, std::string& conversation_id,
                                std::string& sender_id, std::string& receiver_id,
                                std::vector<uint8_t>& content, uint64_t& timestamp) {
    if (data.size() < sizeof(MessageFrame) + sizeof(uint32_t)) return false;

    MessageFrame frame;
    std::memcpy(&frame, data.data(), sizeof(MessageFrame));

    if (frame.length != data.size()) return false;

    size_t content_start = sizeof(MessageFrame);
    if (content_start + frame.content_size + sizeof(uint32_t) > data.size()) return false;

    std::vector<uint8_t> encrypted_content(data.begin() + content_start, data.begin() + content_start + frame.content_size);

    // Verify CRC32
    std::vector<uint8_t> data_part(data.begin(), data.begin() + content_start + frame.content_size);
    uint32_t calculated_crc = pimpl->crc32(data_part.data(), data_part.size());
    uint32_t received_crc;
    std::memcpy(&received_crc, data.data() + data.size() - sizeof(uint32_t), sizeof(uint32_t));
    if (calculated_crc != received_crc) return false;

    // Decrypt content
    content = pimpl->crypto.decrypt_message(id_to_string(frame.sender_id[0], frame.sender_id[1]), encrypted_content);

    id = id_to_string(frame.id[0], frame.id[1]);
    conversation_id = id_to_string(frame.conversation_id[0], frame.conversation_id[1]);
    sender_id = id_to_string(frame.sender_id[0], frame.sender_id[1]);
    receiver_id = id_to_string(frame.receiver_id[0], frame.receiver_id[1]);
    timestamp = frame.timestamp_unix_ms;

    return true;
}



bool Messaging::send_message(const std::string& id, const std::string& conversation_id,
                             const std::string& sender_id, const std::string& receiver_id,
                             const std::vector<uint8_t>& content, MessageStatus status) {
    try {
        auto data = pack_message(id, conversation_id, sender_id, receiver_id, content, static_cast<uint8_t>(status));
        if (data.empty()) {
            // Packing failed
            return false;
        }
        if (pimpl->bluetooth_sender && pimpl->bluetooth_sender(receiver_id, data)) {
            PendingMessage pm{id, data, receiver_id, 0, std::chrono::steady_clock::now() + std::chrono::milliseconds(Impl::ACK_TIMEOUT_MS)};
            std::lock_guard<std::mutex> lock(pimpl->ack_mutex);
            pimpl->ack_waiting[id] = pm;
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error sending message: " << e.what() << std::endl;
        return false;
    }
}

void Messaging::receive_data(const std::string& sender_id, const std::vector<uint8_t>& data) {
    // Check if it's an ACK
    std::string ack_message_id;
    if (unpack_ack(data, ack_message_id)) {
        std::lock_guard<std::mutex> lock(pimpl->ack_mutex);
        pimpl->ack_waiting.erase(ack_message_id);
        return;
    }

    // Otherwise, it's a message
    std::string id, conversation_id, sender, receiver;
    std::vector<uint8_t> content;
    uint8_t status;
    uint64_t timestamp;
    if (unpack_message(data, id, conversation_id, sender, receiver, content, status, timestamp)) {
        if (pimpl->message_callback) {
            pimpl->message_callback(id, conversation_id, sender, receiver, content, static_cast<MessageStatus>(status));
        }
        // Send ACK
        auto ack_data = pack_ack(id);
        if (pimpl->bluetooth_sender) {
            pimpl->bluetooth_sender(sender_id, ack_data);
        }
    }
}

std::vector<uint8_t> Messaging::pack_ack(const std::string& message_id) {
    std::vector<uint8_t> buffer;
    uint32_t magic = MAGIC;
    uint32_t id_len = message_id.size();
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&magic), reinterpret_cast<uint8_t*>(&magic) + sizeof(magic));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&id_len), reinterpret_cast<uint8_t*>(&id_len) + sizeof(id_len));
    buffer.insert(buffer.end(), message_id.begin(), message_id.end());
    return buffer;
}

bool Messaging::unpack_ack(const std::vector<uint8_t>& data, std::string& message_id) {
    if (data.size() < sizeof(uint32_t) * 2) return false;
    uint32_t magic, id_len;
    size_t offset = 0;
    std::memcpy(&magic, data.data() + offset, sizeof(magic));
    offset += sizeof(magic);
    if (magic != MAGIC) return false;
    std::memcpy(&id_len, data.data() + offset, sizeof(id_len));
    offset += sizeof(id_len);
    if (offset + id_len != data.size()) return false;
    message_id.assign(data.begin() + offset, data.end());
    return true;
}