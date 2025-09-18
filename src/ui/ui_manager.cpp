#include "ui_manager.h"
#include <QQmlContext>
#include <QQuickWindow>
#include <QDebug>

UIManager::UIManager(QObject *parent) : QObject(parent)
{
    // Register C++ object for QML interaction if needed
    // m_engine.rootContext()->setContextProperty("uiManager", this);

    // Connect signals from this UIManager to its slots
    // This is a self-connection to emit the signals that BlueLinkApp will connect to
    connect(this, &UIManager::requestBluetoothDiscovery, this, &UIManager::onDiscoverDevicesClicked);
    connect(this, &UIManager::requestConnectDevice, this, &UIManager::onConnectDeviceClicked);
    connect(this, &UIManager::requestDisconnectDevice, this, &UIManager::onDisconnectDeviceClicked);
    connect(this, &UIManager::requestSendMessage, this, &UIManager::onSendMessageClicked);
    connect(this, &UIManager::requestSendFile, this, &UIManager::onSendFileClicked);
    connect(this, &UIManager::requestShowScreen, this, &UIManager::onScreenRequested);
}

UIManager::~UIManager()
{
}

void UIManager::show()
{
    m_engine.load(QUrl(QStringLiteral("qrc:/src/qml/main.qml")));
    if (m_engine.rootObjects().isEmpty()) {
        qCritical("Error: No root objects in QML engine. Check qrc:/src/qml/main.qml");
    }
}

void UIManager::onDiscoverDevicesClicked()
{
    qDebug() << "UIManager: Discover Devices clicked (from QML).";
    emit requestBluetoothDiscovery();
}

void UIManager::onConnectDeviceClicked(const QString& address)
{
    qDebug() << "UIManager: Connect Device clicked (from QML) for:" << address;
    emit requestConnectDevice(address);
}

void UIManager::onDisconnectDeviceClicked(const QString& address)
{
    qDebug() << "UIManager: Disconnect Device clicked (from QML) for:" << address;
    emit requestDisconnectDevice(address);
}

void UIManager::onSendMessageClicked(const QString& address, const QString& message)
{
    qDebug() << "UIManager: Send Message clicked (from QML) to:" << address << ", message:" << message;
    emit requestSendMessage(address, message);
}

void UIManager::onSendFileClicked(const QString& address, const QString& filePath)
{
    qDebug() << "UIManager: Send File clicked (from QML) to:" << address << ", path:" << filePath;
    emit requestSendFile(address, filePath);
}

void UIManager::onScreenRequested(const QString& screenName)
{
    qDebug() << "UIManager: Screen requested (from QML):" << screenName;
    emit requestShowScreen(screenName);
}
