#include "ui_manager.h"
#include "bluelink_app.h" // Include BlueLinkApp header
#include <QQmlContext>
#include <QQuickWindow>
#include <QDebug>

UIManager::UIManager(QObject *parent) : 
    QObject(parent),
    m_blueLinkApp(nullptr) // Initialize to nullptr
{
    // No direct connections here anymore, QML will call methods on blueLinkApp
}

UIManager::~UIManager()
{
}

void UIManager::setBlueLinkApp(BlueLinkApp* app)
{
    if (m_blueLinkApp != app) {
        m_blueLinkApp = app;
        m_engine.rootContext()->setContextProperty("blueLinkApp", m_blueLinkApp);
        emit blueLinkAppChanged();
    }
}

void UIManager::show()
{
    m_engine.load(QUrl(QStringLiteral("qrc:/src/qml/main.qml")));
    if (m_engine.rootObjects().isEmpty()) {
        qCritical("Error: No root objects in QML engine. Check qrc:/src/qml/main.qml");
    } else {
        QQuickWindow *window = qobject_cast<QQuickWindow*>(m_engine.rootObjects().first());
        if (window) {
            window->show();
        }
    }
}

void UIManager::onDiscoverDevicesClicked() {
    emit requestBluetoothDiscovery();
}

void UIManager::onConnectDeviceClicked(const QString& address) {
    emit requestConnectDevice(address);
}

void UIManager::onDisconnectDeviceClicked(const QString& address) {
    emit requestDisconnectDevice(address);
}

void UIManager::onSendMessageClicked(const QString& address, const QString& message) {
    emit requestSendMessage(address, message);
}

void UIManager::onSendFileClicked(const QString& address, const QString& filePath) {
    emit requestSendFile(address, filePath);
}

void UIManager::onScreenRequested(const QString& screenName) {
    emit requestShowScreen(screenName);
}