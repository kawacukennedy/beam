#include <gtk/gtk.h>
#include "ui.h"
#include <iostream>
#include <vector>
#include <string>
#include "../database/database.h"
#include "../bluetooth/bluetooth.h"
#include "../crypto/crypto.h"
#include "../messaging/messaging.h"
#include "../file_transfer/file_transfer.h"

static GtkWidget* main_window;
static GtkWidget* stack;
static GtkWidget* status_label;
static GtkWidget* device_list;
static GtkWidget* chat_text_view;
static GtkWidget* message_entry;
static GtkWidget* progress_bar;
static GtkWidget* progress_dialog;

static Database* db = nullptr;
static Bluetooth* bt = nullptr;
static Crypto* crypto = nullptr;
static Messaging* messaging = nullptr;
static FileTransfer* ft = nullptr;

static std::string selected_device_id;
static std::string current_conversation_id;

// First time use flow
static void show_splash_screen() {
    GtkWidget* splash = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(splash), "BlueBeam");
    gtk_window_set_default_size(GTK_WINDOW(splash), 400, 300);
    gtk_window_set_modal(GTK_WINDOW(splash), TRUE);
    // Load splash image
    gtk_widget_show(splash);
    g_timeout_add(300, (GSourceFunc)gtk_window_destroy, splash);
}

static void request_bluetooth_permissions() {
    GtkWidget* dialog = gtk_message_dialog_new(NULL,
                                                GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_QUESTION,
                                                GTK_BUTTONS_YES_NO,
                                                "BlueBeam needs Bluetooth access to communicate with nearby devices.\n\nAllow access?");
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_window_destroy(GTK_WINDOW(dialog));
    if (response == GTK_RESPONSE_YES) {
        show_profile_setup();
    } else {
        gtk_main_quit();
    }
}

static void show_profile_setup() {
    GtkWidget* dialog = gtk_dialog_new_with_buttons("Set Up Profile",
                                                     GTK_WINDOW(main_window),
                                                     GTK_DIALOG_MODAL,
                                                     "Continue", GTK_RESPONSE_OK,
                                                     NULL);
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter your name");
    gtk_box_append(GTK_BOX(content), entry);
    gtk_widget_show(dialog);

    gtk_dialog_run(GTK_DIALOG(dialog));
    const char* name = gtk_editable_get_text(GTK_EDITABLE(entry));
    // TODO: Save profile
    gtk_window_destroy(GTK_WINDOW(dialog));
    show_tutorial_overlay();
}

static void show_tutorial_overlay() {
    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_INFO,
                                                GTK_BUTTONS_OK,
                                                "Welcome to BlueBeam!\n\nUse the sidebar to discover devices and start chatting.");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_window_destroy(GTK_WINDOW(dialog));
    first_run = false;
    create_main_ui();
}

static void scan_devices_clicked(GtkButton* button, gpointer user_data) {
    gtk_label_set_text(GTK_LABEL(status_label), "Scanning for Bluetooth devices...");
    bt->scan();
    // For demo, add fake devices
    gtk_list_box_remove_all(GTK_LIST_BOX(device_list));
    std::vector<std::string> devices = {"Device 1 (AA:BB:CC)", "Device 2 (DD:EE:FF)"};
    for (const auto& dev : devices) {
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* label = gtk_label_new(dev.c_str());
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_append(GTK_LIST_BOX(device_list), row);
    }
    gtk_label_set_text(GTK_LABEL(status_label), "Scan complete");
}

