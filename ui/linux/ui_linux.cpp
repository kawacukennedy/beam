#include "ui_linux.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <iostream>
#include <vector>
#include <string>

// Forward declarations
class Database;
class Messaging;
class FileTransfer;
class Bluetooth;
class Settings;

class UILinux::Impl {
public:
    GtkWidget *window;
    GtkWidget *main_box;
    GtkWidget *sidebar;
    GtkWidget *content_area;
    GtkWidget *profile_card;
    GtkWidget *search_entry;
    GtkWidget *tabs;
    GtkWidget *chat_list;
    GtkWidget *device_list;
    GtkWidget *transfer_list;
    GtkWidget *conversation_header;
    GtkWidget *message_list;
    GtkWidget *composer;
    GtkWidget *transfer_strip;

    Database *db;
    Messaging *messaging;
    FileTransfer *file_transfer;
    Bluetooth *bluetooth;
    Settings *settings;

    Impl(Database *d, Messaging *m, FileTransfer *f, Bluetooth *b, Settings *s)
        : db(d), messaging(m), file_transfer(f), bluetooth(b), settings(s) {}

    void setup_ui() {
        window = gtk_window_new();
        gtk_window_set_title(GTK_WINDOW(window), "BlueBeam");
        gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);
        g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

        main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_container_add(GTK_CONTAINER(window), main_box);

        // Sidebar
        sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(sidebar, 280, -1);
        gtk_box_pack_start(GTK_BOX(main_box), sidebar, FALSE, FALSE, 0);

        // Profile card
        profile_card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_pack_start(GTK_BOX(sidebar), profile_card, FALSE, FALSE, 8);

        GtkWidget *avatar = gtk_image_new_from_icon_name("avatar-default", GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_box_pack_start(GTK_BOX(profile_card), avatar, FALSE, FALSE, 0);

        GtkWidget *profile_info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_pack_start(GTK_BOX(profile_card), profile_info, TRUE, TRUE, 0);

        GtkWidget *name_label = gtk_label_new("User Name");
        gtk_widget_set_halign(name_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(profile_info), name_label, FALSE, FALSE, 0);

        GtkWidget *device_label = gtk_label_new("Device Nickname");
        gtk_widget_set_halign(device_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(profile_info), device_label, FALSE, FALSE, 0);

        // Search
        search_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Search...");
        gtk_box_pack_start(GTK_BOX(sidebar), search_entry, FALSE, FALSE, 8);

        // Tabs
        tabs = gtk_notebook_new();
        gtk_box_pack_start(GTK_BOX(sidebar), tabs, TRUE, TRUE, 0);

        // Chats tab
        chat_list = gtk_list_box_new();
        gtk_notebook_append_page(GTK_NOTEBOOK(tabs), chat_list, gtk_label_new("Chats"));

        // Devices tab
        device_list = gtk_list_box_new();
        GtkWidget *scan_button = gtk_button_new_with_label("Scan");
        gtk_list_box_insert(GTK_LIST_BOX(device_list), scan_button, -1);
        gtk_notebook_append_page(GTK_NOTEBOOK(tabs), device_list, gtk_label_new("Devices"));

        // Transfers tab
        transfer_list = gtk_list_box_new();
        gtk_notebook_append_page(GTK_NOTEBOOK(tabs), transfer_list, gtk_label_new("Transfers"));

        // Content area
        content_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_pack_start(GTK_BOX(main_box), content_area, TRUE, TRUE, 0);

        // Conversation header
        conversation_header = gtk_label_new("Select a chat");
        gtk_widget_set_halign(conversation_header, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(content_area), conversation_header, FALSE, FALSE, 8);

        // Message list
        GtkWidget *scrolled_messages = gtk_scrolled_window_new(NULL, NULL);
        message_list = gtk_list_box_new();
        gtk_container_add(GTK_CONTAINER(scrolled_messages), message_list);
        gtk_box_pack_start(GTK_BOX(content_area), scrolled_messages, TRUE, TRUE, 0);

        // Composer
        composer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_pack_start(GTK_BOX(content_area), composer, FALSE, FALSE, 8);

        GtkWidget *text_view = gtk_text_view_new();
        gtk_box_pack_start(GTK_BOX(composer), text_view, TRUE, TRUE, 0);

        GtkWidget *send_button = gtk_button_new_with_label("Send");
        gtk_box_pack_start(GTK_BOX(composer), send_button, FALSE, FALSE, 0);

        // Transfer strip
        transfer_strip = gtk_label_new("No active transfers");
        gtk_box_pack_start(GTK_BOX(content_area), transfer_strip, FALSE, FALSE, 8);

        gtk_widget_show_all(window);
    }
};

UILinux::UILinux(Database *db, Messaging *messaging, FileTransfer *file_transfer, Bluetooth *bluetooth, Settings *settings)
    : pimpl(std::make_unique<Impl>(db, messaging, file_transfer, bluetooth, settings)) {}

UILinux::~UILinux() = default;

void UILinux::run() {
    pimpl->setup_ui();
    gtk_main();
}