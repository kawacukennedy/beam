import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme

Rectangle {
    id: sidebar
    
    property bool collapsed: false
    width: collapsed ? 60 : 240
    color: Theme.color("surface")
    Layout.fillHeight: true

    Behavior on width {
        NumberAnimation {
            duration: 200
            easing.type: Easing.InOutCubic
        }
    }

    property var contentStack: null // Property to hold the StackLayout reference

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: collapsed ? 0 : Theme.gridUnit * 2
        spacing: Theme.gridUnit

        Button {
            id: collapseButton
            Layout.alignment: Qt.AlignRight
            text: collapsed ? ">" : "<"
            onClicked: sidebar.collapsed = !sidebar.collapsed
            font.pixelSize: Theme.fonts.h2
            background: Rectangle { color: "transparent" }
        }

        Label {
            text: "BlueLink"
            font.pixelSize: Theme.fonts.h1
            font.weight: Theme.fonts.h1Weight
            color: Theme.color("primary")
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            padding: Theme.gridUnit
            visible: !collapsed
        }

        Repeater {
            model: ["Dashboard", "Devices", "Chat", "File Transfer", "Settings"]
            delegate: Button {
                Layout.fillWidth: true
                text: modelData
                font.pixelSize: Theme.fonts.body
                font.weight: Theme.fonts.bodyWeight
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: parent.hovered ? Theme.color("primary") : Theme.color("text_primary")
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: Theme.gridUnit * 2
                    visible: !sidebar.collapsed
                }
                background: Rectangle {
                    color: parent.hovered ? Theme.color("sidebar_hover") : Theme.color("surface")
                    radius: Theme.cornerRadius / 2
                }
                height: Theme.gridUnit * 6
                onClicked: {
                    if (sidebar.contentStack) {
                        switch(modelData) {
                            case "Dashboard": sidebar.contentStack.currentIndex = 0; break;
                            case "Devices": sidebar.contentStack.currentIndex = 1; break;
                            case "Chat": sidebar.contentStack.currentIndex = 2; break;
                            case "File Transfer": sidebar.contentStack.currentIndex = 3; break;
                            case "Settings": sidebar.contentStack.currentIndex = 4; break;
                        }
                    }
                    console.log("Navigate to: " + modelData);
                }
            }
        }
    }
}