static void device_selected(GtkListBox* box, GtkListBoxRow* row, gpointer user_data) {
    if (row) {
        GtkWidget* label = gtk_list_box_row_get_child(row);
        const char* text = gtk_label_get_text(GTK_LABEL(label));
        selected_device_id = text; // Parse device ID

        // Pairing flow
        if (bt->connect(selected_device_id)) {
            GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                        GTK_DIALOG_MODAL,
                                                        GTK_MESSAGE_INFO,
                                                        GTK_BUTTONS_OK,
                                                        "Device paired successfully!");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_window_destroy(GTK_WINDOW(dialog));
            gtk_stack_set_visible_child_name(GTK_STACK(stack), "chat");
            current_conversation_id = "conv_" + selected_device_id;
        } else {
            GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                        GTK_DIALOG_MODAL,
                                                        GTK_MESSAGE_ERROR,
                                                        GTK_BUTTONS_OK,
                                                        "Pairing failed. Try again.");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_window_destroy(GTK_WINDOW(dialog));
        }
    }
}

static void send_message_clicked(GtkButton* button, gpointer user_data) {
    const char* text = gtk_editable_get_text(GTK_EDITABLE(message_entry));
    if (text && *text) {
        // Encrypt and send
        std::vector<uint8_t> content(text, text + strlen(text));
        messaging->send_message("msg_id", current_conversation_id, "sender", selected_device_id, content, MessageStatus::SENT);

        // Add to chat as sent bubble
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_text_view));
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(buffer, &iter);
        gtk_text_buffer_insert(buffer, &iter, (std::string("You: ") + text + "\n").c_str(), -1);
        gtk_editable_set_text(GTK_EDITABLE(message_entry), "");
    }
}

static void send_file_clicked(GtkButton* button, gpointer user_data) {
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Select File",
                                                     GTK_WINDOW(main_window),
                                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                                     "Cancel", GTK_RESPONSE_CANCEL,
                                                     "Open", GTK_RESPONSE_ACCEPT,
                                                     NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_window_destroy(GTK_WINDOW(dialog));

        // Preview modal
        GtkWidget* confirm_dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                           GTK_DIALOG_MODAL,
                                                           GTK_MESSAGE_QUESTION,
                                                           GTK_BUTTONS_YES_NO,
                                                           "Send this file?");
        if (gtk_dialog_run(GTK_DIALOG(confirm_dialog)) == GTK_RESPONSE_YES) {
            gtk_widget_show(progress_dialog);
            ft->send_file(filename, selected_device_id,
                          [](uint64_t sent, uint64_t total) {
                              gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), (double)sent / total);
                          },
                          [](bool success, const std::string& error) {
                              gtk_widget_hide(progress_dialog);
                              if (success) {
                                  GtkWidget* toast = gtk_info_bar_new();
                                  gtk_info_bar_set_message_type(GTK_INFO_BAR(toast), GTK_MESSAGE_INFO);
                                  gtk_info_bar_set_show_close_button(GTK_INFO_BAR(toast), TRUE);
                                  GtkWidget* label = gtk_label_new("File sent successfully!");
                                  gtk_info_bar_add_child(GTK_INFO_BAR(toast), label);
                                  gtk_window_set_child(GTK_WINDOW(progress_dialog), toast);
                                  gtk_widget_show(toast);
                              } else {
                                  GtkWidget* err_dialog = gtk_message_dialog_new(NULL,
                                                                                 GTK_DIALOG_MODAL,
                                                                                 GTK_MESSAGE_ERROR,
                                                                                 GTK_BUTTONS_OK,
                                                                                 "File transfer failed: %s", error.c_str());
                                  gtk_dialog_run(GTK_DIALOG(err_dialog));
                                  gtk_window_destroy(GTK_WINDOW(err_dialog));
                              }
                          });
        }
        gtk_window_destroy(GTK_WINDOW(confirm_dialog));
        g_free(filename);
    } else {
        gtk_window_destroy(GTK_WINDOW(dialog));
    }
}
                      });
        gtk_widget_show(progress_dialog);
        g_free(filename);
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_message_received(const std::string& id, const std::string& conversation_id,
                           const std::string& sender_id, const std::string& receiver_id,
                           const std::vector<uint8_t>& content, MessageStatus status) {
    if (conversation_id == current_conversation_id) {
        std::string msg(content.begin(), content.end());
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_text_view));
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(buffer, &iter);
        gtk_text_buffer_insert(buffer, &iter, (sender_id + ": " + msg + "\n").c_str(), -1);
    }
}

