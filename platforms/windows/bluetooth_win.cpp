#include "bluetooth.h"
#include <windows.h>
#include <bluetoothapis.h>
#include <ws2bth.h>
#include <thread>
#include <unordered_map>

class Bluetooth::Impl {
public:
    std::function<void(const std::string& device_id, const std::vector<uint8_t>& data)> receive_callback;
    std::unordered_map<std::string, HANDLE> connections;
    std::unordered_map<std::string, BLUETOOTH_DEVICE_INFO> discovered_devices;
    std::thread receive_thread;
    bool running = true;

    Impl() {
        receive_thread = std::thread(&Impl::receive_worker, this);
    }
    ~Impl() {
        running = false;
        if (receive_thread.joinable()) receive_thread.join();
        for (auto& conn : connections) {
            CloseHandle(conn.second);
        }
    }

    void receive_worker() {
        while (running) {
            for (auto& conn : connections) {
                DWORD bytes_read;
                char buffer[1024];
                if (ReadFile(conn.second, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
                    std::vector<uint8_t> data(buffer, buffer + bytes_read);
                    if (receive_callback) {
                        receive_callback(conn.first, data);
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

Bluetooth::Bluetooth() : pimpl(std::make_unique<Impl>()) {}

Bluetooth::~Bluetooth() = default;

void Bluetooth::scan() {
    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = { sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS) };
    searchParams.fReturnAuthenticated = TRUE;
    searchParams.fReturnRemembered = TRUE;
    searchParams.fReturnUnknown = TRUE;
    searchParams.fReturnConnected = TRUE;
    searchParams.fIssueInquiry = TRUE;
    searchParams.cTimeoutMultiplier = 2;

    BLUETOOTH_DEVICE_INFO deviceInfo = { sizeof(BLUETOOTH_DEVICE_INFO) };
    HBLUETOOTH_DEVICE_FIND hFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
    if (hFind) {
        do {
            char address[18];
            sprintf_s(address, "%02X:%02X:%02X:%02X:%02X:%02X",
                     deviceInfo.Address.rgBytes[5], deviceInfo.Address.rgBytes[4],
                     deviceInfo.Address.rgBytes[3], deviceInfo.Address.rgBytes[2],
                     deviceInfo.Address.rgBytes[1], deviceInfo.Address.rgBytes[0]);
            std::string device_id = address;
            pimpl->discovered_devices[device_id] = deviceInfo;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } while (BluetoothFindNextDevice(hFind, &deviceInfo));
        BluetoothFindDeviceClose(hFind);
    }
}

bool Bluetooth::connect(const std::string& device_id) {
    auto it = pimpl->discovered_devices.find(device_id);
    if (it != pimpl->discovered_devices.end()) {
        SOCKET s = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
        if (s == INVALID_SOCKET) return false;

        SOCKADDR_BTH addr = {0};
        addr.addressFamily = AF_BTH;
        addr.btAddr = it->second.Address.ullLong;
        addr.serviceClassId = RFCOMM_PROTOCOL_UUID;
        addr.port = BT_PORT_ANY;

        if (connect(s, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(s);
            return false;
        }

        pimpl->connections[device_id] = (HANDLE)s;
        return true;
    }
    return false;
}

bool Bluetooth::send_data(const std::string& device_id, const std::vector<uint8_t>& data) {
    auto it = pimpl->connections.find(device_id);
    if (it != pimpl->connections.end()) {
        DWORD bytes_written;
        if (WriteFile(it->second, data.data(), data.size(), &bytes_written, NULL)) {
            return bytes_written == data.size();
        }
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