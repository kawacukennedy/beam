import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme
import GeneralSettings
import BluetoothSettings
import AdvancedSettings

Rectangle {
    id: settingsScreen
    color: Theme.color("background")
    Layout.fillWidth: true
    Layout.fillHeight: true

    RowLayout {
        anchors.fill: parent

        // Settings Navigation
        ListView {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            spacing: Theme.gridUnit
            model: ["General", "Bluetooth Preferences", "Advanced"]
            delegate: Button {
                Layout.fillWidth: true
                text: modelData
                font.pixelSize: Theme.fonts.body
                background: Rectangle {
                    color: parent.hovered ? Theme.color("background") : Theme.color("surface")
                    radius: Theme.cornerRadius / 2
                }
                height: Theme.gridUnit * 6
                onClicked: {
                    settingsStack.currentIndex = index;
                }
            }
        }

        // Settings Content
        StackLayout {
            id: settingsStack
            Layout.fillWidth: true
            Layout.fillHeight: true

            GeneralSettings { }
            BluetoothSettings { }
            AdvancedSettings { }
        }
    }
}