static void on_device_disconnected(const std::string& device_id) {
    if (device_id == selected_device_id) {
        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                    GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_WARNING,
                                                    GTK_BUTTONS_OK,
                                                    "Device disconnected. Attempting to reconnect...");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_window_destroy(GTK_WINDOW(dialog));
        for (int i = 0; i < 3; ++i) {
            if (bt->connect(device_id)) {
                GtkWidget* success_dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                                   GTK_DIALOG_MODAL,
                                                                   GTK_MESSAGE_INFO,
                                                                   GTK_BUTTONS_OK,
                                                                   "Reconnected!");
                gtk_dialog_run(GTK_DIALOG(success_dialog));
                gtk_window_destroy(GTK_WINDOW(success_dialog));
                return;
            }
            sleep(1);
        }
        GtkWidget* fail_dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                        GTK_DIALOG_MODAL,
                                                        GTK_MESSAGE_ERROR,
                                                        GTK_BUTTONS_OK,
                                                        "Failed to reconnect. Device marked offline.");
        gtk_dialog_run(GTK_DIALOG(fail_dialog));
        gtk_window_destroy(GTK_WINDOW(fail_dialog));
    }
}

static void activate(GtkApplication* app, gpointer user_data) {
    // Initialize backends
    db = new Database();
    bt = new Bluetooth();
    crypto = new Crypto();
    messaging = new Messaging(*crypto);
    ft = new FileTransfer();

    // Set up callbacks
    bt->set_receive_callback([](const std::string& device_id, const std::vector<uint8_t>& data) {
        messaging->receive_data(device_id, data);
        ft->receive_packet(device_id, data);
    });
    messaging->set_bluetooth_sender([](const std::string& device_id, const std::vector<uint8_t>& data) {
        return bt->send_data(device_id, data);
    });
    messaging->set_message_callback(on_message_received);
    ft->set_data_sender([](const std::string& device_id, const std::vector<uint8_t>& data) {
        return bt->send_data(device_id, data);
    });

    // First time use flow
    if (first_run) {
        show_splash_screen();
        g_timeout_add(300, (GSourceFunc)request_bluetooth_permissions, NULL);
    } else {
        create_main_ui(app);
    }
}

