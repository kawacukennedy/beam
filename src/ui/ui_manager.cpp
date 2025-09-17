#include "ui_manager.h"
#include <QApplication>
#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QDebug>
#include <QTimer>
#include <QMessageBox>

// Global UI elements
static QApplication *app = nullptr;
static QMainWindow *mainWindow = nullptr;
static QStackedWidget *stackedWidget = nullptr;

// Screens
static QWidget *splashScreen = nullptr;
static QWidget *pairingScreen = nullptr;
static QWidget *chatScreen = nullptr;
static QWidget *fileTransferScreen = nullptr;
static QWidget *settingsScreen = nullptr;

// Pairing Screen elements
static QListWidget *deviceListWidget = nullptr;
static QPushButton *discoverDevicesButton = nullptr;
static QPushButton *connectButton = nullptr;

// Chat Screen elements
static QListWidget *chatDeviceListWidget = nullptr;
static QTextEdit *chatHistoryTextEdit = nullptr;
static QLineEdit *messageInputLineEdit = nullptr;
static QPushButton *sendMessageButton = nullptr;
static QPushButton *attachFileButton = nullptr;

// File Transfer Screen elements
static QListWidget *fileTransferListWidget = nullptr;

// Callback for C backend events
static UiEventCallback global_ui_event_callback = NULL;

// --- Helper functions for UI construction ---
static QWidget* createSplashScreen() {
    QWidget *screen = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(screen);
    QLabel *logoLabel = new QLabel("BlueBeamNative", screen);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setStyleSheet("font-size: 48px; font-weight: bold;");
    layout->addWidget(logoLabel);

    QLabel *spinnerLabel = new QLabel("Loading...", screen);
    spinnerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(spinnerLabel);

    return screen;
}

static QWidget* createPairingScreen() {
    QWidget *screen = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(screen);

    // Left sidebar for device list
    QVBoxLayout *sidebarLayout = new QVBoxLayout();
    QLabel *titleLabel = new QLabel("Discovered Devices", screen);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold;");
    sidebarLayout->addWidget(titleLabel);

    deviceListWidget = new QListWidget(screen);
    sidebarLayout->addWidget(deviceListWidget);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    discoverDevicesButton = new QPushButton("Discover Devices", screen);
    connectButton = new QPushButton("Connect", screen);
    buttonLayout->addWidget(discoverDevicesButton);
    buttonLayout->addWidget(connectButton);
    sidebarLayout->addLayout(buttonLayout);

    mainLayout->addLayout(sidebarLayout, 1);

    // Right side for details/empty state
    QVBoxLayout *detailsLayout = new QVBoxLayout();
    QLabel *emptyStateLabel = new QLabel("No devices found. Enable Bluetooth and click 'Discover Devices'.", screen);
    emptyStateLabel->setAlignment(Qt::AlignCenter);
    detailsLayout->addWidget(emptyStateLabel);
    mainLayout->addLayout(detailsLayout, 2);

    // Connect signals to slots
    QObject::connect(discoverDevicesButton, &QPushButton::clicked, []() {
        qDebug() << "Discover Devices button clicked!";
        ui_gui_request_bluetooth_discovery();
    });

    QObject::connect(connectButton, &QPushButton::clicked, []() {
        if (deviceListWidget->currentItem()) {
            QString deviceAddress = deviceListWidget->currentItem()->data(Qt::UserRole).toString();
            qDebug() << "Connect button clicked for device:" << deviceAddress;
            ui_gui_request_connect_device(deviceAddress.toUtf8().constData());
        }
    });

    return screen;
}

static QWidget* createChatScreen() {
    QWidget *screen = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(screen);

    // Left sidebar for device list
    QVBoxLayout *sidebarLayout = new QVBoxLayout();
    QLabel *titleLabel = new QLabel("Chats", screen);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold;");
    sidebarLayout->addWidget(titleLabel);

    chatDeviceListWidget = new QListWidget(screen);
    sidebarLayout->addWidget(chatDeviceListWidget);
    mainLayout->addLayout(sidebarLayout, 1);

    // Right side for chat area
    QVBoxLayout *chatLayout = new QVBoxLayout();
    chatHistoryTextEdit = new QTextEdit(screen);
    chatHistoryTextEdit->setReadOnly(true);
    chatLayout->addWidget(chatHistoryTextEdit);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    messageInputLineEdit = new QLineEdit(screen);
    messageInputLineEdit->setPlaceholderText("Type a message...");
    sendMessageButton = new QPushButton("Send", screen);
    attachFileButton = new QPushButton("Attach File", screen);
    inputLayout->addWidget(messageInputLineEdit);
    inputLayout->addWidget(sendMessageButton);
    inputLayout->addWidget(attachFileButton);
    chatLayout->addLayout(inputLayout);

    mainLayout->addLayout(chatLayout, 3);

    // Connect signals to slots
    QObject::connect(sendMessageButton, &QPushButton::clicked, []() {
        if (chatDeviceListWidget->currentItem()) {
            QString deviceAddress = chatDeviceListWidget->currentItem()->data(Qt::UserRole).toString();
            QString message = messageInputLineEdit->text();
            if (!message.isEmpty()) {
                qDebug() << "Send message to" << deviceAddress << ":" << message;
                ui_gui_request_send_message(deviceAddress.toUtf8().constData(), message.toUtf8().constData());
                messageInputLineEdit->clear();
            }
        }
    });

    return screen;
}

