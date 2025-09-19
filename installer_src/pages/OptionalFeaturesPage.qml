import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

WizardPage {
    id: optionalFeaturesPage
    title: qsTr("Optional Features")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 50
        spacing: 10

        CheckBox {
            id: autoStartServiceCheckbox
            text: qsTr("Auto-start Bluetooth service")
            checked: true
            tooltip: qsTr("Enable background service on startup")
            focus: true
            onCheckedChanged: {
                installer.setValue("AutoStartService", checked);
            }
        }

        CheckBox {
            id: startMenuShortcutCheckbox
            text: qsTr("Start menu shortcut")
            checked: false
            tooltip: qsTr("Add app to start menu")
            focus: true // Explicitly set focus for accessibility
            onCheckedChanged: {
                installer.setValue("StartMenuShortcut", checked);
            }
        }

        CheckBox {
            id: desktopShortcutCheckbox
            text: qsTr("Desktop shortcut")
            checked: false
            tooltip: qsTr("Create desktop icon")
            focus: true // Explicitly set focus for accessibility
            onCheckedChanged: {
                installer.setValue("DesktopShortcut", checked);
            }
        }
    }

    property bool complete: true
}