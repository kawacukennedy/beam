#include "bluetooth_manager.h"
#include <stdio.h>
#include "bluetooth_callbacks.h"

#ifdef __APPLE__
#include "bluetooth_macos.h"
#endif

#ifdef _WIN32
#include "bluetooth_windows.h"
#endif

#ifdef __linux__
#include "bluetooth_linux.h"
#endif

// Define the global UI callbacks structure
BluetoothUICallbacks g_bluetooth_ui_callbacks; // Definition added here

// Declare dummy functions before they are used in the struct
static void dummy_discoverDevices(void);
static bool dummy_pairDevice(const char* device_address);
static bool dummy_connect(const char* device_address);
static void dummy_disconnect(const char* device_address);
static bool dummy_sendMessage(const char* device_address, const char* message);
static char* dummy_receiveMessage(const char* device_address);
static bool dummy_sendFile(const char* device_address, const char* file_path);
static bool dummy_receiveFile(const char* device_address, const char* destination_path);

// Example dummy implementation for now
static void dummy_discoverDevices(void) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Discovering devices (dummy)...");
    if (g_bluetooth_ui_callbacks.clear_discovered_devices) g_bluetooth_ui_callbacks.clear_discovered_devices();
}

static bool dummy_pairDevice(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Pairing with device (dummy)...");
    return true;
}

static bool dummy_connect(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Connecting to device (dummy)...");
    if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(device_address, true);
    return true;
}

static void dummy_disconnect(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Disconnecting from device (dummy)...");
    if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(device_address, false);
}

static bool dummy_sendMessage(const char* device_address, const char* message) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Sending message (dummy)...");
    if (g_bluetooth_ui_callbacks.add_message_bubble) g_bluetooth_ui_callbacks.add_message_bubble(device_address, message, true);
    return true;
}

static char* dummy_receiveMessage(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Receiving message (dummy)...");
    return NULL; // No message received
}

static bool dummy_sendFile(const char* device_address, const char* file_path) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Sending file (dummy)...");
    if (g_bluetooth_ui_callbacks.add_file_transfer_item) g_bluetooth_ui_callbacks.add_file_transfer_item(device_address, file_path, true);
    return true;
}

static bool dummy_receiveFile(const char* device_address, const char* destination_path) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Receiving file (dummy)...");
    if (g_bluetooth_ui_callbacks.add_file_transfer_item) g_bluetooth_ui_callbacks.add_file_transfer_item(device_address, destination_path, false);
    return true;
}

static IBluetoothManager dummy_manager = {
    .discoverDevices = dummy_discoverDevices,
    .pairDevice = dummy_pairDevice,
    .connect = dummy_connect,
    .disconnect = dummy_disconnect,
    .sendMessage = dummy_sendMessage,
    .receiveMessage = dummy_receiveMessage,
    .sendFile = dummy_sendFile,
    .receiveFile = dummy_receiveFile
};

IBluetoothManager* get_bluetooth_manager(void) {
    #ifdef __APPLE__
    return get_macos_bluetooth_manager();
#elif _WIN32
    return get_windows_bluetooth_manager();
#elif __linux__
    return get_linux_bluetooth_manager();
#else
    return &dummy_manager; // Fallback for unsupported platforms
#endif
}
