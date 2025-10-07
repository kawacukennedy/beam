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

class Database {
public:
    Database();
    ~Database();

    bool add_device(const Device& device);
    std::vector<Device> get_devices();
    // Add other methods

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};