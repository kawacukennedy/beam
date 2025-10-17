#include "ui/ui.h"
#include <gtk/gtk.h>
#include <iostream>
#include <memory>
#include <chrono>
#include <string>
#include <vector>
#include <glib.h>
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
    std::string current_conversation_id;

    Impl() {
        gtk_init(NULL, NULL);

        showOnboarding();

        db = std::make_unique<Database>();
        bt = std::make_unique<Bluetooth>();
        crypto = std::make_unique<Crypto>();
        messaging = std::make_unique<Messaging>(*crypto);
        ft = std::make_unique<FileTransfer>(*crypto);
        settings = std::make_unique<Settings>();

        create_main_window();

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
            if (conversation_id == current_conversation_id) {
                update_message_list();
            }
            // Save to database
            Message db_msg{id, conversation_id, sender_id, receiver_id, content, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()), status == MessageStatus::SENT ? "sent" : "received"};
            db->add_message(db_msg);
        });
        ft->set_data_sender([this](const std::string& device_id, const std::vector<uint8_t>& data) {
            return bt->send_data(device_id, data);
        });
        ft->set_incoming_file_callback([this](const std::string& filename, uint64_t size, auto response) {
            show_incoming_file_dialog(filename, size, response);
        });
    }

    void create_main_window() {
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window), "BlueBeam");
        gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);
        g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

        // Main horizontal box
        GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_container_add(GTK_CONTAINER(window), hbox);

        // Left sidebar
        create_sidebar();
        gtk_box_pack_start(GTK_BOX(hbox), sidebar, FALSE, FALSE, 0);

        // Right main area
        create_main_area();
        gtk_box_pack_start(GTK_BOX(hbox), main_area, TRUE, TRUE, 0);
    }

    void create_sidebar() {
        sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(sidebar, 280, -1);

        // Profile card
        create_profile_card();
        gtk_box_pack_start(GTK_BOX(sidebar), profile_card, FALSE, FALSE, 0);

        // Settings button
        GtkWidget* settings_button = gtk_button_new_with_label("Settings");
        g_signal_connect(settings_button, "clicked", G_CALLBACK(on_settings_clicked), this);
        gtk_box_pack_start(GTK_BOX(sidebar), settings_button, FALSE, FALSE, 0);

        // Search box
        search_entry = gtk_search_entry_new();
        gtk_box_pack_start(GTK_BOX(sidebar), search_entry, FALSE, FALSE, 0);

        // Tabs
        notebook = gtk_notebook_new();
        gtk_box_pack_start(GTK_BOX(sidebar), notebook, TRUE, TRUE, 0);

        // Chats tab
        chats_list = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(chats_list), GTK_SELECTION_SINGLE);
        g_signal_connect(chats_list, "row-selected", G_CALLBACK(on_chat_row_selected), this);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), chats_list, gtk_label_new("Chats"));

        // Devices tab
        GtkWidget* devices_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        GtkWidget* scan_button = gtk_button_new_with_label("Scan Devices");
        g_signal_connect(scan_button, "clicked", G_CALLBACK(on_scan_clicked), this);
        gtk_box_pack_start(GTK_BOX(devices_box), scan_button, FALSE, FALSE, 0);
        devices_grid = gtk_flow_box_new();
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(devices_grid), 1);
        gtk_box_pack_start(GTK_BOX(devices_box), devices_grid, TRUE, TRUE, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), devices_box, gtk_label_new("Devices"));

        // Transfers tab
        transfers_list = gtk_list_box_new();
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), transfers_list, gtk_label_new("Transfers"));
    }

    void create_profile_card() {
        profile_card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start(profile_card, 16);
        gtk_widget_set_margin_end(profile_card, 16);
        gtk_widget_set_margin_top(profile_card, 16);
        gtk_widget_set_margin_bottom(profile_card, 16);

        // Avatar placeholder
        GtkWidget* avatar = gtk_image_new_from_icon_name("avatar-default", GTK_ICON_SIZE_LARGE);
        gtk_box_pack_start(GTK_BOX(profile_card), avatar, FALSE, FALSE, 0);

        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_pack_start(GTK_BOX(profile_card), vbox, TRUE, TRUE, 0);

        std::string name = settings->get_user_name();
        if (name.empty()) name = "User";
        profile_name_label = gtk_label_new(name.c_str());
        gtk_widget_set_halign(profile_name_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(vbox), profile_name_label, FALSE, FALSE, 0);

        profile_device_label = gtk_label_new("Device Name");
        gtk_widget_set_halign(profile_device_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(vbox), profile_device_label, FALSE, FALSE, 0);
    }

    void create_main_area() {
        main_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

        // Conversation header
        conversation_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start(conversation_header, 16);
        gtk_widget_set_margin_end(conversation_header, 16);
        gtk_widget_set_margin_top(conversation_header, 16);
        gtk_widget_set_margin_bottom(conversation_header, 8);
        gtk_box_pack_start(GTK_BOX(main_area), conversation_header, FALSE, FALSE, 0);

        conversation_name_label = gtk_label_new("Select a conversation");
        gtk_widget_set_halign(conversation_name_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(conversation_header), conversation_name_label, TRUE, TRUE, 0);

        // Message list
        scrolled_messages = gtk_scrolled_window_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(main_area), scrolled_messages, TRUE, TRUE, 0);

        messages_list = gtk_list_box_new();
        gtk_container_add(GTK_CONTAINER(scrolled_messages), messages_list);

        // Composer
        composer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start(composer_box, 16);
        gtk_widget_set_margin_end(composer_box, 16);
        gtk_widget_set_margin_bottom(composer_box, 16);
        gtk_box_pack_start(GTK_BOX(main_area), composer_box, FALSE, FALSE, 0);

        message_text_view = gtk_text_view_new();
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(message_text_view), GTK_WRAP_WORD);
        gtk_widget_set_size_request(message_text_view, -1, 60);
        GtkWidget* scrolled_composer = gtk_scrolled_window_new(NULL, NULL);
        gtk_container_add(GTK_CONTAINER(scrolled_composer), message_text_view);
        gtk_box_pack_start(GTK_BOX(composer_box), scrolled_composer, TRUE, TRUE, 0);

        send_button = gtk_button_new_with_label("Send");
        g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_clicked), this);
        gtk_box_pack_start(GTK_BOX(composer_box), send_button, FALSE, FALSE, 0);

        attach_button = gtk_button_new_from_icon_name("mail-attachment", GTK_ICON_SIZE_BUTTON);
        g_signal_connect(attach_button, "clicked", G_CALLBACK(on_attach_clicked), this);
        gtk_box_pack_start(GTK_BOX(composer_box), attach_button, FALSE, FALSE, 0);

        // Transfer strip
        transfer_strip = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_pack_start(GTK_BOX(main_area), transfer_strip, FALSE, FALSE, 0);
    }

    void showOnboarding() {
        GtkWidget* dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                   "Welcome to BlueBeam!\n\n1. Go to Devices tab and scan for devices.\n2. Pair with a device.\n3. Start chatting or sending files.\n\nEnjoy secure Bluetooth communication!");
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
            gtk_label_set_text(GTK_LABEL(profile_name_label), name);
        }

        gtk_widget_destroy(dialog);
    }

    void update_message_list() {
        if (current_conversation_id.empty()) return;

        // Clear list
        GList* children = gtk_container_get_children(GTK_CONTAINER(messages_list));
        for (GList* iter = children; iter; iter = iter->next) {
            gtk_widget_destroy(GTK_WIDGET(iter->data));
        }
        g_list_free(children);

        auto messages = db->get_messages(current_conversation_id);
        for (const auto& msg : messages) {
            GtkWidget* bubble = create_message_bubble(msg);
            gtk_list_box_insert(GTK_LIST_BOX(messages_list), bubble, -1);
        }
        gtk_widget_show_all(messages_list);
    }

    GtkWidget* create_message_bubble(const Message& msg) {
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_margin_start(box, 8);
        gtk_widget_set_margin_end(box, 8);
        gtk_widget_set_margin_top(box, 4);
        gtk_widget_set_margin_bottom(box, 4);

        bool is_sent = (msg.sender_id == "self");
        gtk_widget_set_halign(box, is_sent ? GTK_ALIGN_END : GTK_ALIGN_START);

        GtkWidget* content_label = gtk_label_new(std::string(msg.content.begin(), msg.content.end()).c_str());
        gtk_label_set_wrap(GTK_LABEL(content_label), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(content_label), 40);
        gtk_widget_set_halign(content_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(box), content_label, FALSE, FALSE, 0);

        GtkWidget* time_label = gtk_label_new(msg.timestamp.c_str());
        gtk_widget_set_halign(time_label, GTK_ALIGN_END);
        gtk_box_pack_start(GTK_BOX(box), time_label, FALSE, FALSE, 0);

        return box;
    }

    void update_devices_list() {
        // Clear devices grid
        GList* children = gtk_container_get_children(GTK_CONTAINER(devices_grid));
        for (GList* iter = children; iter; iter = iter->next) {
            gtk_widget_destroy(GTK_WIDGET(iter->data));
        }
        g_list_free(children);

        auto devices = bt->get_discovered_devices();
        for (const auto& dev : devices) {
            GtkWidget* card = create_device_card(dev);
            gtk_flow_box_insert(GTK_FLOW_BOX(devices_grid), card, -1);
        }
        gtk_widget_show_all(devices_grid);
    }

    GtkWidget* create_device_card(const std::string& device_name) {
        GtkWidget* frame = gtk_frame_new(NULL);
        gtk_widget_set_size_request(frame, 140, 140);

        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_container_add(GTK_CONTAINER(frame), vbox);
        gtk_widget_set_margin_start(vbox, 8);
        gtk_widget_set_margin_end(vbox, 8);
        gtk_widget_set_margin_top(vbox, 8);
        gtk_widget_set_margin_bottom(vbox, 8);

        // Avatar
        GtkWidget* avatar = gtk_image_new_from_icon_name("bluetooth", GTK_ICON_SIZE_LARGE);
        gtk_box_pack_start(GTK_BOX(vbox), avatar, FALSE, FALSE, 0);

        // Name
        GtkWidget* name_label = gtk_label_new(device_name.c_str());
        gtk_box_pack_start(GTK_BOX(vbox), name_label, FALSE, FALSE, 0);

        // Signal bar (placeholder)
        GtkWidget* signal_label = gtk_label_new("Signal: Good");
        gtk_box_pack_start(GTK_BOX(vbox), signal_label, FALSE, FALSE, 0);

        // Pair button
        GtkWidget* pair_button = gtk_button_new_with_label("Pair");
        g_signal_connect(pair_button, "clicked", G_CALLBACK(on_pair_clicked), this);
        g_object_set_data(G_OBJECT(pair_button), "device_name", g_strdup(device_name.c_str()));
        gtk_box_pack_start(GTK_BOX(vbox), pair_button, FALSE, FALSE, 0);

        return frame;
    }

    void show_pairing_modal(const std::string& device_name) {
        GtkWidget* dialog = gtk_dialog_new_with_buttons("Pairing", GTK_WINDOW(window), GTK_DIALOG_MODAL,
                                                        "Confirm", GTK_RESPONSE_ACCEPT, "Cancel", GTK_RESPONSE_CANCEL, NULL);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 420, 260);

        GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_add(GTK_CONTAINER(content), vbox);
        gtk_widget_set_margin_start(vbox, 16);
        gtk_widget_set_margin_end(vbox, 16);
        gtk_widget_set_margin_top(vbox, 16);
        gtk_widget_set_margin_bottom(vbox, 16);

        // PIN display
        std::string pin = "123456"; // Generate real PIN
        GtkWidget* pin_label = gtk_label_new(("PIN: " + pin).c_str());
        gtk_box_pack_start(GTK_BOX(vbox), pin_label, FALSE, FALSE, 0);

        // Fingerprint
        GtkWidget* fingerprint_label = gtk_label_new("Fingerprint: ABC123...");
        gtk_box_pack_start(GTK_BOX(vbox), fingerprint_label, FALSE, FALSE, 0);

        gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            // Perform pairing/connect
            std::string device_id = bt->get_device_id_from_name(device_name);
            if (bt->connect(device_id)) {
                // Add to trusted devices
                Device dev{device_id, device_name, "", true, "", ""};
                db->add_device(dev);
                // Switch to chats tab
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
                update_chats_list();
            }
        }

        gtk_widget_destroy(dialog);
    }

    void update_chats_list() {
        // Clear chats list
        GList* children = gtk_container_get_children(GTK_CONTAINER(chats_list));
        for (GList* iter = children; iter; iter = iter->next) {
            gtk_widget_destroy(GTK_WIDGET(iter->data));
        }
        g_list_free(children);

        auto devices = db->get_devices();
        for (const auto& dev : devices) {
            if (dev.trusted) {
                GtkWidget* label = gtk_label_new(dev.name.c_str());
                g_object_set_data(G_OBJECT(label), "device_id", g_strdup(dev.id.c_str()));
                gtk_list_box_insert(GTK_LIST_BOX(chats_list), label, -1);
            }
        }
        gtk_widget_show_all(chats_list);
    }

    void update_transfers_list() {
        // Clear transfers list
        GList* children = gtk_container_get_children(GTK_CONTAINER(transfers_list));
        for (GList* iter = children; iter; iter = iter->next) {
            gtk_widget_destroy(GTK_WIDGET(iter->data));
        }
        g_list_free(children);

        auto files = db->get_files();
        for (const auto& file : files) {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_box_pack_start(GTK_BOX(row), gtk_label_new(file.filename.c_str()), FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(row), gtk_progress_bar_new(), TRUE, TRUE, 0);
            gtk_list_box_insert(GTK_LIST_BOX(transfers_list), row, -1);
        }
        gtk_widget_show_all(transfers_list);
    }

    void show_incoming_file_dialog(const std::string& filename, uint64_t size, std::function<void(bool, const std::string&)> response) {
        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                                                   "Incoming file: %s (%llu bytes)\nAccept?", filename.c_str(), size);
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
            response(true, "/tmp/" + filename);
        } else {
            response(false, "");
        }
        gtk_widget_destroy(dialog);
    }

    void run() {
        gtk_widget_show_all(window);
        gtk_main();
    }

