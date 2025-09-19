import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme

Rectangle {
    id: adapterCard
    width: 300
    height: 160
    radius: Theme.cornerRadius
    color: Theme.color("surface")

    // Apply shadow effect
    layer.enabled: true
    layer.effect: Theme.shadows.low

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gridUnit * 2
        spacing: Theme.gridUnit

        Text {
            id: nameText
            text: "Bluetooth Adapter"
            font.pixelSize: Theme.fonts.h2
            font.weight: Theme.fonts.h2Weight
            color: Theme.color("text_primary")
            Layout.fillWidth: true
        }

        Text {
            id: macText
            text: "MAC: 00:11:22:33:44:55"
            font.pixelSize: Theme.fonts.body
            color: Theme.color("text_secondary")
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gridUnit

            Text {
                text: "Status:"
                font.pixelSize: Theme.fonts.body
                color: Theme.color("text_primary")
            }

            Rectangle {
                id: ledIndicator
                width: Theme.gridUnit
                height: Theme.gridUnit
                radius: width / 2
                color: bluetoothToggle.checked ? Theme.color("success") : Theme.color("error")
                Layout.alignment: Qt.AlignVCenter

                Behavior on color {
                    ColorAnimation { duration: 200 }
                }
                
                SequentialAnimation {
                    id: statusChangeAnimation
                    running: false
                    PropertyAnimation { target: ledIndicator; property: "scale"; to: 1.5; duration: 100 }
                    PropertyAnimation { target: ledIndicator; property: "scale"; to: 1.0; duration: 100 }
                }
            }

            Switch {
                id: bluetoothToggle
                text: bluetoothToggle.checked ? "On" : "Off"
                checked: true // TODO: Bind to actual adapter status
                onCheckedChanged: {
                    statusChangeAnimation.start();
                    console.log("Adapter Toggled: " + checked);
                    // TODO: Call C++ backend to enable/disable Bluetooth adapter
                }
            }
        }
    }
}