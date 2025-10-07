#include "messaging.h"
#include <cstring>
#include <chrono>

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

    Impl(Crypto& c) : retry_thread(&Impl::retry_worker, this), crypto(c) {
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
            queue_cv.wait_for(lock, std::chrono::milliseconds(1000), [this]() { return !running; }); // Check periodically
            if (!running) break;

            auto now = std::chrono::steady_clock::now();

            // Handle pending retries
            while (!pending_messages.empty() && pending_messages.front().next_retry <= now) {
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
                        // Send failed, requeue immediately? Or increment retry
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
            }

            // Check ack timeouts
            for (auto it = ack_waiting.begin(); it != ack_waiting.end(); ) {
                if (it->second.next_retry <= now) {
                    pending_messages.push(it->second);
                    it = ack_waiting.erase(it);
                } else {
                    ++it;
                }
            }

            lock.unlock();
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
                                              const std::vector<uint8_t>& content, uint8_t status) {
    std::vector<uint8_t> buffer;

    // Encrypt content
    auto encrypted_content = pimpl->crypto.encrypt_message(receiver_id, content);

    // Calculate sizes
    uint32_t id_len = id.size();
    uint32_t conv_len = conversation_id.size();
    uint32_t sender_len = sender_id.size();
    uint32_t receiver_len = receiver_id.size();
    uint32_t content_size = encrypted_content.size();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Create frame header
    MessageFrame frame;
    frame.id_len = id_len;
    frame.conversation_id_len = conv_len;
    frame.sender_id_len = sender_len;
    frame.receiver_id_len = receiver_len;
    frame.timestamp = timestamp;
    frame.content_size = content_size;
    frame.status = status;

    // Calculate CRC32 over the data part
    std::vector<uint8_t> data_part;
    data_part.insert(data_part.end(), reinterpret_cast<uint8_t*>(&frame.id_len), reinterpret_cast<uint8_t*>(&frame.id_len) + sizeof(frame) - sizeof(frame.crc32));
    data_part.insert(data_part.end(), id.begin(), id.end());
    data_part.insert(data_part.end(), conversation_id.begin(), conversation_id.end());
    data_part.insert(data_part.end(), sender_id.begin(), sender_id.end());
    data_part.insert(data_part.end(), receiver_id.begin(), receiver_id.end());
    data_part.insert(data_part.end(), encrypted_content.begin(), encrypted_content.end());

    frame.crc32 = pimpl->crc32(data_part.data(), data_part.size());

    // Pack into buffer
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&frame), reinterpret_cast<uint8_t*>(&frame) + sizeof(frame));
    buffer.insert(buffer.end(), id.begin(), id.end());
    buffer.insert(buffer.end(), conversation_id.begin(), conversation_id.end());
    buffer.insert(buffer.end(), sender_id.begin(), sender_id.end());
    buffer.insert(buffer.end(), receiver_id.begin(), receiver_id.end());
    buffer.insert(buffer.end(), encrypted_content.begin(), encrypted_content.end());

    return buffer;
}

bool Messaging::unpack_message(const std::vector<uint8_t>& data, std::string& id, std::string& conversation_id,
                                std::string& sender_id, std::string& receiver_id,
                                std::vector<uint8_t>& content, uint8_t& status, uint64_t& timestamp) {
    if (data.size() < sizeof(MessageFrame)) return false;

    MessageFrame frame;
    std::memcpy(&frame, data.data(), sizeof(MessageFrame));

    size_t offset = sizeof(MessageFrame);

    if (offset + frame.id_len > data.size()) return false;
    id.assign(data.begin() + offset, data.begin() + offset + frame.id_len);
    offset += frame.id_len;

    if (offset + frame.conversation_id_len > data.size()) return false;
    conversation_id.assign(data.begin() + offset, data.begin() + offset + frame.conversation_id_len);
    offset += frame.conversation_id_len;

    if (offset + frame.sender_id_len > data.size()) return false;
    sender_id.assign(data.begin() + offset, data.begin() + offset + frame.sender_id_len);
    offset += frame.sender_id_len;

    if (offset + frame.receiver_id_len > data.size()) return false;
    receiver_id.assign(data.begin() + offset, data.begin() + offset + frame.receiver_id_len);
    offset += frame.receiver_id_len;

    if (offset + frame.content_size > data.size()) return false;
    std::vector<uint8_t> encrypted_content(data.begin() + offset, data.begin() + offset + frame.content_size);

    status = frame.status;
    timestamp = frame.timestamp;

    // Verify CRC32
    std::vector<uint8_t> data_part(data.begin() + sizeof(uint32_t), data.end());
    uint32_t calculated_crc = pimpl->crc32(data_part.data(), data_part.size());
    if (calculated_crc != frame.crc32) return false;

    // Decrypt content
    content = pimpl->crypto.decrypt_message(sender_id, encrypted_content);
    return true;
}

uint32_t Messaging::crc32(const uint8_t* data, size_t length) {
    return pimpl->crc32(data, length);
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