private:
    static void on_scan_clicked(GtkWidget* widget, gpointer data) {
        UI::Impl* impl = static_cast<UI::Impl*>(data);
        impl->bt->scan();
        impl->update_devices_list();
    }

    static void on_pair_clicked(GtkWidget* widget, gpointer data) {
        UI::Impl* impl = static_cast<UI::Impl*>(data);
        const char* device_name = (const char*)g_object_get_data(G_OBJECT(widget), "device_name");
        impl->show_pairing_modal(device_name);
    }

    static void on_chat_row_selected(GtkListBox* listbox, GtkListBoxRow* row, gpointer data) {
        UI::Impl* impl = static_cast<UI::Impl*>(data);
        if (!row) return;
        GtkWidget* child = gtk_bin_get_child(GTK_BIN(row));
        const char* device_id = (const char*)g_object_get_data(G_OBJECT(child), "device_id");
        impl->current_conversation_id = device_id;
        impl->current_device_id = device_id;
        impl->update_message_list();
        // Update header
        auto devices = impl->db->get_devices();
        for (const auto& dev : devices) {
            if (dev.id == device_id) {
                gtk_label_set_text(GTK_LABEL(impl->conversation_name_label), dev.name.c_str());
                break;
            }
        }
    }

    static void on_send_clicked(GtkWidget* widget, gpointer data) {
        UI::Impl* impl = static_cast<UI::Impl*>(data);
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(impl->message_text_view));
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        char* text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
        if (strlen(text) > 0 && !impl->current_device_id.empty()) {
            std::string msg = text;
            std::vector<uint8_t> content(msg.begin(), msg.end());
            std::string conversation_id = impl->current_device_id;
            std::string id = "msg_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            impl->messaging->send_message(id, conversation_id, "self", impl->current_device_id, content, MessageStatus::SENT);
            gtk_text_buffer_set_text(buffer, "", -1);
            impl->update_message_list();
        }
        g_free(text);
    }

    static void on_attach_clicked(GtkWidget* widget, gpointer data) {
        UI::Impl* impl = static_cast<UI::Impl*>(data);
        GtkWidget* dialog = gtk_file_chooser_dialog_new("Select File",
                                                        GTK_WINDOW(impl->window),
                                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                                        "Cancel", GTK_RESPONSE_CANCEL,
                                                        "Send", GTK_RESPONSE_ACCEPT,
                                                        NULL);
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            std::string path = filename;
            g_free(filename);
            if (!impl->current_device_id.empty()) {
                impl->ft->send_file(path, impl->current_device_id,
                    [impl](uint64_t sent, uint64_t total) {
                        // Update transfer strip
                        impl->update_transfers_list();
                    },
                    [impl](bool success, const std::string& error) {
                        impl->update_transfers_list();
                        if (!success) {
                            GtkWidget* err_dialog = gtk_message_dialog_new(GTK_WINDOW(impl->window), GTK_DIALOG_MODAL,
                                                                           GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                                                           ("File send failed: " + error).c_str());
                            gtk_dialog_run(GTK_DIALOG(err_dialog));
                            gtk_widget_destroy(err_dialog);
                        }
                    });
            }
        }
        gtk_widget_destroy(dialog);
    }

    static void on_settings_clicked(GtkWidget* widget, gpointer data) {
        UI::Impl* impl = static_cast<UI::Impl*>(data);
        impl->showSettingsDialog();
    }

    // Widgets
    GtkWidget* window;
    GtkWidget* sidebar;
    GtkWidget* profile_card;
    GtkWidget* profile_name_label;
    GtkWidget* profile_device_label;
    GtkWidget* search_entry;
    GtkWidget* notebook;
    GtkWidget* chats_list;
    GtkWidget* devices_grid;
    GtkWidget* transfers_list;
    GtkWidget* main_area;
    GtkWidget* conversation_header;
    GtkWidget* conversation_name_label;
    GtkWidget* scrolled_messages;
    GtkWidget* messages_list;
    GtkWidget* composer_box;
    GtkWidget* message_text_view;
    GtkWidget* send_button;
    GtkWidget* attach_button;
    GtkWidget* transfer_strip;
    GtkWidget* name_entry;
};

UI::UI() : pimpl(std::make_unique<Impl>()) {}
UI::~UI() = default;

void UI::run() {
    pimpl->run();
}