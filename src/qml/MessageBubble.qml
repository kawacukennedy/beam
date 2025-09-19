import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme

Rectangle {
    id: messageBubble
    property bool isOutgoing: false
    property string messageText: ""
    property string senderName: ""
    property string messageTime: ""
    property string status: "" // "sending", "sent", "read"

    implicitWidth: messageTextLabel.implicitWidth + Theme.gridUnit * 4
    implicitHeight: messageTextLabel.implicitHeight + Theme.gridUnit * 2
    radius: 16
    color: isOutgoing ? Theme.color("primary") : "#E6E6E6"

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

        Text {
            id: senderLabel
            text: senderName
            font.pixelSize: Theme.fonts.caption
            font.weight: Theme.fonts.captionWeight
            color: Theme.color("primary")
            visible: !isOutgoing // Only show sender for incoming messages
        }

        Text {
            id: messageTextLabel
            text: messageText
            font.pixelSize: Theme.fonts.body
            color: isOutgoing ? "white" : "black"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            
            Text {
                id: timeLabel
                text: messageTime
                font.pixelSize: Theme.fonts.caption
                color: isOutgoing ? Qt.lighter(Theme.color("primary"), 2.0) : Theme.color("text_secondary")
            }

            Item { Layout.fillWidth: true }

            Text {
                id: statusIcon
                text: {
                    if (status === "sending") return "⏳";
                    if (status === "sent") return "✓";
                    if (status === "read") return "✓✓";
                    return "";
                }
                font.pixelSize: Theme.fonts.caption
                color: status === "read" ? Theme.color("secondary") : (isOutgoing ? Qt.lighter(Theme.color("primary"), 2.0) : Theme.color("text_secondary"))
                visible: isOutgoing
            }
        }
    }
}
