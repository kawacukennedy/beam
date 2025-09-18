#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <stdbool.h>

// Define IBluetoothManager struct with function pointers
typedef struct IBluetoothManager {
    void (*discoverDevices)(void);
    bool (*pairDevice)(const char* device_address);
    bool (*connect)(const char* device_address);
    void (*disconnect)(const char* device_address);
    bool (*sendMessage)(const char* device_address, const char* message);
    char* (*receiveMessage)(const char* device_address);
    bool (*sendFile)(const char* device_address, const char* file_path);
    bool (*receiveFile)(const char* device_address, const char* destination_path);
} IBluetoothManager;

// Function to get the platform-specific Bluetooth manager implementation
IBluetoothManager* get_bluetooth_manager(void);

#endif // BLUETOOTH_MANAGER_H