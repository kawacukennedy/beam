import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Page {
    id: optionalFeaturesPage
    title: qsTr("Optional Features")

    Component.onCompleted: {
        // This page is always complete
        optionalFeaturesPage.complete = true;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        Label {
            text: qsTr("Select the optional features you want to install.")
            font.pointSize: 12
        }

        CheckBox {
            id: autoStartCheckbox
            text: qsTr("Auto-start Bluetooth service")
            checked: installer.componentByName("AutoStart").selected
            onCheckedChanged: installer.componentByName("AutoStart").selected = checked
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Enable background service on startup")
        }

        CheckBox {
            id: startMenuCheckbox
            text: qsTr("Start menu shortcut")
            checked: installer.componentByName("StartMenu").selected
            onCheckedChanged: installer.componentByName("StartMenu").selected = checked
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Add app to start menu")
        }

        CheckBox {
            id: desktopShortcutCheckbox
            text: qsTr("Desktop shortcut")
            checked: installer.componentByName("DesktopShortcut").selected
            onCheckedChanged: installer.componentByName("DesktopShortcut").selected = checked
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Create desktop icon")
        }
    }
}