static QWidget* createFileTransferScreen() {
    QWidget *screen = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(screen);
    QLabel *titleLabel = new QLabel("File Transfers", screen);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold;");
    layout->addWidget(titleLabel);

    fileTransferListWidget = new QListWidget(screen);
    layout->addWidget(fileTransferListWidget);

    return screen;
}

static QWidget* createSettingsScreen() {
    QWidget *screen = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(screen);
    QLabel *titleLabel = new QLabel("Settings", screen);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold;");
    layout->addWidget(titleLabel);

    // TODO: Add actual settings options
    QLabel *placeholder = new QLabel("Settings options will go here.", screen);
    layout->addWidget(placeholder);

    return screen;
}

// --- UI Manager C functions ---
void ui_manager_init(void) {
    // QApplication is initialized in main.cpp
    mainWindow = new QMainWindow();
    mainWindow->setWindowTitle("BlueBeamNative");
    mainWindow->resize(1024, 768);

    stackedWidget = new QStackedWidget(mainWindow);
    mainWindow->setCentralWidget(stackedWidget);

    splashScreen = createSplashScreen();
    pairingScreen = createPairingScreen();
    chatScreen = createChatScreen();
    fileTransferScreen = createFileTransferScreen();
    settingsScreen = createSettingsScreen();

    stackedWidget->addWidget(splashScreen);
    stackedWidget->addWidget(pairingScreen);
    stackedWidget->addWidget(chatScreen);
    stackedWidget->addWidget(fileTransferScreen);
    stackedWidget->addWidget(settingsScreen);

    // Initial screen
    ui_show_splash_screen();

    mainWindow->show();
}

void ui_manager_run_loop(void) {
    // QApplication::exec() is called in main.cpp
    // This function is primarily for conceptual completeness in the C interface.
}

void ui_manager_register_event_callback(UiEventCallback callback) {
    global_ui_event_callback = callback;
    qDebug() << "UI Event callback registered.";
}

// Functions to update UI elements
void ui_show_splash_screen(void) {
    stackedWidget->setCurrentWidget(splashScreen);
    // After a delay, transition to pairing screen
    QTimer::singleShot(2000, []() { ui_show_pairing_screen(); });
}

void ui_show_pairing_screen(void) {
    stackedWidget->setCurrentWidget(pairingScreen);
}

void ui_show_chat_screen(void) {
    stackedWidget->setCurrentWidget(chatScreen);
}

void ui_show_file_transfer_screen(void) {
    stackedWidget->setCurrentWidget(fileTransferScreen);
}

void ui_show_settings_screen(void) {
    stackedWidget->setCurrentWidget(settingsScreen);
}

void ui_add_discovered_device(const char* device_name, const char* device_address, int rssi) {
    if (deviceListWidget) {
        QListWidgetItem *item = new QListWidgetItem(QString("%1 (%2) [RSSI: %3]").arg(device_name).arg(device_address).arg(rssi));
        item->setData(Qt::UserRole, QString(device_address)); // Store address for connection
        deviceListWidget->addItem(item);
        qDebug() << "Added discovered device to UI:" << device_name << device_address;
    }
}

void ui_clear_discovered_devices(void) {
    if (deviceListWidget) {
        deviceListWidget->clear();
        qDebug() << "Cleared discovered devices from UI.";
    }
}

void ui_update_device_connection_status(const char* device_address, bool is_connected) {
    // TODO: Update UI to reflect connection status (e.g., change icon, text color)
    qDebug() << "Device connection status updated for" << device_address << ":" << is_connected;
    // For chat screen device list
    if (chatDeviceListWidget) {
        bool found = false;
        for (int i = 0; i < chatDeviceListWidget->count(); ++i) {
            QListWidgetItem *item = chatDeviceListWidget->item(i);
            if (item->data(Qt::UserRole).toString() == QString(device_address)) {
                // Update existing item
                // item->setText(QString("%1 %2").arg(item->text().split(" ").first()).arg(is_connected ? "(Connected)" : "(Disconnected)"));
                found = true;
                break;
            }
        }
        if (!found && is_connected) {
            // Add new item to chat device list if connected and not already there
            QListWidgetItem *item = new QListWidgetItem(QString("%1 (Connected)").arg(device_address)); // Use device_address as name for now
            item->setData(Qt::UserRole, QString(device_address));
            chatDeviceListWidget->addItem(item);
        }
    }
}

