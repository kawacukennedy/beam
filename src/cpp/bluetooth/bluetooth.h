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
    std::vector<std::string> get_discovered_devices();
    std::string get_device_id_from_name(const std::string& name);
    void receive_data(const std::string& device_id, const std::vector<uint8_t>& data);
    void add_discovered_device(const std::string& device_id, void* peripheral);
    void set_tx_characteristic(const std::string& device_id, void* characteristic);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};