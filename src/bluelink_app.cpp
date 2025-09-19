#include "bluelink_app.h"
#include "ui/ui_manager.h"
#include "bluetooth/bluetooth_manager.h"
#include "database/db_manager.h"
#include "bluetooth/bluetooth_callbacks.h"
#include "notifications/notification_manager.h" // Include notification manager
#include <QDebug>
#include <QTimer>
#include <QCoreApplication>

// Static pointer to the BlueLinkApp instance to allow C callbacks to access it
static BlueLinkApp* s_instance = nullptr;

BlueLinkApp::BlueLinkApp(QObject *parent) : 
    QObject(parent),
    m_uiManager(new UIManager(this)),
    m_bluetoothManager(get_bluetooth_manager()),
    m_databaseManager(nullptr), // Initialize to nullptr, create in start()
    m_isBluetoothPoweredOn(false)
{
    s_instance = this;

    // Set up C Bluetooth callbacks to point to our static methods
    g_bluetooth_ui_callbacks.show_alert = BlueLinkApp::staticShowAlert;
    g_bluetooth_ui_callbacks.clear_discovered_devices = BlueLinkApp::staticClearDiscoveredDevices;
    g_bluetooth_ui_callbacks.add_discovered_device = BlueLinkApp::staticAddDiscoveredDevice;
    g_bluetooth_ui_callbacks.update_device_connection_status = BlueLinkApp::staticUpdateDeviceConnectionStatus;
    g_bluetooth_ui_callbacks.add_message_bubble = BlueLinkApp::staticAddMessageBubble;
    g_bluetooth_ui_callbacks.update_file_transfer_progress = BlueLinkApp::staticUpdateFileTransferProgress;
    g_bluetooth_ui_callbacks.add_file_transfer_item = BlueLinkApp::staticAddFileTransferItem;
    g_bluetooth_ui_callbacks.remove_file_transfer_item = BlueLinkApp::staticRemoveFileTransferItem;

    // Pass this BlueLinkApp instance to the UIManager
    m_uiManager->setBlueLinkApp(this);

    // Connect UI signals to Bluetooth manager (example connections)
    QObject::connect(m_uiManager, &UIManager::requestBluetoothDiscovery, this, [&]() {
        if (m_bluetoothManager) {
            m_bluetoothManager->discoverDevices();
            qDebug() << "BlueLinkApp: Requesting Bluetooth discovery.";
        }
    });
    QObject::connect(m_uiManager, &UIManager::requestConnectDevice, this, [&](const QString& address) {
        if (m_bluetoothManager) {
            m_bluetoothManager->connect(address.toUtf8().constData());
            qDebug() << "BlueLinkApp: Requesting connect to device:" << address;
        }
    });
    QObject::connect(m_uiManager, &UIManager::requestDisconnectDevice, this, [&](const QString& address) {
        if (m_bluetoothManager) {
            m_bluetoothManager->disconnect(address.toUtf8().constData());
            qDebug() << "BlueLinkApp: Requesting disconnect from device:" << address;
        }
    });
    QObject::connect(m_uiManager, &UIManager::requestSendMessage, this, [&](const QString& address, const QString& message) {
        if (m_bluetoothManager) {
            m_bluetoothManager->sendMessage(address.toUtf8().constData(), message.toUtf8().constData());
            qDebug() << "BlueLinkApp: Requesting send message to" << address << ":" << message;
        }
    });
    QObject::connect(m_uiManager, &UIManager::requestSendFile, this, [&](const QString& address, const QString& filePath) {
        if (m_bluetoothManager) {
            m_bluetoothManager->sendFile(address.toUtf8().constData(), filePath.toUtf8().constData());
            qDebug() << "BlueLinkApp: Requesting send file to" << address << ":" << filePath;
        }
    });
    QObject::connect(m_uiManager, &UIManager::requestShowScreen, this, [&](const QString& screenName) {
        qDebug() << "BlueLinkApp: Requesting show screen:" << screenName;
        // This signal can be used to switch screens in QML if needed, or for C++ logic
    });
}

BlueLinkApp::~BlueLinkApp()
{
    if (m_uiManager) {
        delete m_uiManager;
        m_uiManager = nullptr;
    }
    if (m_databaseManager) {
        db_close();
        // m_databaseManager is not dynamically allocated here, so no delete
        m_databaseManager = nullptr;
    }
    s_instance = nullptr;
}

void BlueLinkApp::start()
{
    // Initialize database
    // m_databaseManager = new DatabaseManager(this); // Placeholder, actual implementation will be C-style
    db_open("bluelink.db"); // Open the database
    db_create_tables(); // Create tables if they don't exist

    // Initialize notification manager
    notification_manager_init();

    m_uiManager->show();
}

void BlueLinkApp::setIsBluetoothPoweredOn(bool poweredOn) {
    if (m_isBluetoothPoweredOn != poweredOn) {
        m_isBluetoothPoweredOn = poweredOn;
        emit bluetoothPoweredOnChanged();
    }
}

void BlueLinkApp::startScan() {
    if (m_bluetoothManager) {
        m_bluetoothManager->discoverDevices();
    }
}

