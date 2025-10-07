#include "file_transfer.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <cstring>
#include <openssl/sha.h>

class FileTransfer::Impl {
public:
    std::unordered_map<std::string, TransferSession> active_transfers;
    std::mutex transfer_mutex;
    std::queue<std::string> transfer_queue; // FIFO queue for file IDs
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    bool stop_processing = false;
    std::function<bool(const std::string& device_id, const std::vector<uint8_t>& data)> data_sender;
    std::unordered_map<std::string, std::ofstream> receiving_files;
    std::unordered_map<std::string, uint64_t> received_bytes;
    std::unordered_map<std::string, std::string> receiving_checksums;
    std::unordered_map<std::string, std::string> receiving_paths;
    std::mutex receive_mutex;

    uint32_t crc32_table[256];

    Impl() {
        // Initialize CRC32 table
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
            }
            crc32_table[i] = crc;
        }
    }

    uint32_t crc32(const uint8_t* data, size_t length) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < length; ++i) {
            crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFF;
    }

    std::vector<uint8_t> encode_unicode(const std::string& str) {
        std::vector<uint8_t> result;
        for (char c : str) {
            result.push_back(0); // High byte
            result.push_back(c); // Low byte
        }
        result.push_back(0);
        result.push_back(0);
        return result;
    }

    std::string decode_unicode(const std::vector<uint8_t>& data) {
        std::string result;
        for (size_t i = 0; i < data.size(); i += 2) {
            if (i + 1 < data.size()) {
                result += data[i + 1];
            }
        }
        return result;
    }

    std::string sha256_file(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return "";

        SHA256_CTX sha256;
        SHA256_Init(&sha256);

        char buffer[8192];
        while (file.read(buffer, sizeof(buffer))) {
            SHA256_Update(&sha256, buffer, file.gcount());
        }
        SHA256_Update(&sha256, buffer, file.gcount());

        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_Final(hash, &sha256);

        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

    std::vector<FileChunk> create_chunks(const std::string& file_path, const std::string& file_id) {
        std::vector<FileChunk> chunks;
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file) return chunks;

        uint64_t file_size = file.tellg();
        file.seekg(0);

        uint64_t offset = 0;
        while (offset < file_size) {
            uint64_t chunk_size = std::min(CHUNK_SIZE, file_size - offset);
            std::vector<uint8_t> data(chunk_size);
            file.read(reinterpret_cast<char*>(data.data()), chunk_size);

            FileChunk chunk;
            chunk.file_id = file_id;
            chunk.offset = offset;
            chunk.data = std::move(data);
            chunk.checksum = crc32(chunk.data.data(), chunk.data.size());
            chunk.retry_count = 0;

            chunks.push_back(std::move(chunk));
            offset += chunk_size;
        }

        return chunks;
    }

    void process_transfer_queue() {
        while (!stop_processing) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this]() { return !transfer_queue.empty() || stop_processing; });

            if (stop_processing) break;

            std::string file_id = transfer_queue.front();
            transfer_queue.pop();
            lock.unlock();

            // Process transfer
            std::unique_lock<std::mutex> transfer_lock(transfer_mutex);
            auto it = active_transfers.find(file_id);
            if (it != active_transfers.end()) {
                TransferSession& session = it->second;
                transfer_lock.unlock();

                 // Send chunks
                 while (!session.chunk_queue.empty() && session.active) {
                     FileChunk chunk = session.chunk_queue.front();
                     session.chunk_queue.pop();

                     auto packet = create_chunk_packet(chunk);
                     if (data_sender && data_sender(session.receiver_id, packet)) {
                         session.bytes_sent += chunk.data.size();
                         // Send ACK expected, but for now assume success
                     } else {
                         // Retry logic
                         if (chunk.retry_count < MAX_RETRIES) {
                             chunk.retry_count++;
                             session.chunk_queue.push(chunk); // Requeue
                             std::this_thread::sleep_for(std::chrono::milliseconds(BACKOFF_MS * chunk.retry_count));
                         }
                     }

                     // Update progress
                     // TODO: Call progress callback
                 }
            }
        }
    }
};

FileTransfer::FileTransfer() : pimpl(std::make_unique<Impl>()) {
    // Start transfer processing thread
    std::thread(&Impl::process_transfer_queue, pimpl.get()).detach();
}

FileTransfer::~FileTransfer() {
    pimpl->stop_processing = true;
    pimpl->queue_cv.notify_all();
}

