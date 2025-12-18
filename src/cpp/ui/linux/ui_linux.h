#pragma once

#include "../ui.h"
#include <gtk/gtk.h>

extern "C" {
    typedef void* CoreManagerPtr;
    CoreManagerPtr core_new();
    void core_free(CoreManagerPtr);
    void core_send_event(CoreManagerPtr, const char* event_json);
    char* core_recv_update(CoreManagerPtr);
    void core_free_string(char* str);
}

class LinuxUI : public UI {
public:
    LinuxUI();
    ~LinuxUI();
    void run() override;

private:
    GtkWidget *window;
    GtkWidget *stack;
    CoreManagerPtr core;

    // Screens
    GtkWidget *welcome_screen;
    GtkWidget *device_discovery_screen;
    GtkWidget *pairing_screen;
    GtkWidget *chat_screen;
    GtkWidget *file_transfer_screen;
    GtkWidget *settings_screen;

    // Methods to create screens
    void create_welcome_screen();
    void create_device_discovery_screen();
    void create_pairing_screen();
    void create_chat_screen();
    void create_file_transfer_screen();
    void create_settings_screen();

    // State update handler
    static gboolean on_state_update(gpointer user_data);

    // Event handlers
    static void on_welcome_continue(GtkWidget *widget, gpointer data);
    static void on_start_discovery(GtkWidget *widget, gpointer data);
    // etc.
};