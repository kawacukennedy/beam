#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unordered_map>
#include <functional>
#include <chrono>

#pragma pack(push, 1)
struct MessageFrame {
    uint32_t length;
    uint64_t id[2];
    uint64_t conversation_id[2];
    uint64_t sender_id[2];
    uint64_t receiver_id[2];
    uint64_t timestamp_unix_ms;
    uint32_t content_size;
    // uint8_t content[content_size]; // variable
    uint32_t crc32;
};

struct AckFrame {
    uint32_t magic; // 'MACK'
    uint32_t message_id_len;
    // Followed by message_id
};
#pragma pack(pop)

enum class MessageStatus {
    SENT = 0,
    DELIVERED = 1,
    READ = 2
};

struct PendingMessage {
    uint64_t id[2];
    std::vector<uint8_t> data;
    uint64_t receiver_id[2];
    int retry_count;
    std::chrono::steady_clock::time_point next_retry;
};

class Crypto;

class Messaging {
public:
    Messaging(Crypto& crypto);
    ~Messaging();

    using MessageCallback = std::function<void(const uint64_t id[2], const uint64_t conversation_id[2],
                                               const uint64_t sender_id[2], const uint64_t receiver_id[2],
                                               const std::vector<uint8_t>& content)>;

    void set_message_callback(MessageCallback callback);
    void set_bluetooth_sender(std::function<bool(const std::string& device_id, const std::vector<uint8_t>& data)> sender);

    bool send_message(const uint64_t id[2], const uint64_t conversation_id[2],
                      const uint64_t sender_id[2], const uint64_t receiver_id[2],
                      const std::vector<uint8_t>& content);

    void receive_data(const std::string& sender_id, const std::vector<uint8_t>& data);

    std::vector<uint8_t> pack_message(const uint64_t id[2], const uint64_t conversation_id[2],
                                      const uint64_t sender_id[2], const uint64_t receiver_id[2],
                                      const std::vector<uint8_t>& content, uint64_t timestamp_unix_ms);
    bool unpack_message(const std::vector<uint8_t>& data, uint64_t id[2], uint64_t conversation_id[2],
                        uint64_t sender_id[2], uint64_t receiver_id[2],
                        std::vector<uint8_t>& content, uint64_t& timestamp_unix_ms);

    std::vector<uint8_t> pack_ack(const uint64_t message_id[2]);
    bool unpack_ack(const std::vector<uint8_t>& data, uint64_t message_id[2]);

private:
    static constexpr uint32_t MAGIC = 0x4D41434B; // 'MACK'
    static constexpr size_t MAX_MESSAGE_SIZE = 65536;

    class Impl;
    std::unique_ptr<Impl> pimpl;
};