import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import Theme
import SideBar
import TopBar

ApplicationWindow {
    id: appWindow
    width: 1280
    height: 800
    visible: true
    title: qsTr("BlueLink Manager")

    // Access theme properties
    color: Theme.color("background")

    header: TopBar { id: topBar }

    RowLayout {
        anchors.fill: parent

        SideBar { id: sideBar; contentStack: contentStack }

        StackLayout {
            id: contentStack
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Dashboard Screen
            DashboardScreen { }

            // Devices Screen
            DeviceListScreen { }

            // Chat Screen
            ChatScreen { }

            // File Transfer Screen
            FileTransferScreen { }

            // Settings Screen
            SettingsScreen { }
        }
    }
}