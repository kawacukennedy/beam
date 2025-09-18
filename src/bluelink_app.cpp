#include "bluelink_app.h"
#include "ui/ui_manager.h"
#include "bluetooth/bluetooth_manager.h"
// #include "database/db_manager.h" // To be included later

// Placeholder for IBluetoothManager creation
// This will be properly implemented later based on OS and JSON specs
std::unique_ptr<IBluetoothManager> createBluetoothManager() {
    // For now, return a nullptr or a dummy implementation
    return nullptr;
}

BlueLinkApp::BlueLinkApp(QObject *parent) : QObject(parent)
{
    m_uiManager = new UIManager(this);
    m_bluetoothManager = createBluetoothManager(); // Now uncommented
    // m_databaseManager = new DatabaseManager(this); // To be added later

    // Connect UI signals to Bluetooth manager (example connections)
    QObject::connect(m_uiManager, &UIManager::requestBluetoothDiscovery, this, [&]() {
        if (m_bluetoothManager) {
            // m_bluetoothManager->discoverDevices(); // Placeholder
            qDebug() << "BlueLinkApp: Requesting Bluetooth discovery.";
        }
    });
    QObject::connect(m_uiManager, &UIManager::requestConnectDevice, this, [&](const QString& address) {
        if (m_bluetoothManager) {
            // m_bluetoothManager->connect(address.toStdString()); // Placeholder
            qDebug() << "BlueLinkApp: Requesting connect to device:" << address;
        }
    });
    QObject::connect(m_uiManager, &UIManager::requestDisconnectDevice, this, [&](const QString& address) {
        if (m_bluetoothManager) {
            // m_bluetoothManager->disconnect(address.toStdString()); // Placeholder
            qDebug() << "BlueLinkApp: Requesting disconnect from device:" << address;
        }
    });
    QObject::connect(m_uiManager, &UIManager::requestSendMessage, this, [&](const QString& address, const QString& message) {
        if (m_bluetoothManager) {
            // m_bluetoothManager->sendMessage(address.toStdString(), message.toStdString()); // Placeholder
            qDebug() << "BlueLinkApp: Requesting send message to" << address << ":" << message;
        }
    });
    QObject::connect(m_uiManager, &UIManager::requestSendFile, this, [&](const QString& address, const QString& filePath) {
        if (m_bluetoothManager) {
            // m_bluetoothManager->sendFile(address.toStdString(), filePath.toStdString()); // Placeholder
            qDebug() << "BlueLinkApp: Requesting send file to" << address << ":" << filePath;
        }
    });
    QObject::connect(m_uiManager, &UIManager::requestShowScreen, this, [&](const QString& screenName) {
        qDebug() << "BlueLinkApp: Requesting show screen:" << screenName;
        // This signal can be used to switch screens in QML if needed, or for C++ logic
    });

    // Connect Bluetooth manager events to UI slots (example connections)
    // This part requires IBluetoothManager to have a way to emit events.
    // For now, we'll assume a direct call or a bridge will be implemented.
    // For example, if IBluetoothManager had a signal-emitting wrapper:
    // QObject::connect(bluetoothManagerWrapper, &BluetoothManagerWrapper::deviceDiscovered, m_uiManager, &UIManager::addDiscoveredDevice);
}

BlueLinkApp::~BlueLinkApp()
{
    delete m_uiManager;
}

void BlueLinkApp::start()
{
    m_uiManager->show();
}