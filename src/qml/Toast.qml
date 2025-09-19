import QtQuick
import QtQuick.Controls
import Theme

Rectangle {
    id: toast

    property string text: ""

    width: 250
    height: 50
    radius: 8
    color: Theme.color("surface")
    border.color: Theme.color("border")
    anchors.bottom: parent.bottom
    anchors.right: parent.right - 20

    property int duration: 4000

    opacity: 0
    y: parent.height // Start off-screen at the bottom

    Text {
        anchors.centerIn: parent
        text: toast.text
        color: Theme.color("text_primary")
    }

    Timer {
        id: lifeTimer
        interval: toast.duration
        onTriggered: hide()
    }

    function show(message) {
        toast.text = message;
        entryAnimation.start();
        lifeTimer.start();
    }

    function hide() {
        exitAnimation.start();
    }

    ParallelAnimation {
        id: entryAnimation
        PropertyAnimation { target: toast; property: "opacity"; to: 1.0; duration: 200 }
        PropertyAnimation { target: toast; property: "y"; to: parent.height - height - 20; duration: 200; easing.type: Easing.OutCubic }
    }

    PropertyAnimation {
        id: exitAnimation
        target: toast
        property: "opacity"
        to: 0.0
        duration: 150
        onFinished: visible = false
    }
}
