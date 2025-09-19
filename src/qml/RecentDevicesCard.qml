import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme

Rectangle {
    id: recentCard
    width: 120
    height: 120
    radius: Theme.cornerRadius
    color: Theme.color("surface")

    // Apply shadow effect
    layer.enabled: true
    layer.effect: DropShadow {
        anchors.fill: parent
        horizontalOffset: 0
        verticalOffset: 1
        radius: 3
        samples: 17
        color: "#1F000000"
        spread: 0.12
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gridUnit
        spacing: Theme.gridUnit / 2

        Rectangle {
            id: avatarPlaceholder
            width: 64
            height: 64
            radius: 32
            color: Theme.color("secondary")
            Layout.alignment: Qt.AlignHCenter
            
            Text {
                anchors.centerIn: parent
                text: "D" // "Device"
                color: "white"
                font.pixelSize: 32
            }
        }

        Text {
            id: deviceNameText
            text: "Device Name"
            font.pixelSize: Theme.fonts.caption
            font.weight: Theme.fonts.captionWeight
            color: Theme.color("text_primary")
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Button {
            Layout.fillWidth: true
            text: "Connect"
            font.pixelSize: Theme.fonts.caption
            background: Rectangle {
                color: parent.hovered ? Qt.lighter(Theme.color("primary"), 1.2) : Theme.color("primary")
                radius: Theme.cornerRadius / 2
            }
            contentItem: Text {
                text: parent.text
                font: parent.font
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                console.log("Quick connect to: " + deviceNameText.text);
                // TODO: Implement quick connect logic
            }
        }
    }
}
