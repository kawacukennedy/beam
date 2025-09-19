import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme

Rectangle {
    id: generalSettings
    color: Theme.color("background")
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gridUnit * 2
        spacing: Theme.gridUnit * 2

        Label {
            text: "General Settings"
            font.pixelSize: Theme.fonts.h2
            font.weight: Theme.fonts.h2Weight
            color: Theme.color("text_primary")
        }

        // Language Setting
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Language:"; font.pixelSize: Theme.fonts.body; color: Theme.color("text_primary") }
            ComboBox {
                Layout.fillWidth: true
                model: ["English", "Spanish", "French"]
                currentIndex: 0 // TODO: Bind to actual setting
                onCurrentIndexChanged: console.log("Language: " + model[currentIndex]);
            }
        }

        // Theme Setting
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Theme:"; font.pixelSize: Theme.fonts.body; color: Theme.color("text_primary") }
            ComboBox {
                Layout.fillWidth: true
                model: ["Light", "Dark"]
                currentIndex: Theme.currentTheme === "light" ? 0 : 1 // Bind to actual setting
                onCurrentIndexChanged: {
                    Theme.currentTheme = currentIndex === 0 ? "light" : "dark";
                    console.log("Theme: " + Theme.currentTheme);
                }
            }
        }

        // Startup Behavior Setting
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Startup Behavior:"; font.pixelSize: Theme.fonts.body; color: Theme.color("text_primary") }
            ComboBox {
                Layout.fillWidth: true
                model: ["Start minimized", "Start normally", "Don't start with system"]
                currentIndex: 1 // TODO: Bind to actual setting
                onCurrentIndexChanged: console.log("Startup Behavior: " + model[currentIndex]);
            }
        }
    }
}