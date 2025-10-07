#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

class Bluetooth {
public:
    Bluetooth();
    ~Bluetooth();

    void scan();
    bool connect(const std::string& device_id);
    bool send_data(const std::string& device_id, const std::vector<uint8_t>& data);
    void set_receive_callback(std::function<void(const std::string& device_id, const std::vector<uint8_t>& data)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};