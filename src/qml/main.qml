import QtQuick
import QtQuick.Window

ApplicationWindow {
    width: 640
    height: 480
    visible: true
    title: qsTr("Simplified BlueLink Manager")

    Rectangle {
        anchors.fill: parent
        color: "lightblue"
        Text {
            anchors.centerIn: parent
            text: "Hello from Simplified QML!"
            font.pixelSize: 24
        }
    }
}