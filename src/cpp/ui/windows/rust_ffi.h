#pragma once

extern "C" {
    void request_bluetooth_permission();
    void start_device_discovery();
    void get_discovered_devices(void* devices, int* count); // Adjust types
    void confirm_pairing();
    void send_message(const char* msg);
    void pause_transfer();
    void resume_transfer();
    void set_dark_mode(bool enabled);
    void set_notifications(bool enabled);
}