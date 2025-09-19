import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme

Rectangle {
    id: inputField

    property bool hasFocus: messageInput.focus
    property bool hasError: false

    Layout.fillWidth: true
    height: 40
    radius: 8 // spec radius for button is 8, let's use it here for consistency
    border.width: hasFocus ? 2 : 1
    border.color: {
        if (hasError) return Theme.color("error");
        if (hasFocus) return Theme.color("primary");
        return Theme.color("border");
    }

    // Glow effect for focus
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        border.width: hasFocus ? 2 : 0
        border.color: Qt.rgba(Theme.color("primary").r, Theme.color("primary").g, Theme.color("primary").b, 0.3)
        visible: hasFocus
        color: "transparent"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: Theme.gridUnit

        TextField {
            id: messageInput
            placeholderText: "Type a message..."
            Layout.fillWidth: true
            font.pixelSize: Theme.fonts.body
            color: Theme.color("text_primary")
            background: Rectangle { color: "transparent" }
            onAccepted: {
                // TODO: Emit signal to send message
                console.log("Message sent: " + text);
                messageInput.text = "";
            }
        }

        Button {
            text: "😀"
            font.pixelSize: Theme.fonts.body
            background: Rectangle { color: "transparent" }
            onClicked: {
                console.log("Emoji button clicked");
                // TODO: Show emoji picker
            }
        }

        Button {
            text: "📎"
            font.pixelSize: Theme.fonts.body
            background: Rectangle { color: "transparent" }
            onClicked: {
                console.log("File send button clicked");
                // TODO: Open file picker
            }
        }
    }

    // Shake animation for error
    SequentialAnimation {
        id: shakeAnimation
        running: false
        loops: 1
        NumberAnimation { target: inputField; property: "x"; from: inputField.x - 3; to: inputField.x + 3; duration: 50; easing.type: Easing.InOutSine }
        NumberAnimation { target: inputField; property: "x"; from: inputField.x + 3; to: inputField.x - 3; duration: 50; easing.type: Easing.InOutSine }
        NumberAnimation { target: inputField; property: "x"; from: inputField.x - 3; to: inputField.x + 3; duration: 50; easing.type: Easing.InOutSine }
        NumberAnimation { target: inputField; property: "x"; from: inputField.x + 3; to: inputField.x; duration: 50; easing.type: Easing.InOutSine }
    }

    onHasErrorChanged: {
        if (hasError) {
            shakeAnimation.start();
            // Reset error state after animation
            errorTimer.start();
        }
    }

    Timer {
        id: errorTimer
        interval: 500
        onTriggered: hasError = false
    }
    
    Behavior on border.color { ColorAnimation { duration: 150 } }
    Behavior on border.width { NumberAnimation { duration: 150 } }
}