void BlueLinkApp::stopScan() {
    // TODO: Implement stop scan in bluetooth_manager
    qDebug() << "BlueLinkApp: Stop scan requested.";
}

void BlueLinkApp::connectDevice(const QString &deviceAddress) {
    if (m_bluetoothManager) {
        m_bluetoothManager->connect(deviceAddress.toUtf8().constData());
    }
}

void BlueLinkApp::disconnectDevice(const QString &deviceAddress) {
    if (m_bluetoothManager) {
        m_bluetoothManager->disconnect(deviceAddress.toUtf8().constData());
    }
}

void BlueLinkApp::sendMessage(const QString &deviceAddress, const QString &message) {
    if (m_bluetoothManager) {
        m_bluetoothManager->sendMessage(deviceAddress.toUtf8().constData(), message.toUtf8().constData());
    }
}

void BlueLinkApp::sendFile(const QString &deviceAddress, const QString &filePath) {
    if (m_bluetoothManager) {
        m_bluetoothManager->sendFile(deviceAddress.toUtf8().constData(), filePath.toUtf8().constData());
    }
}

void BlueLinkApp::emitDeviceDiscovered(const QString &name, const QString &address, int rssi) {
    emit deviceDiscovered(name, address, rssi);
}

void BlueLinkApp::emitDeviceConnected(const QString &address, bool connected) {
    emit deviceConnected(address, connected);
}

void BlueLinkApp::emitMessageReceived(const QString &deviceAddress, const QString &message) {
    emit messageReceived(deviceAddress, message);
}

void BlueLinkApp::emitFileTransferProgress(const QString &deviceAddress, const QString &fileName, double progress) {
    emit fileTransferProgress(deviceAddress, fileName, progress);
}

void BlueLinkApp::emitFileTransferFinished(const QString &deviceAddress, const QString &fileName, bool success) {
    emit fileTransferFinished(deviceAddress, fileName, success);
}

void BlueLinkApp::emitFileTransferError(const QString &deviceAddress, const QString &fileName, const QString &error) {
    emit fileTransferError(deviceAddress, fileName, error);
}

void BlueLinkApp::emitShowAlert(const QString &title, const QString &message) {
    emit showAlert(title, message);
}

void BlueLinkApp::emitClearDiscoveredDevices() {
    emit clearDiscoveredDevices();
}

void BlueLinkApp::emitAddMessageBubble(const QString &deviceAddress, const QString &message, bool isOutgoing) {
    emit addMessageBubble(deviceAddress, message, isOutgoing);
}

void BlueLinkApp::emitAddFileTransferItem(const QString &deviceAddress, const QString &fileName, bool isSending) {
    emit addFileTransferItem(deviceAddress, fileName, isSending);
}

void BlueLinkApp::emitRemoveFileTransferItem(const QString &deviceAddress, const QString &fileName) {
    emit removeFileTransferItem(deviceAddress, fileName);
}

// Static Callbacks
void BlueLinkApp::staticShowAlert(const char* title, const char* message) {
    // Directly call the notification manager to show the alert
    notification_manager_show(NOTIFICATION_TYPE_ERROR, title, message, NULL); // Assuming error type for generic alerts
    // Optionally, still emit a signal for QML to react to, if needed for in-app alerts
    if (s_instance) {
        s_instance->emitShowAlert(QString::fromUtf8(title), QString::fromUtf8(message));
    }
}

void BlueLinkApp::staticClearDiscoveredDevices() {
    if (s_instance) {
        s_instance->emitClearDiscoveredDevices();
    }
}

void BlueLinkApp::staticAddDiscoveredDevice(const char* name, const char* address, int rssi) {
    if (s_instance) {
        s_instance->emitDeviceDiscovered(QString::fromUtf8(name), QString::fromUtf8(address), rssi);
    }
}

void BlueLinkApp::staticUpdateDeviceConnectionStatus(const char* address, bool connected) {
    if (s_instance) {
        s_instance->emitDeviceConnected(QString::fromUtf8(address), connected);
    }
}

void BlueLinkApp::staticAddMessageBubble(const char* deviceAddress, const char* message, bool isOutgoing) {
    if (s_instance) {
        s_instance->emitAddMessageBubble(QString::fromUtf8(deviceAddress), QString::fromUtf8(message), isOutgoing);
    }
}

void BlueLinkApp::staticUpdateFileTransferProgress(const char* deviceAddress, const char* fileName, double progress) {
    if (s_instance) {
        s_instance->emitFileTransferProgress(QString::fromUtf8(deviceAddress), QString::fromUtf8(fileName), progress);
    }
}

void BlueLinkApp::staticAddFileTransferItem(const char* deviceAddress, const char* fileName, bool isSending) {
    if (s_instance) {
        s_instance->emitAddFileTransferItem(QString::fromUtf8(deviceAddress), QString::fromUtf8(fileName), isSending);
    }
}

void BlueLinkApp::staticRemoveFileTransferItem(const char* deviceAddress, const char* fileName) {
    if (s_instance) {
        s_instance->emitRemoveFileTransferItem(QString::fromUtf8(deviceAddress), QString::fromUtf8(fileName));
    }
}
