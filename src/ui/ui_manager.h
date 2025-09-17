#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Enum for UI events
typedef enum {
    UI_EVENT_DEVICE_SELECTED,
    UI_EVENT_SEND_MESSAGE,
    UI_EVENT_ATTACH_FILE,
    UI_EVENT_PAUSE_TRANSFER,
    UI_EVENT_RESUME_TRANSFER,
    UI_EVENT_CANCEL_TRANSFER,
    UI_EVENT_THEME_TOGGLE,
    UI_EVENT_REQUEST_BLUETOOTH_DISCOVERY,
    UI_EVENT_REQUEST_CONNECT_DEVICE,
    UI_EVENT_REQUEST_DISCONNECT_DEVICE,
    UI_EVENT_REQUEST_SEND_FILE,
    // ... other UI events
} UiEventType;

// Structure for UI event data (can be extended for specific events)
typedef struct {
    UiEventType type;
    void* data; // Pointer to event-specific data
} UiEvent;

// Callback function type for UI events
typedef void (*UiEventCallback)(const UiEvent* event);

// Function to initialize the UI manager
void ui_manager_init(void);

// Function to run the UI event loop (platform-specific)
// For GUI, this might be handled by the OS's main loop
void ui_manager_run_loop(void);

// Function to register a callback for UI events
void ui_manager_register_event_callback(UiEventCallback callback);

// Functions to update UI elements (called from C backend to update GUI)
void ui_show_splash_screen(void);
void ui_show_pairing_screen(void);
void ui_show_chat_screen(void);
void ui_show_file_transfer_screen(void);
void ui_show_settings_screen(void);

void ui_add_discovered_device(const char* device_name, const char* device_address, int rssi);
void ui_clear_discovered_devices(void);
void ui_update_device_connection_status(const char* device_address, bool is_connected);

void ui_add_message_bubble(const char* device_address, const char* message, bool is_outgoing);
void ui_clear_chat_messages(const char* device_address);

void ui_update_file_transfer_progress(const char* device_address, const char* filename, double progress);
void ui_add_file_transfer_item(const char* device_address, const char* filename, bool is_sending);
void ui_remove_file_transfer_item(const char* device_address, const char* filename);

void ui_show_alert(const char* title, const char* message);

// Functions to be called by the GUI (from Objective-C/Swift/C++/GTK) to interact with C backend
// These will trigger events that are handled by the registered UiEventCallback
void ui_gui_request_bluetooth_discovery(void);
void ui_gui_request_connect_device(const char* device_address);
void ui_gui_request_disconnect_device(const char* device_address);
void ui_gui_request_send_message(const char* device_address, const char* message);
void ui_gui_request_send_file(const char* device_address, const char* file_path);

#ifdef __cplusplus
}
#endif

#endif // UI_MANAGER_H