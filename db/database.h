#pragma once
#include <string>
#include <vector>
#include <memory>

struct Device {
    std::string id;
    std::string name;
    std::string address;
    bool trusted;
    int64_t last_seen;
    std::string fingerprint;
};

struct Message {
    std::string id;
    std::string conversation_id;
    std::string sender_id;
    std::string receiver_id;
    std::vector<uint8_t> content;
    int64_t timestamp;
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
    int64_t timestamp;
    std::string status;
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

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};