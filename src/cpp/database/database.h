#pragma once
#include <string>
#include <vector>
#include <memory>

struct Device {
    std::string id;
    std::string name;
    std::string address;
    bool trusted;
    std::string last_seen;
    std::string fingerprint;
};

struct Message {
    std::string id;
    std::string conversation_id;
    std::string sender_id;
    std::string receiver_id;
    std::vector<uint8_t> content;
    std::string timestamp;
    std::string status;
};

struct File {
    std::string id;
    std::string sender_id;
    std::string receiver_id;
    std::string filename;
    int64_t size;
    std::string checksum;
    std::string path;
    std::string timestamp;
    std::string status;
};

struct FileTransferChunk {
    std::string transfer_id;
    uint64_t offset;
    uint32_t checksum;
    bool sent;
    int retry_count;
};

class Database {
public:
    Database();
    ~Database();

    bool add_device(const Device& device);
    std::vector<Device> get_devices();

    bool add_message(const Message& message);
    std::vector<Message> get_messages(const std::string& conversation_id);

    bool add_file(const File& file);
    bool update_file_status(const std::string& id, const std::string& status);
    std::vector<File> get_files();

    bool add_transfer_chunk(const FileTransferChunk& chunk);
    bool update_chunk_sent(const std::string& transfer_id, uint64_t offset, bool sent);
    std::vector<FileTransferChunk> get_transfer_chunks(const std::string& transfer_id);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};