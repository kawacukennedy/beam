#ifndef BLUETOOTH_CALLBACKS_H
#define BLUETOOTH_CALLBACKS_H

#include <stdbool.h>

// Define a structure to hold function pointers for UI updates
typedef struct {
    void (*add_discovered_device)(const char* device_name, const char* device_address, int rssi);
    void (*clear_discovered_devices)(void);
    void (*update_device_connection_status)(const char* device_address, bool is_connected);
    void (*add_message_bubble)(const char* device_address, const char* message, bool is_outgoing);
    void (*clear_chat_messages)(const char* device_address);
    void (*update_file_transfer_progress)(const char* device_address, const char* filename, double progress);
    void (*add_file_transfer_item)(const char* device_address, const char* filename, bool is_sending);
    void (*remove_file_transfer_item)(const char* device_address, const char* filename);
    void (*show_alert)(const char* title, const char* message);
} BluetoothUICallbacks;

// Global instance of the callbacks, to be set by the main application
extern BluetoothUICallbacks g_bluetooth_ui_callbacks;

#endif // BLUETOOTH_CALLBACKS_H
