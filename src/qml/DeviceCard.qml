import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme

Rectangle {
    id: deviceCard
    width: 220
    height: 130
    radius: Theme.cornerRadius
    color: Theme.color("surface")

    // Apply shadow effect
    layer.enabled: true
    layer.effect: Theme.shadows.low

    property string deviceName: "Unknown Device"
    property string deviceMac: "00:00:00:00:00:00"
    property int deviceRssi: 0
    property bool devicePaired: false
    property bool deviceConnected: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gridUnit
        spacing: Theme.gridUnit / 2

        Text {
            text: deviceName
            font.pixelSize: Theme.fonts.body
            font.weight: Theme.fonts.bodyWeight
            color: Theme.color("text_primary")
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Text {
            text: deviceMac
            font.pixelSize: Theme.fonts.caption
            color: Theme.color("text_secondary")
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "RSSI: " + deviceRssi + " dBm"
                font.pixelSize: Theme.fonts.caption
                color: Theme.color("text_secondary")
            }

            Text {
                text: devicePaired ? "Paired" : "Not Paired"
                font.pixelSize: Theme.fonts.caption
                color: devicePaired ? Theme.color("success") : Theme.color("error")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Button {
                text: "Connect"
                visible: !deviceConnected
                font.pixelSize: Theme.fonts.caption
                onClicked: { console.log("Connect to: " + deviceName); }
            }
            Button {
                text: "Disconnect"
                visible: deviceConnected
                font.pixelSize: Theme.fonts.caption
                onClicked: { console.log("Disconnect from: " + deviceName); }
            }
            Button {
                text: "Pair"
                visible: !devicePaired
                font.pixelSize: Theme.fonts.caption
                onClicked: { console.log("Pair with: " + deviceName); }
            }
            Button {
                text: "Unpair"
                visible: devicePaired
                font.pixelSize: Theme.fonts.caption
                onClicked: { console.log("Unpair from: " + deviceName); }
            }
        }
    }
}
