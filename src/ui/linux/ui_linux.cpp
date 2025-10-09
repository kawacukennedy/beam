#include "ui/ui.h"
#include <gtk/gtk.h>
#include <iostream>

class UI::Impl {
public:
    Impl() {
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

        // Chat view
        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

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
    }

    void run() {
        gtk_widget_show_all(window);
        gtk_main();
    }

private:
    static void on_scan_clicked(GtkWidget* widget, gpointer data) {
        std::cout << "Scan devices" << std::endl;
    }

    static void on_connect_clicked(GtkWidget* widget, gpointer data) {
        std::cout << "Connect" << std::endl;
    }

    static void on_send_clicked(GtkWidget* widget, gpointer data) {
        std::cout << "Send message" << std::endl;
    }

    static void on_file_clicked(GtkWidget* widget, gpointer data) {
        std::cout << "Send file" << std::endl;
    }

    GtkWidget* window;
    GtkWidget* scan_button;
    GtkWidget* connect_button;
    GtkWidget* scrolled_window;
    GtkWidget* chat_view;
    GtkWidget* message_entry;
    GtkWidget* send_button;
    GtkWidget* file_button;
};

UI::UI() : pimpl(std::make_unique<Impl>()) {}
UI::~UI() = default;

void UI::run() {
    pimpl->run();
}