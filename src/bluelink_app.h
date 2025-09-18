#ifndef BLUELINK_APP_H
#define BLUELINK_APP_H

#include <QObject>
#include <memory>

// Forward declarations
class UIManager;
class IBluetoothManager;
class DatabaseManager;

class BlueLinkApp : public QObject
{
    Q_OBJECT

public:
    explicit BlueLinkApp(QObject *parent = nullptr);
    ~BlueLinkApp();

    void start();

private:
    UIManager* m_uiManager;
    std::unique_ptr<IBluetoothManager> m_bluetoothManager;
    // DatabaseManager* m_databaseManager; // To be added later
};

#endif // BLUELINK_APP_H
