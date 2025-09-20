import QtQuick 2.12
import QtQuick.Controls 2.12

Page {
    id: welcomePage
    title: qsTr("Welcome to BlueConnect Desktop")

    // As per JSON, window is not resizable.
    // This is set in config.xml, but we can re-iterate here for clarity.
    property int windowWidth: 640
    property int windowHeight: 480

    background: Rectangle {
        color: "#FFFFFF"
    }

    Image {
        id: logo
        x: 220
        y: 60
        width: 200
        height: 200
        source: "qrc:///resources/icons/icon.png"
        fillMode: Image.PreserveAspectFit

        // Fade-in animation
        OpacityAnimator on opacity { from: 0; to: 1.0; duration: 1000; easing.type: Easing.InOutCubic; running: true }

        // Pulse animation
        SequentialAnimation on scale {
            loops: Animation.Infinite
            running: true
            PropertyAnimation { to: 1.05; duration: 500; easing.type: Easing.InOutQuad }
            PropertyAnimation { to: 1.0; duration: 500; easing.type: Easing.InOutQuad }
        }
    }

    Text {
        id: tagline
        text: qsTr("Connect & Share Instantly")
        x: 200
        y: 280
        font.family: "System Default"
        font.pointSize: 16
        font.bold: true
        color: "#1E90FF"

        // Fade-in animation
        OpacityAnimator on opacity { from: 0; to: 1.0; duration: 300; running: true }
    }

    Text {
        id: versionLabel
        text: "v" + installer.value("Version")
        x: 550
        y: 20
        font.family: "System Default"
        font.pointSize: 10
        color: "#888888"
    }

    Button {
        id: nextButton
        x: 540
        y: 420
        width: 80
        height: 30
        text: qsTr("Next")
        enabled: true // Per discussion, enabling this by default for better UX.

        ToolTip.visible: hovered
        ToolTip.text: qsTr("Click Next to continue")

        background: Rectangle {
            color: parent.enabled ? (parent.pressed ? "#0056b3" : (parent.hovered ? "#0069d9" : "#1E90FF")) : "#E0E0E0"
            radius: 4
            border.color: parent.focus ? "#FFD700" : "transparent"
            border.width: 2
            Behavior on color { ColorAnimation { duration: 200 } }
        }

        contentItem: Text {
            text: parent.text
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pointSize: 10
        }

        onClicked: {
            installer.executeLater(function() { gui.clickButton(buttons.NextButton); });
        }

        // Hover effect
        transform: Scale {
            origin.x: nextButton.width / 2
            origin.y: nextButton.height / 2
            xScale: nextButton.hovered ? 1.05 : 1.0
            yScale: nextButton.hovered ? 1.05 : 1.0
            Behavior on xScale { NumberAnimation { duration: 150 } }
            Behavior on yScale { NumberAnimation { duration: 150 } }
        }
    }
}