std::vector<uint8_t> FileTransfer::create_connect_packet() {
    std::vector<uint8_t> packet;
    OBEXHeader header;
    header.opcode = static_cast<uint8_t>(OBEXOpcode::CONNECT);
    header.length = 7; // Header + version + flags + max_packet

    packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&header), reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    packet.push_back(0x10); // Version
    packet.push_back(0x00); // Flags
    packet.push_back(0x20); // Max packet size high
    packet.push_back(0x00); // Max packet size low (8192)

    return packet;
}

std::vector<uint8_t> FileTransfer::create_put_packet(const std::string& filename, uint64_t file_size, const std::vector<uint8_t>& data, bool is_final) {
    std::vector<uint8_t> packet;
    std::vector<uint8_t> headers;

    // Name header
    auto name_data = pimpl->encode_unicode(filename);
    uint16_t name_len = 3 + name_data.size(); // HI + Len + data
    headers.push_back(static_cast<uint8_t>(OBEXHeaderId::NAME));
    headers.push_back(name_len >> 8);
    headers.push_back(name_len & 0xFF);
    headers.insert(headers.end(), name_data.begin(), name_data.end());

    // Length header
    headers.push_back(static_cast<uint8_t>(OBEXHeaderId::LENGTH));
    headers.push_back(0x00);
    headers.push_back(0x00);
    headers.push_back(0x00);
    headers.push_back(0x05); // 5 bytes for length
    headers.push_back((file_size >> 24) & 0xFF);
    headers.push_back((file_size >> 16) & 0xFF);
    headers.push_back((file_size >> 8) & 0xFF);
    headers.push_back(file_size & 0xFF);

    // Body or End-of-Body
    uint8_t body_hi = is_final ? static_cast<uint8_t>(OBEXHeaderId::END_OF_BODY) : static_cast<uint8_t>(OBEXHeaderId::BODY);
    uint16_t body_len = 3 + data.size();
    headers.push_back(body_hi);
    headers.push_back(body_len >> 8);
    headers.push_back(body_len & 0xFF);
    headers.insert(headers.end(), data.begin(), data.end());

    OBEXHeader obex_header;
    obex_header.opcode = static_cast<uint8_t>(OBEXOpcode::PUT);
    obex_header.length = sizeof(OBEXHeader) + headers.size();

    packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&obex_header), reinterpret_cast<uint8_t*>(&obex_header) + sizeof(obex_header));
    packet.insert(packet.end(), headers.begin(), headers.end());

    return packet;
}

std::vector<uint8_t> FileTransfer::create_disconnect_packet() {
    std::vector<uint8_t> packet;
    OBEXHeader header;
    header.opcode = static_cast<uint8_t>(OBEXOpcode::DISCONNECT);
    header.length = sizeof(OBEXHeader);

    packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&header), reinterpret_cast<uint8_t*>(&header) + sizeof(header));

    return packet;
}

std::vector<uint8_t> FileTransfer::create_abort_packet() {
    std::vector<uint8_t> packet;
    OBEXHeader header;
    header.opcode = static_cast<uint8_t>(OBEXOpcode::ABORT);
    header.length = sizeof(OBEXHeader);

    packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&header), reinterpret_cast<uint8_t*>(&header) + sizeof(header));

    return packet;
}

bool FileTransfer::parse_obex_packet(const std::vector<uint8_t>& data, OBEXHeader& header, std::vector<uint8_t>& headers) {
    if (data.size() < sizeof(OBEXHeader)) return false;

    std::memcpy(&header, data.data(), sizeof(OBEXHeader));

    if (data.size() < header.length) return false;

    headers.assign(data.begin() + sizeof(OBEXHeader), data.end());

    return true;
}