void ui_add_message_bubble(const char* device_address, const char* message, bool is_outgoing) {
    if (chatHistoryTextEdit) {
        QString htmlMessage = QString("<p style=\"margin: 0; padding: 5px; border-radius: 10px; display: inline-block; max-width: 60%; %1\">%2</p>")
                                .arg(is_outgoing ? "background-color: #1E88E5; color: white; float: right;" : "background-color: #F5F5F5; color: #212121; float: left;")
                                .arg(message);
        chatHistoryTextEdit->append(htmlMessage);
        qDebug() << "Added message bubble to UI:" << message;
    }
}

void ui_clear_chat_messages(const char* device_address) {
    if (chatHistoryTextEdit) {
        chatHistoryTextEdit->clear();
        qDebug() << "Cleared chat messages from UI for" << device_address;
    }
}

void ui_update_file_transfer_progress(const char* device_address, const char* filename, double progress) {
    // TODO: Update progress bar for specific file transfer item
    qDebug() << "File transfer progress for" << filename << ":" << progress * 100 << "%\n";
}

void ui_add_file_transfer_item(const char* device_address, const char* filename, bool is_sending) {
    if (fileTransferListWidget) {
        QListWidgetItem *item = new QListWidgetItem(QString("%1 (%2) - %3").arg(filename).arg(device_address).arg(is_sending ? "Sending" : "Receiving"));
        // TODO: Store more data for progress updates
        fileTransferListWidget->addItem(item);
        qDebug() << "Added file transfer item to UI:" << filename;
    }
}

void ui_remove_file_transfer_item(const char* device_address, const char* filename) {
    // TODO: Find and remove specific file transfer item
    qDebug() << "Removed file transfer item from UI:" << filename;
}

void ui_show_alert(const char* title, const char* message) {
    QMessageBox::information(mainWindow, QString(title), QString(message));
    qDebug() << "Showing alert:" << title << message;
}

// --- GUI Request Functions (called by GUI, trigger C backend events) ---
void ui_gui_request_bluetooth_discovery(void) {
    if (global_ui_event_callback) {
        UiEvent event = { .type = UI_EVENT_REQUEST_BLUETOOTH_DISCOVERY, .data = NULL };
        global_ui_event_callback(&event);
        qDebug() << "Triggered UI_EVENT_REQUEST_BLUETOOTH_DISCOVERY.";
    }
}

void ui_gui_request_connect_device(const char* device_address) {
    if (global_ui_event_callback) {
        // Allocate memory for device_address to pass to callback
        char* addr_copy = strdup(device_address);
        UiEvent event = { .type = UI_EVENT_REQUEST_CONNECT_DEVICE, .data = addr_copy };
        global_ui_event_callback(&event);
        qDebug() << "Triggered UI_EVENT_REQUEST_CONNECT_DEVICE for" << device_address;
    }
}

void ui_gui_request_disconnect_device(const char* device_address) {
    if (global_ui_event_callback) {
        char* addr_copy = strdup(device_address);
        UiEvent event = { .type = UI_EVENT_REQUEST_DISCONNECT_DEVICE, .data = addr_copy };
        global_ui_event_callback(&event);
        qDebug() << "Triggered UI_EVENT_REQUEST_DISCONNECT_DEVICE for" << device_address;
    }
}

void ui_gui_request_send_message(const char* device_address, const char* message) {
    if (global_ui_event_callback) {
        // Create a struct to hold both device_address and message
        typedef struct { char* addr; char* msg; } MessageData;
        MessageData* data = (MessageData*)malloc(sizeof(MessageData));
        data->addr = strdup(device_address);
        data->msg = strdup(message);

        UiEvent event = { .type = UI_EVENT_SEND_MESSAGE, .data = data };
        global_ui_event_callback(&event);
        qDebug() << "Triggered UI_EVENT_SEND_MESSAGE to" << device_address << ":" << message;
    }
}

void ui_gui_request_send_file(const char* device_address, const char* file_path) {
    if (global_ui_event_callback) {
        // Create a struct to hold both device_address and file_path
        typedef struct { char* addr; char* path; } FileData;
        FileData* data = (FileData*)malloc(sizeof(FileData));
        data->addr = strdup(device_address);
        data->path = strdup(file_path);

        UiEvent event = { .type = UI_EVENT_REQUEST_SEND_FILE, .data = data };
        global_ui_event_callback(&event);
        qDebug() << "Triggered UI_EVENT_REQUEST_SEND_FILE to" << device_address << ":" << file_path;
    }
}