#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>

class UIManager : public QObject
{
    Q_OBJECT

public:
    explicit UIManager(QObject *parent = nullptr);
    ~UIManager();

    void show();

signals:
    // Signals to communicate UI events to the BlueLinkApp
    void requestBluetoothDiscovery();
    void requestConnectDevice(const QString& address);
    void requestDisconnectDevice(const QString& address);
    void requestSendMessage(const QString& address, const QString& message);
    void requestSendFile(const QString& address, const QString& filePath);
    void requestShowScreen(const QString& screenName);

public slots:
    // Slots to be called from QML to trigger C++ actions
    void onDiscoverDevicesClicked();
    void onConnectDeviceClicked(const QString& address);
    void onDisconnectDeviceClicked(const QString& address);
    void onSendMessageClicked(const QString& address, const QString& message);
    void onSendFileClicked(const QString& address, const QString& filePath);
    void onScreenRequested(const QString& screenName);

private:
    QQmlApplicationEngine m_engine;
};

#endif // UI_MANAGER_H
