import QtQuick
import QtQuick.Window
import "Theme.qml" as Theme

Window {
    width: 640
    height: 480
    visible: true
    title: qsTr("BlueLink Manager")

    // Access theme properties
    color: Theme.color("background")

    Rectangle {
        anchors.fill: parent
        color: Theme.color("surface")
        radius: Theme.cornerRadius

        Text {
            anchors.centerIn: parent
            text: "Welcome to BlueLink Manager (QML)"
            font.pixelSize: Theme.fonts.h1
            font.weight: Theme.fonts.h1Weight
            color: Theme.color("text_primary")
        }
    }
}