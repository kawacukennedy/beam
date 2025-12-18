#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_set>

class Crypto;
class Database;

#pragma pack(push, 1)
struct OBEXHeader {
    uint8_t opcode; // OBEX opcodes: 0x80 Connect, 0x02 Put, 0x03 Get, 0x81 Disconnect, etc.
    uint16_t length; // Total packet length
    // Followed by headers
};

struct OBEXHeaderItem {
    uint8_t hi; // Header identifier
    // Followed by length and value
};
#pragma pack(pop)

enum class OBEXOpcode {
    CONNECT = 0x80,
    DISCONNECT = 0x81,
    PUT = 0x02,
    GET = 0x03,
    SETPATH = 0x85,
    ABORT = 0xFF
};

enum class OBEXHeaderId {
    COUNT = 0xC0,
    NAME = 0x01,
    TYPE = 0x42,
    LENGTH = 0xC3,
    TIME = 0x44,
    DESCRIPTION = 0x05,
    TARGET = 0x46,
    HTTP = 0x47,
    WHO = 0x4A,
    CONNECTION = 0xCB,
    APPLICATION = 0x4C,
    AUTH_CHALLENGE = 0x4D,
    AUTH_RESPONSE = 0x4E,
    OBJECT_CLASS = 0x4F,
    BODY = 0x48,
    END_OF_BODY = 0x49
};

struct FileChunk {
    std::string file_id;
    uint64_t offset;
    std::vector<uint8_t> data;
    uint32_t checksum;
    int retry_count;
};

struct TransferSession {
    std::string file_id;
    std::string filename;
    uint64_t file_size;
    std::string checksum;
    std::string receiver_id;
    uint64_t bytes_sent;
    std::queue<FileChunk> chunk_queue;
    std::unordered_set<uint64_t> sent_offsets;
    bool active;
    bool paused;
    ProgressCallback progress_cb;
    CompletionCallback completion_cb;
};

class FileTransfer {
public:
    FileTransfer(Crypto& crypto, Database& database);
    ~FileTransfer();

    using ProgressCallback = std::function<void(uint64_t sent, uint64_t total)>;
    using CompletionCallback = std::function<void(bool success, const std::string& error)>;
    using IncomingFileCallback = std::function<void(const std::string& filename, uint64_t size, std::function<void(bool accept, const std::string& save_path)> response)>;

    bool send_file(const std::string& path, const std::string& receiver_id,
                    ProgressCallback progress_cb = nullptr,
                    CompletionCallback completion_cb = nullptr);

    bool resume_send(const std::string& file_id);

    bool pause_send(const std::string& file_id);

    bool receive_file(const std::string& file_id, const std::string& filename, uint64_t size,
                        const std::string& checksum, const std::string& save_path,
                        ProgressCallback progress_cb = nullptr,
                        CompletionCallback completion_cb = nullptr);

    void set_incoming_file_callback(IncomingFileCallback callback);

    void set_data_sender(std::function<bool(const std::string& device_id, const std::vector<uint8_t>& data)> sender);
    void receive_packet(const std::string& sender_id, const std::vector<uint8_t>& data);

    // OBEX protocol methods
    std::vector<uint8_t> create_connect_packet();
    std::vector<uint8_t> create_put_packet(const std::string& filename, uint64_t file_size, const std::vector<uint8_t>& data, bool is_final, const std::string& session_id);
    std::vector<uint8_t> create_chunk_packet(const FileChunk& chunk, bool is_final, const std::string& session_id);
    std::vector<uint8_t> create_disconnect_packet();
    std::vector<uint8_t> create_abort_packet();

    bool parse_obex_packet(const std::vector<uint8_t>& data, OBEXHeader& header, std::vector<uint8_t>& headers);

private:
    static constexpr uint64_t CHUNK_SIZE = 131072; // 128KB
    static constexpr uint64_t MAX_FILE_SIZE = 4294967296; // 4GB
    static constexpr int MAX_RETRIES = 3;
    static constexpr int BACKOFF_MS = 1000;

    class Impl;
    std::unique_ptr<Impl> pimpl;
};