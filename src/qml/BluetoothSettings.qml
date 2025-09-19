import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme

Rectangle {
    id: bluetoothSettings
    color: Theme.color("background")
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gridUnit * 2
        spacing: Theme.gridUnit * 2

        Label {
            text: "Bluetooth Preferences"
            font.pixelSize: Theme.fonts.h2
            font.weight: Theme.fonts.h2Weight
            color: Theme.color("text_primary")
        }

        // Auto-Connect Setting
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Auto-Connect to Paired Devices:"; font.pixelSize: Theme.fonts.body; color: Theme.color("text_primary") }
            Switch {
                checked: true // TODO: Bind to actual setting
                onCheckedChanged: console.log("Auto-Connect: " + checked);
            }
        }

        // Device Priority Setting
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Prioritize Devices:"; font.pixelSize: Theme.fonts.body; color: Theme.color("text_primary") }
            ComboBox {
                Layout.fillWidth: true
                model: ["Last Connected", "Strongest Signal", "Alphabetical"]
                currentIndex: 0 // TODO: Bind to actual setting
                onCurrentIndexChanged: console.log("Device Priority: " + model[currentIndex]);
            }
        }
    }
}