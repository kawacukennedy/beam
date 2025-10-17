#include "bluetooth.h"
#include <dbus/dbus.h>
#include <thread>
#include <atomic>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <unistd.h>
#include <sys/select.h>
#include <unordered_map>

class Bluetooth::Impl {
public:
    DBusConnection* conn;
    std::function<void(const std::string& device_id, const std::vector<uint8_t>& data)> receive_callback;
    std::unordered_map<std::string, int> connections;
    std::unordered_map<std::string, std::string> discovered_devices;
    std::thread receive_thread;
    std::atomic<bool> running{true};

    Impl() : conn(nullptr) {
        DBusError error;
        dbus_error_init(&error);
        conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
        if (dbus_error_is_set(&error)) {
            dbus_error_free(&error);
        } else {
            receive_thread = std::thread(&Impl::receive_worker, this);
        }
    }

    ~Impl() {
        running = false;
        if (receive_thread.joinable()) receive_thread.join();
        for (auto& conn : connections) {
            close(conn.second);
        }
        if (conn) dbus_connection_unref(conn);
    }

    void receive_worker() {
        while (running) {
            for (auto& conn : connections) {
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(conn.second, &readfds);
                struct timeval tv = {0, 100000}; // 100ms timeout
                if (select(conn.second + 1, &readfds, NULL, NULL, &tv) > 0) {
                    char buffer[1024];
                    ssize_t bytes_read = read(conn.second, buffer, sizeof(buffer));
                    if (bytes_read > 0) {
                        std::vector<uint8_t> data(buffer, buffer + bytes_read);
                        if (receive_callback) {
                            receive_callback(conn.first, data);
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

Bluetooth::Bluetooth() : pimpl(std::make_unique<Impl>()) {}

Bluetooth::~Bluetooth() = default;

void Bluetooth::scan() {
    if (pimpl->conn) {
        DBusMessage* msg = dbus_message_new_method_call("org.bluez", "/org/bluez/hci0", "org.bluez.Adapter1", "StartDiscovery");
        dbus_connection_send(pimpl->conn, msg, nullptr);
        dbus_message_unref(msg);
    }
}

bool Bluetooth::connect(const std::string& device_id) {
    int sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (sock < 0) return false;

    struct sockaddr_rc addr = {0};
    addr.rc_family = AF_BLUETOOTH;
    str2ba(device_id.c_str(), &addr.rc_bdaddr);
    addr.rc_channel = 1;

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return false;
    }

    pimpl->connections[device_id] = sock;
    return true;
}

bool Bluetooth::send_data(const std::string& device_id, const std::vector<uint8_t>& data) {
    auto it = pimpl->connections.find(device_id);
    if (it != pimpl->connections.end()) {
        ssize_t bytes_written = write(it->second, data.data(), data.size());
        return bytes_written == (ssize_t)data.size();
    }
    return false;
}

void Bluetooth::set_receive_callback(std::function<void(const std::string& device_id, const std::vector<uint8_t>& data)> callback) {
    pimpl->receive_callback = callback;
}

std::vector<std::string> Bluetooth::get_discovered_devices() {
    std::vector<std::string> devices;
    for (const auto& pair : pimpl->discovered_devices) {
        devices.push_back(pair.first);
    }
    return devices;
}