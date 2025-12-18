#include "ui_linux.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <json/json.h> // Assuming jsoncpp or similar, but since not specified, use string parsing or add dependency

// For simplicity, use string parsing for JSON, or assume we can use nlohmann/json if available.
// To keep it simple, I'll use string streams.

LinuxUI::LinuxUI() {
    gtk_init();

    window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), "BlueBeam");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    stack = gtk_stack_new();
    gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(stack));

    // Set CSS for fonts and colors
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "window { font-family: Inter, Ubuntu; }"
        ".primary { color: #2563EB; }"
        ".background { background-color: #FFFFFF; }"
        ".surface { background-color: #F1F5F9; }"
        ".text-primary { color: #020617; }"
        ".success { color: #16A34A; }"
        ".error { color: #DC2626; }"
        // Add more as needed
    );
    gtk_style_context_add_provider_for_display(gtk_widget_get_display(window), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    core = core_new();

    create_welcome_screen();
    create_device_discovery_screen();
    create_pairing_screen();
    create_chat_screen();
    create_file_transfer_screen();
    create_settings_screen();

    // Show welcome first
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "welcome");

    g_signal_connect(window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer) { gtk_main_quit(); }), NULL);

    // Poll for state updates every 100ms
    g_timeout_add(100, on_state_update, this);
}

LinuxUI::~LinuxUI() {
    if (core) {
        core_free(core);
    }
}

void LinuxUI::run() {
    gtk_widget_show(window);
    gtk_main();
}

void LinuxUI::create_welcome_screen() {
    welcome_screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_name(welcome_screen, "welcome");

    GtkWidget *title = gtk_label_new("Welcome to BlueBeam");
    gtk_widget_add_css_class(title, "title");
    gtk_box_append(GTK_BOX(welcome_screen), title);

    GtkWidget *desc = gtk_label_new("Secure peer-to-peer messaging over Bluetooth.");
    gtk_box_append(GTK_BOX(welcome_screen), desc);

    GtkWidget *button = gtk_button_new_with_label("Grant Bluetooth Permission and Continue");
    g_signal_connect(button, "clicked", G_CALLBACK(on_welcome_continue), this);
    gtk_box_append(GTK_BOX(welcome_screen), button);

    gtk_stack_add_named(GTK_STACK(stack), welcome_screen, "welcome");
}

void LinuxUI::create_device_discovery_screen() {
    device_discovery_screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_name(device_discovery_screen, "device_discovery");

    GtkWidget *title = gtk_label_new("Device Discovery");
    gtk_box_append(GTK_BOX(device_discovery_screen), title);

    GtkWidget *list = gtk_list_box_new();
    gtk_box_append(GTK_BOX(device_discovery_screen), list);

    GtkWidget *refresh = gtk_button_new_with_label("Refresh");
    g_signal_connect(refresh, "clicked", G_CALLBACK(on_start_discovery), this);
    gtk_box_append(GTK_BOX(device_discovery_screen), refresh);

    gtk_stack_add_named(GTK_STACK(stack), device_discovery_screen, "device_discovery");
}

void LinuxUI::create_pairing_screen() {
    pairing_screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_name(pairing_screen, "pairing");

    GtkWidget *title = gtk_label_new("Pairing");
    gtk_box_append(GTK_BOX(pairing_screen), title);

    GtkWidget *pin_label = gtk_label_new("PIN: 123456"); // Placeholder
    gtk_box_append(GTK_BOX(pairing_screen), pin_label);

    GtkWidget *confirm = gtk_button_new_with_label("Confirm");
    gtk_box_append(GTK_BOX(pairing_screen), confirm);

    gtk_stack_add_named(GTK_STACK(stack), pairing_screen, "pairing");
}

void LinuxUI::set_pairing_pin(const std::string& pin) {
    if (pin_label) {
        std::string text = "PIN: " + pin;
        gtk_label_set_text(pin_label, text.c_str());
    }
}

void LinuxUI::create_chat_screen() {
    chat_screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_name(chat_screen, "chat");

    GtkWidget *messages = gtk_text_view_new();
    gtk_box_append(GTK_BOX(chat_screen), messages);

    GtkWidget *entry = gtk_entry_new();
    gtk_box_append(GTK_BOX(chat_screen), entry);

    GtkWidget *send = gtk_button_new_with_label("Send");
    gtk_box_append(GTK_BOX(chat_screen), send);

    gtk_stack_add_named(GTK_STACK(stack), chat_screen, "chat");
}

void LinuxUI::create_file_transfer_screen() {
    file_transfer_screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_name(file_transfer_screen, "file_transfer");

    GtkWidget *progress = gtk_progress_bar_new();
    gtk_box_append(GTK_BOX(file_transfer_screen), progress);

    GtkWidget *pause = gtk_button_new_with_label("Pause");
    gtk_box_append(GTK_BOX(file_transfer_screen), pause);

    GtkWidget *resume = gtk_button_new_with_label("Resume");
    gtk_box_append(GTK_BOX(file_transfer_screen), resume);

    gtk_stack_add_named(GTK_STACK(stack), file_transfer_screen, "file_transfer");
}

void LinuxUI::create_settings_screen() {
    settings_screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_name(settings_screen, "settings");

    GtkWidget *title = gtk_label_new("Settings");
    gtk_box_append(GTK_BOX(settings_screen), title);

    // Add toggles etc.

    gtk_stack_add_named(GTK_STACK(stack), settings_screen, "settings");
}

gboolean LinuxUI::on_state_update(gpointer user_data) {
    LinuxUI *ui = static_cast<LinuxUI*>(user_data);
    char *update_json = core_recv_update(ui->core);
    if (update_json) {
        std::string json_str(update_json);
        core_free_string(update_json);
        // Simple JSON parsing (assuming format like {"DeviceFound":{"id":"...", "name":"..."}})
        if (json_str.find("DeviceFound") != std::string::npos) {
            // Extract id and name, add to device list
            // For simplicity, just print
            std::cout << "Device found: " << json_str << std::endl;
        } else if (json_str.find("DiscoveryStarted") != std::string::npos) {
            // Update UI
        }
        // Add more cases
    }
    return G_SOURCE_CONTINUE;
}

void LinuxUI::on_welcome_continue(GtkWidget *widget, gpointer data) {
    LinuxUI *ui = static_cast<LinuxUI*>(data);
    gtk_stack_set_visible_child_name(GTK_STACK(ui->stack), "device_discovery");
    // Send event to core
    std::string event = "{\"StartDiscovery\":{}}";
    core_send_event(ui->core, event.c_str());
}

void LinuxUI::on_start_discovery(GtkWidget *widget, gpointer data) {
    LinuxUI *ui = static_cast<LinuxUI*>(data);
    std::string event = "{\"StartDiscovery\":{}}";
    core_send_event(ui->core, event.c_str());
}