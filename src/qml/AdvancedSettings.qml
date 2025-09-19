import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme

Rectangle {
    id: advancedSettings
    color: Theme.color("background")
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gridUnit * 2
        spacing: Theme.gridUnit * 2

        Label {
            text: "Advanced Settings"
            font.pixelSize: Theme.fonts.h2
            font.weight: Theme.fonts.h2Weight
            color: Theme.color("text_primary")
        }

        // Debug Logging Setting
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Enable Debug Logging:"; font.pixelSize: Theme.fonts.body; color: Theme.color("text_primary") }
            Switch {
                checked: false // TODO: Bind to actual setting
                onCheckedChanged: console.log("Debug Logging: " + checked);
            }
        }

        // Developer Mode Setting
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Developer Mode:"; font.pixelSize: Theme.fonts.body; color: Theme.color("text_primary") }
            Switch {
                checked: false // TODO: Bind to actual setting
                onCheckedChanged: console.log("Developer Mode: " + checked);
            }
        }
    }
}