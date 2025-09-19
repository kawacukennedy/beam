import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

WizardPage {
    id: welcomePage
    title: qsTr("Welcome to BlueConnect Desktop")

    Image {
        id: logoImage
        source: "qrc:///resources/icons/icon.png"
        x: 220
        y: 60
        width: 200
        height: 200
        fillMode: Image.PreserveAspectFit

        OpacityAnimator {
            target: logoImage
            from: 0.0
            to: 1.0
            duration: 1000 // 1s fade_in
            running: true
        }

        // Pulse animation
        SequentialAnimation on scale {
            id: logoPulseAnimation
            running: true
            loops: Animation.Infinite
            PropertyAnimation { from: 1.0; to: 1.05; duration: 250; easing.type: Easing.InOutQuad }
            PropertyAnimation { from: 1.05; to: 1.0; duration: 250; easing.type: Easing.InOutQuad }
        }
    }

    Label {
        id: taglineLabel
        text: qsTr("Connect & Share Instantly")
        x: 200
        y: 280
        font.pixelSize: 21 // Adjusted to ~16pt
        font.bold: true
        color: "#1E90FF"

        OpacityAnimator {
            target: taglineLabel
            from: 0.0
            to: 1.0
            duration: 300 // 0.3s fade_in
            running: true
        }
    }

    Label {
        id: versionLabel
        text: qsTr("v1.0.0")
        x: 550
        y: 20
        font.pixelSize: 13 // 10pt
        color: "#888888"
    }

    // Custom Next Button as per JSON specification
    Button {
        id: nextButton
        text: qsTr("Next")
        x: 540
        y: 420
        width: 80
        height: 30
        enabled: false // As per JSON: "Next enabled only when EULA scroll completed"
        tooltip: qsTr("Click Next to continue")
        focus: true // Explicitly set focus for accessibility

        // Basic hover effect
        background: Rectangle {
            color: nextButton.pressed ? "#007BFF" : (nextButton.hovered ? "#4CAF50" : "#1E90FF")
            radius: 5
            border.color: nextButton.focus ? "#FFD700" : "transparent" // Focus indicator
            border.width: nextButton.focus ? 2 : 0
            Behavior on color { ColorAnimation { duration: 100 } }
        }
        contentItem: Label {
            text: nextButton.text
            font.pixelSize: 14
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        // Placeholder for action, actual navigation will be handled by framework or controller
        onClicked: {
            // installer.currentPage.complete = true; // Example of how to signal completion to the framework
        }

        // Hover scale animation
        Behavior on scale {
            NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
        }
        onHoveredChanged: {
            if (hovered) {
                scale = 1.05;
            } else {
                scale = 1.0;
            }
        }
    }
}}