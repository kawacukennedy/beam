#include "ui/ui.h"
#include <gtk/gtk.h>
#include <iostream>
#include <memory>
#include <chrono>
#include <string>
#include "database/database.h"
#include "crypto/crypto.h"
#include "bluetooth/bluetooth.h"
#include "messaging/messaging.h"
#include "file_transfer/file_transfer.h"
#include "settings/settings.h"

class UI::Impl {
public:
    std::unique_ptr<Database> db;
    std::unique_ptr<Bluetooth> bt;
    std::unique_ptr<Crypto> crypto;
    std::unique_ptr<Messaging> messaging;
    std::unique_ptr<FileTransfer> ft;
    std::unique_ptr<Settings> settings;
    std::string current_device_id;

    Impl() {
        gtk_init(NULL, NULL);

        showOnboarding();

        db = std::make_unique<Database>();
        bt = std::make_unique<Bluetooth>();
        crypto = std::make_unique<Crypto>();
        messaging = std::make_unique<Messaging>(*crypto);
        ft = std::make_unique<FileTransfer>(*crypto);
        settings = std::make_unique<Settings>();
        gtk_init(NULL, NULL);

        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window), "BlueBeam");
        gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
        g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_add(GTK_CONTAINER(window), vbox);

        // Buttons
        GtkWidget* hbox_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(vbox), hbox_buttons, FALSE, FALSE, 0);

        scan_button = gtk_button_new_with_label("Scan Devices");
        g_signal_connect(scan_button, "clicked", G_CALLBACK(on_scan_clicked), this);
        gtk_box_pack_start(GTK_BOX(hbox_buttons), scan_button, FALSE, FALSE, 0);

        connect_button = gtk_button_new_with_label("Connect");
        g_signal_connect(connect_button, "clicked", G_CALLBACK(on_connect_clicked), this);
        gtk_box_pack_start(GTK_BOX(hbox_buttons), connect_button, FALSE, FALSE, 0);

        GtkWidget* settings_button = gtk_button_new_with_label("Settings");
        g_signal_connect(settings_button, "clicked", G_CALLBACK(on_settings_clicked), this);
        gtk_box_pack_start(GTK_BOX(hbox_buttons), settings_button, FALSE, FALSE, 0);

        // Device list and chat
        GtkWidget* hbox_main = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(vbox), hbox_main, TRUE, TRUE, 0);

        device_list = gtk_list_box_new();
        gtk_box_pack_start(GTK_BOX(hbox_main), device_list, FALSE, FALSE, 0);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(hbox_main), scrolled_window, TRUE, TRUE, 0);

        chat_view = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_view), FALSE);
        gtk_container_add(GTK_CONTAINER(scrolled_window), chat_view);

        // Message input
        GtkWidget* hbox_input = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(vbox), hbox_input, FALSE, FALSE, 0);

        message_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(hbox_input), message_entry, TRUE, TRUE, 0);

        send_button = gtk_button_new_with_label("Send");
        g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_clicked), this);
        gtk_box_pack_start(GTK_BOX(hbox_input), send_button, FALSE, FALSE, 0);

        file_button = gtk_button_new_with_label("Send File");
        g_signal_connect(file_button, "clicked", G_CALLBACK(on_file_clicked), this);
        gtk_box_pack_start(GTK_BOX(hbox_input), file_button, FALSE, FALSE, 0);

        progress_bar = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 0);

        // Set up callbacks
        bt->set_receive_callback([this](const std::string& device_id, const std::vector<uint8_t>& data) {
            messaging->receive_data(device_id, data);
            ft->receive_packet(device_id, data);
        });
        messaging->set_bluetooth_sender([this](const std::string& device_id, const std::vector<uint8_t>& data) {
            return bt->send_data(device_id, data);
        });
        messaging->set_message_callback([this](const std::string& id, const std::string& conversation_id,
                                              const std::string& sender_id, const std::string& receiver_id,
                                              const std::vector<uint8_t>& content, MessageStatus status) {
            if (conversation_id == current_device_id) {
                std::string msg(content.begin(), content.end());
                std::string display = sender_id + ": " + msg + "\n";
                GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_view));
                GtkTextIter end;
                gtk_text_buffer_get_end_iter(buffer, &end);
                gtk_text_buffer_insert(buffer, &end, display.c_str(), -1);

                // Save to database
                Message db_msg{id, conversation_id, sender_id, receiver_id, content, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()), status == MessageStatus::SENT ? "sent" : "received"};
                db->add_message(db_msg);
            }
        });
        ft->set_data_sender([this](const std::string& device_id, const std::vector<uint8_t>& data) {
            return bt->send_data(device_id, data);
        });
        ft->set_incoming_file_callback([this](const std::string& filename, uint64_t size, auto response) {
            // Simple accept for demo
            response(true, "/tmp/" + filename);
        });
    }

    void showOnboarding() {
        GtkWidget* dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                   "Welcome to BlueBeam!\n\n1. Click Scan Devices to find nearby Bluetooth devices.\n2. Select a device and click Connect.\n3. Start chatting or sending files.\n\nEnjoy secure Bluetooth communication!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    void showSettingsDialog() {
        GtkWidget* dialog = gtk_dialog_new_with_buttons("Settings", GTK_WINDOW(window), GTK_DIALOG_MODAL,
                                                        "Save", GTK_RESPONSE_ACCEPT, "Cancel", GTK_RESPONSE_CANCEL, NULL);

        GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_add(GTK_CONTAINER(content), vbox);

        GtkWidget* name_label = gtk_label_new("User Name:");
        gtk_box_pack_start(GTK_BOX(vbox), name_label, FALSE, FALSE, 0);

        name_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(vbox), name_entry, FALSE, FALSE, 0);

        // Load current settings
        std::string name = settings->get_user_name();
        gtk_entry_set_text(GTK_ENTRY(name_entry), name.c_str());

        gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            const char* name = gtk_entry_get_text(GTK_ENTRY(name_entry));
            settings->set_user_name(name);
            settings->save();
        }

        gtk_widget_destroy(dialog);
    }

    void loadMessageHistory() {
        if (current_device_id.empty()) return;

        auto messages = db->get_messages(current_device_id);
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_view));
        gtk_text_buffer_set_text(buffer, "", -1);
        for (const auto& msg : messages) {
            std::string display = msg.sender_id + ": " + std::string(msg.content.begin(), msg.content.end()) + "\n";
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(buffer, &end);
            gtk_text_buffer_insert(buffer, &end, display.c_str(), -1);
        }
    }

    void run() {
        gtk_widget_show_all(window);
        gtk_main();
    }

