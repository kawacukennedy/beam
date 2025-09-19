import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme
import MessageBubble
import InputField
import SendButton
import TypingIndicator

Rectangle {
    id: chatScreen
    color: Theme.color("background")
    Layout.fillWidth: true
    Layout.fillHeight: true

    property bool isTyping: false // Set to true to show typing indicator

    RowLayout {
        anchors.fill: parent

        // Left Panel (Device List for Chat)
        Rectangle {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            color: Theme.color("surface")
            Text {
                anchors.centerIn: parent
                text: "Chat Devices"
                color: Theme.color("text_primary")
            }
        }

        // Main Chat Panel
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.gridUnit
            padding: Theme.gridUnit

            // Message List
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.gridUnit
                model: 10 // Placeholder model
                delegate: MessageBubble {
                    isOutgoing: index % 2 === 0
                    messageText: "This is a sample message " + index
                    senderName: index % 2 === 0 ? "Me" : "Other Device"
                    messageTime: "12:34 PM"
                    status: index % 3 === 0 ? "sent" : (index % 3 === 1 ? "read" : "sending")
                }
                ScrollIndicator.vertical: ScrollIndicator { }

                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: 150 }
                    NumberAnimation { property: "y"; from: 10; to: 0; duration: 150 }
                }
            }

            TypingIndicator {
                visible: isTyping
                Layout.alignment: Qt.AlignLeft
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gridUnit

                InputField { Layout.fillWidth: true }
                SendButton { }
            }
        }
    }
}