static void create_main_ui(GtkApplication* app) {
    // Main window
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "BlueBeam");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 800, 600);

    // Header bar
    GtkWidget* header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "BlueBeam");
    gtk_window_set_titlebar(GTK_WINDOW(main_window), header);

    // Sidebar
    GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(sidebar, 200, -1);

    GtkWidget* discovery_btn = gtk_button_new_with_label("Device Discovery");
    g_signal_connect(discovery_btn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "discovery");
    }), nullptr);

    GtkWidget* chat_btn = gtk_button_new_with_label("Chat");
    g_signal_connect(chat_btn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "chat");
    }), nullptr);

    GtkWidget* settings_btn = gtk_button_new_with_label("Settings");
    g_signal_connect(settings_btn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "settings");
    }), nullptr);

    gtk_box_append(GTK_BOX(sidebar), discovery_btn);
    gtk_box_append(GTK_BOX(sidebar), chat_btn);
    gtk_box_append(GTK_BOX(sidebar), settings_btn);

    // Stack for different views
    stack = gtk_stack_new();

    // Discovery page
    GtkWidget* discovery_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(discovery_box, 20);
    gtk_widget_set_margin_end(discovery_box, 20);
    gtk_widget_set_margin_top(discovery_box, 20);
    gtk_widget_set_margin_bottom(discovery_box, 20);

    status_label = gtk_label_new("Welcome to BlueBeam");
    gtk_box_append(GTK_BOX(discovery_box), status_label);

    GtkWidget* scan_btn = gtk_button_new_with_label("Scan Devices");
    g_signal_connect(scan_btn, "clicked", G_CALLBACK(scan_devices_clicked), nullptr);
    gtk_box_append(GTK_BOX(discovery_box), scan_btn);

    device_list = gtk_list_box_new();
    g_signal_connect(device_list, "row-selected", G_CALLBACK(device_selected), nullptr);
    gtk_box_append(GTK_BOX(discovery_box), device_list);

    gtk_stack_add_named(GTK_STACK(stack), discovery_box, "discovery");

    // Chat page
    GtkWidget* chat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(chat_box, 20);
    gtk_widget_set_margin_end(chat_box, 20);
    gtk_widget_set_margin_top(chat_box, 20);
    gtk_widget_set_margin_bottom(chat_box, 20);

    chat_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_text_view), FALSE);
    gtk_widget_set_vexpand(chat_text_view, TRUE);
    gtk_box_append(GTK_BOX(chat_box), chat_text_view);

    GtkWidget* input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    message_entry = gtk_entry_new();
    gtk_widget_set_hexpand(message_entry, TRUE);
    g_signal_connect(message_entry, "activate", G_CALLBACK(send_message_clicked), nullptr);
    gtk_box_append(GTK_BOX(input_box), message_entry);

    GtkWidget* send_btn = gtk_button_new_with_label("Send");
    g_signal_connect(send_btn, "clicked", G_CALLBACK(send_message_clicked), nullptr);
    gtk_box_append(GTK_BOX(input_box), send_btn);

    GtkWidget* file_btn = gtk_button_new_with_label("File");
    g_signal_connect(file_btn, "clicked", G_CALLBACK(send_file_clicked), nullptr);
    gtk_box_append(GTK_BOX(input_box), file_btn);

    gtk_box_append(GTK_BOX(chat_box), input_box);

    gtk_stack_add_named(GTK_STACK(stack), chat_box, "chat");

    // Settings page
    GtkWidget* settings_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(settings_box, 20);
    gtk_widget_set_margin_end(settings_box, 20);
    gtk_widget_set_margin_top(settings_box, 20);
    gtk_widget_set_margin_bottom(settings_box, 20);

    GtkWidget* settings_label = gtk_label_new("Settings - Coming Soon");
    gtk_box_append(GTK_BOX(settings_box), settings_label);

    gtk_stack_add_named(GTK_STACK(stack), settings_box, "settings");

    // Progress dialog
    progress_dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(progress_dialog), "File Transfer");
    gtk_window_set_default_size(GTK_WINDOW(progress_dialog), 300, 100);
    gtk_window_set_modal(GTK_WINDOW(progress_dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(progress_dialog), GTK_WINDOW(main_window));

    GtkWidget* progress_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(progress_box, 20);
    gtk_widget_set_margin_end(progress_box, 20);
    gtk_widget_set_margin_top(progress_box, 20);
    gtk_widget_set_margin_bottom(progress_box, 20);

    progress_bar = gtk_progress_bar_new();
    gtk_box_append(GTK_BOX(progress_box), progress_bar);

    gtk_window_set_child(GTK_WINDOW(progress_dialog), progress_box);

    // Main layout
    GtkWidget* paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned), sidebar);
    gtk_paned_set_end_child(GTK_PANED(paned), stack);
    gtk_paned_set_position(GTK_PANED(paned), 200);

    gtk_window_set_child(GTK_WINDOW(main_window), paned);
    gtk_window_present(GTK_WINDOW(main_window));
}

class UI::Impl {
public:
    GtkApplication* app;

    Impl() {
        app = gtk_application_new("com.bluebeam.app", G_APPLICATION_DEFAULT_FLAGS);
        g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    }

    ~Impl() {
        delete db;
        delete bt;
        delete crypto;
        delete messaging;
        delete ft;
        g_object_unref(app);
    }

    void run() {
        g_application_run(G_APPLICATION(app), 0, nullptr);
    }
};

UI::UI() : pimpl(std::make_unique<Impl>()) {}
UI::~UI() = default;

void UI::run() {
    pimpl->run();
}