private:
    static void on_scan_clicked(GtkWidget* widget, gpointer data) {
        UI::Impl* impl = static_cast<UI::Impl*>(data);
        impl->bt->scan();
        // Clear list
        GList* children = gtk_container_get_children(GTK_CONTAINER(impl->device_list));
        for (GList* iter = children; iter; iter = iter->next) {
            gtk_widget_destroy(GTK_WIDGET(iter->data));
        }
        g_list_free(children);
        // Add devices
        auto devices = impl->bt->get_discovered_devices();
        for (const auto& dev : devices) {
            GtkWidget* row = gtk_label_new(dev.c_str());
            gtk_list_box_insert(GTK_LIST_BOX(impl->device_list), row, -1);
        }
        gtk_widget_show_all(impl->device_list);
    }

    static void on_connect_clicked(GtkWidget* widget, gpointer data) {
        UI::Impl* impl = static_cast<UI::Impl*>(data);
        GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(impl->device_list));
        if (row) {
            GtkWidget* child = gtk_bin_get_child(GTK_BIN(row));
            const char* text = gtk_label_get_text(GTK_LABEL(child));
            std::string name = text;
            std::string device_id = impl->bt->get_device_id_from_name(name);
                if (impl->bt->connect(device_id)) {
                    impl->current_device_id = device_id;
                    impl->loadMessageHistory();
                }
        }
    }

    static void on_send_clicked(GtkWidget* widget, gpointer data) {
        UI::Impl* impl = static_cast<UI::Impl*>(data);
        const char* text = gtk_entry_get_text(GTK_ENTRY(impl->message_entry));
        if (strlen(text) > 0 && !impl->current_device_id.empty()) {
            std::string msg = text;
            std::vector<uint8_t> content(msg.begin(), msg.end());
            std::string conversation_id = impl->current_device_id;
            std::string id = "msg_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            impl->messaging->send_message(id, conversation_id, "self", impl->current_device_id, content, MessageStatus::SENT);
            gtk_entry_set_text(GTK_ENTRY(impl->message_entry), "");

            // Append to chat
            GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(impl->chat_view));
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(buffer, &end);
            std::string display = "You: " + msg + "\n";
            gtk_text_buffer_insert(buffer, &end, display.c_str(), -1);
        }
    }

    static void on_file_clicked(GtkWidget* widget, gpointer data) {
        UI::Impl* impl = static_cast<UI::Impl*>(data);
        GtkWidget* dialog = gtk_file_chooser_dialog_new("Select File",
                                                        GTK_WINDOW(impl->window),
                                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                                        "Cancel", GTK_RESPONSE_CANCEL,
                                                        "Open", GTK_RESPONSE_ACCEPT,
                                                        NULL);
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            std::string path = filename;
            g_free(filename);
            if (!impl->current_device_id.empty()) {
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(impl->progress_bar), 0.0);
                impl->ft->send_file(path, impl->current_device_id,
                    [impl](uint64_t sent, uint64_t total) {
                        double fraction = total > 0 ? (double)sent / total : 0.0;
                        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(impl->progress_bar), fraction);
                    },
                    [impl](bool success, const std::string& error) {
                        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(impl->progress_bar), 1.0);
                        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(impl->window), GTK_DIALOG_MODAL,
                                                                   success ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR,
                                                                   GTK_BUTTONS_OK,
                                                                   success ? "File sent successfully!" : ("File send failed: " + error).c_str());
                        gtk_dialog_run(GTK_DIALOG(dialog));
                        gtk_widget_destroy(dialog);
                    });
            }
        }
        gtk_widget_destroy(dialog);
    }

    static void on_settings_clicked(GtkWidget* widget, gpointer data) {
        UI::Impl* impl = static_cast<UI::Impl*>(data);
        impl->showSettingsDialog();
    }

    GtkWidget* window;
    GtkWidget* scan_button;
    GtkWidget* connect_button;
    GtkWidget* device_list;
    GtkWidget* scrolled_window;
    GtkWidget* chat_view;
    GtkWidget* message_entry;
    GtkWidget* send_button;
    GtkWidget* file_button;
    GtkWidget* name_entry;
    GtkWidget* progress_bar;
};

UI::UI() : pimpl(std::make_unique<Impl>()) {}
UI::~UI() = default;

void UI::run() {
    pimpl->run();
}