bool FileTransfer::send_file(const std::string& path, const std::string& receiver_id,
                             ProgressCallback progress_cb, CompletionCallback completion_cb) {
    if (!std::filesystem::exists(path)) {
        if (completion_cb) completion_cb(false, "File does not exist");
        return false;
    }

    std::filesystem::path file_path(path);
    uint64_t file_size = std::filesystem::file_size(file_path);
    if (file_size > MAX_FILE_SIZE) {
        if (completion_cb) completion_cb(false, "File too large");
        return false;
    }

    std::string checksum = pimpl->sha256_file(path);
    if (checksum.empty()) {
        if (completion_cb) completion_cb(false, "Failed to calculate checksum");
        return false;
    }

    std::string file_id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string filename = file_path.filename().string();

    // Create transfer session
    TransferSession session;
    session.file_id = file_id;
    session.filename = filename;
    session.file_size = file_size;
    session.checksum = checksum;
    session.receiver_id = receiver_id;
    session.bytes_sent = 0;
    session.active = true;

    auto chunks = pimpl->create_chunks(path, file_id);
    for (auto& chunk : chunks) {
        session.chunk_queue.push(chunk);
    }

    {
        std::lock_guard<std::mutex> lock(pimpl->transfer_mutex);
        pimpl->active_transfers[file_id] = std::move(session);
    }

    // Queue for processing
    {
        std::lock_guard<std::mutex> lock(pimpl->queue_mutex);
        pimpl->transfer_queue.push(file_id);
    }
    pimpl->queue_cv.notify_one();

    // Send Connect packet first
    auto connect_packet = create_connect_packet();
    if (data_sender) {
        data_sender(receiver_id, connect_packet);
    }

    // Send Put packet with file data (simplified, send all at once)
    auto file_data = std::vector<uint8_t>(file_size);
    // Read file data
    std::ifstream file(path, std::ios::binary);
    file.read(reinterpret_cast<char*>(file_data.data()), file_size);

    auto put_packet = create_put_packet(filename, file_size, file_data, true);
    if (data_sender) {
        data_sender(receiver_id, put_packet);
    }

    // Send Disconnect
    auto disconnect_packet = create_disconnect_packet();
    if (data_sender) {
        data_sender(receiver_id, disconnect_packet);
    }

    if (completion_cb) completion_cb(true, "");
    return true;
}

bool FileTransfer::receive_file(const std::string& file_id, const std::string& filename, uint64_t size,
                                const std::string& checksum, const std::string& save_path,
                                ProgressCallback progress_cb, CompletionCallback completion_cb) {
    // TODO: Implement receiver logic for handling incoming chunks
    // This would involve listening for packets, validating checksums, and assembling the file
    if (completion_cb) completion_cb(true, "");
    return true;
}

void FileTransfer::set_data_sender(std::function<bool(const std::string& device_id, const std::vector<uint8_t>& data)> sender) {
    pimpl->data_sender = sender;
}

void FileTransfer::receive_packet(const std::string& sender_id, const std::vector<uint8_t>& data) {
    OBEXHeader header;
    std::vector<uint8_t> headers;
    if (!parse_obex_packet(data, header, headers)) return;

    if (header.opcode == static_cast<uint8_t>(OBEXOpcode::CONNECT)) {
        // Handle connect
    } else if (header.opcode == static_cast<uint8_t>(OBEXOpcode::PUT)) {
        // Parse headers
        size_t offset = 0;
        std::string filename;
        uint64_t file_size = 0;
        std::vector<uint8_t> body;

        while (offset < headers.size()) {
            uint8_t hi = headers[offset++];
            uint16_t len = (headers[offset] << 8) | headers[offset + 1];
            offset += 2;

            std::vector<uint8_t> value(headers.begin() + offset, headers.begin() + offset + len - 3);
            offset += len - 3;

            if (hi == static_cast<uint8_t>(OBEXHeaderId::NAME)) {
                filename = pimpl->decode_unicode(value);
            } else if (hi == static_cast<uint8_t>(OBEXHeaderId::LENGTH)) {
                if (value.size() >= 4) {
                    file_size = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];
                }
            } else if (hi == static_cast<uint8_t>(OBEXHeaderId::BODY) || hi == static_cast<uint8_t>(OBEXHeaderId::END_OF_BODY)) {
                body = value;
            }
        }

        if (!filename.empty() && file_size > 0) {
            std::string save_path = "/tmp/" + filename; // TODO: configurable
            std::lock_guard<std::mutex> lock(pimpl->receive_mutex);
            std::string file_id = "recv_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            pimpl->receiving_files[file_id].open(save_path, std::ios::binary);
            pimpl->received_bytes[file_id] = 0;
            pimpl->receiving_paths[file_id] = save_path;

            if (!body.empty()) {
                pimpl->receiving_files[file_id].write(reinterpret_cast<const char*>(body.data()), body.size());
                pimpl->received_bytes[file_id] += body.size();
            }

            // If END_OF_BODY, complete
            if (hi == static_cast<uint8_t>(OBEXHeaderId::END_OF_BODY)) {
                pimpl->receiving_files[file_id].close();
                // TODO: Verify checksum, update DB
            }
        }
    } else if (header.opcode == static_cast<uint8_t>(OBEXOpcode::DISCONNECT)) {
        // Handle disconnect
    }
}