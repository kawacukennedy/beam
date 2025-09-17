#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>

#include "ui/ui_manager.h"
#include "bluetooth/bluetooth_manager.h"
#include "bluetooth/bluetooth_callbacks.h"

// Global pointer to the Bluetooth Manager
static IBluetoothManager* bluetooth_manager = NULL;

// Global instance of the UI callbacks for Bluetooth module
BluetoothUICallbacks g_bluetooth_ui_callbacks;

// Callback function to handle UI events from the GUI
void handle_ui_event(const UiEvent* event) {
    qDebug() << "Received UI Event:" << event->type;

    switch (event->type) {
        case UI_EVENT_REQUEST_BLUETOOTH_DISCOVERY: {
            if (bluetooth_manager) {
                bluetooth_manager->discoverDevices();
            }
            break;
        }
        case UI_EVENT_REQUEST_CONNECT_DEVICE: {
            if (bluetooth_manager && event->data) {
                const char* device_address = (const char*)event->data;
                bluetooth_manager->connect(device_address);
                free((void*)device_address); // Free memory allocated in ui_manager.cpp
            }
            break;
        }
        case UI_EVENT_REQUEST_DISCONNECT_DEVICE: {
            if (bluetooth_manager && event->data) {
                const char* device_address = (const char*)event->data;
                bluetooth_manager->disconnect(device_address);
                free((void*)device_address);
            }
            break;
        }
        case UI_EVENT_SEND_MESSAGE: {
            if (bluetooth_manager && event->data) {
                typedef struct { char* addr; char* msg; } MessageData;
                MessageData* data = (MessageData*)event->data;
                bluetooth_manager->sendMessage(data->addr, data->msg);
                free(data->addr);
                free(data->msg);
                free(data);
            }
            break;
        }
        case UI_EVENT_REQUEST_SEND_FILE: {
            if (bluetooth_manager && event->data) {
                typedef struct { char* addr; char* path; } FileData;
                FileData* data = (FileData*)event->data;
                bluetooth_manager->sendFile(data->addr, data->path);
                free(data->addr);
                free(data->path);
                free(data);
            }
            break;
        }
        // Handle other UI events as needed
        default:
            break;
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Initialize the UI Manager
    ui_manager_init();

    // Register the UI event callback
    ui_manager_register_event_callback(handle_ui_event);

    // Populate the global Bluetooth UI callbacks
    g_bluetooth_ui_callbacks.add_discovered_device = ui_add_discovered_device;
    g_bluetooth_ui_callbacks.clear_discovered_devices = ui_clear_discovered_devices;
    g_bluetooth_ui_callbacks.update_device_connection_status = ui_update_device_connection_status;
    g_bluetooth_ui_callbacks.add_message_bubble = ui_add_message_bubble;
    g_bluetooth_ui_callbacks.clear_chat_messages = ui_clear_chat_messages;
    g_bluetooth_ui_callbacks.update_file_transfer_progress = ui_update_file_transfer_progress;
    g_bluetooth_ui_callbacks.add_file_transfer_item = ui_add_file_transfer_item;
    g_bluetooth_ui_callbacks.remove_file_transfer_item = ui_remove_file_transfer_item;
    g_bluetooth_ui_callbacks.show_alert = ui_show_alert;

    // Get the platform-specific Bluetooth manager
    bluetooth_manager = get_bluetooth_manager();

    // Run the Qt application event loop
    return app.exec();
}
