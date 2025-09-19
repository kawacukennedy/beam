#ifndef BLUELINK_APP_H
#define BLUELINK_APP_H

#include <QObject>
#include <memory>
#include <QVariant>

// Forward declarations
class UIManager;
struct IBluetoothManager;
class DatabaseManager;

class BlueLinkApp : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isBluetoothPoweredOn READ isBluetoothPoweredOn NOTIFY bluetoothPoweredOnChanged)

public:
    explicit BlueLinkApp(QObject *parent = nullptr);
    ~BlueLinkApp();

    void start();

    bool isBluetoothPoweredOn() const { return m_isBluetoothPoweredOn; }

public slots:
    void startScan();
    void stopScan();
    void connectDevice(const QString &deviceAddress);
    void disconnectDevice(const QString &deviceAddress);
    void sendMessage(const QString &deviceAddress, const QString &message);
    void sendFile(const QString &deviceAddress, const QString &filePath);

signals:
    void bluetoothPoweredOnChanged();
    void deviceDiscovered(const QString &name, const QString &address, int rssi);
    void deviceConnected(const QString &address, bool connected);
    void messageReceived(const QString &deviceAddress, const QString &message);
    void fileTransferProgress(const QString &deviceAddress, const QString &fileName, double progress);
    void fileTransferFinished(const QString &deviceAddress, const QString &fileName, bool success);
    void fileTransferError(const QString &deviceAddress, const QString &fileName, const QString &error);
    void showAlert(const QString &title, const QString &message);
    void clearDiscoveredDevices();
    void addMessageBubble(const QString &deviceAddress, const QString &message, bool isOutgoing);
    void addFileTransferItem(const QString &deviceAddress, const QString &fileName, bool isSending);
    void removeFileTransferItem(const QString &deviceAddress, const QString &fileName);

private:
    UIManager* m_uiManager;
    IBluetoothManager* m_bluetoothManager; // Raw pointer, managed by platform-specific getter
    DatabaseManager* m_databaseManager;
    bool m_isBluetoothPoweredOn;

    // Callback functions for C Bluetooth manager
    static void staticShowAlert(const char* title, const char* message);
    static void staticClearDiscoveredDevices();
    static void staticAddDiscoveredDevice(const char* name, const char* address, int rssi);
    static void staticUpdateDeviceConnectionStatus(const char* address, bool connected);
    static void staticAddMessageBubble(const char* deviceAddress, const char* message, bool isOutgoing);
    static void staticUpdateFileTransferProgress(const char* deviceAddress, const char* fileName, double progress);
    static void staticAddFileTransferItem(const char* deviceAddress, const char* fileName, bool isSending);
    static void staticRemoveFileTransferItem(const char* deviceAddress, const char* fileName);

    // Helper to emit signals from static callbacks
    void emitDeviceDiscovered(const QString &name, const QString &address, int rssi);
    void emitDeviceConnected(const QString &address, bool connected);
    void emitMessageReceived(const QString &deviceAddress, const QString &message);
    void emitFileTransferProgress(const QString &deviceAddress, const QString &fileName, double progress);
    void emitFileTransferFinished(const QString &deviceAddress, const QString &fileName, bool success);
    void emitFileTransferError(const QString &deviceAddress, const QString &fileName, const QString &error);
    void emitShowAlert(const QString &title, const QString &message);
    void emitClearDiscoveredDevices();
    void emitAddMessageBubble(const QString &deviceAddress, const QString &message, bool isOutgoing);
    void emitAddFileTransferItem(const QString &deviceAddress, const QString &fileName, bool isSending);
    void emitRemoveFileTransferItem(const QString &deviceAddress, const QString &fileName);

    // Private setter for isBluetoothPoweredOn
    void setIsBluetoothPoweredOn(bool poweredOn);
};

#endif // BLUELINK_APP_H
