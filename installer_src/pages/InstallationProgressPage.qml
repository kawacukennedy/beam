import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12

WizardPage {
    id: installationProgressPage
    title: qsTr("Installation Progress")

    property int currentProgress: 0
    property int totalSteps: 100
    property int elapsedTime: 0 // in seconds
    property bool installationRunning: false // This will be controlled by install.qs now
    property bool showErrorModal: false
    property string errorMessage: ""

    // Animated Gradient Progress Bar
    Rectangle {
        id: progressBarRect
        x: 50
        y: 400
        width: 540
        height: 20
        radius: 5
        border.color: "#CCCCCC"
        border.width: 1
        clip: true

        Rectangle {
            id: progressBarFill
            width: (installationProgressPage.currentProgress / installationProgressPage.totalSteps) * parent.width
            height: parent.height
            anchors.left: parent.left

            gradient: LinearGradient {
                id: progressGradient
                start: QtPoint(0, 0)
                end: QtPoint(progressBarFill.width, 0)
                GradientStop { position: 0.0; color: "#1E90FF" }
                GradientStop { position: 1.0; color: "#00BFFF" }
            }

            // Indeterminate animation (runs when installation is running and progress is 0)
            SequentialAnimation on x {
                id: indeterminateAnimation
                running: installationProgressPage.installationRunning && installationProgressPage.currentProgress === 0
                loops: Animation.Infinite
                PropertyAnimation { from: -progressBarFill.width; to: parent.width; duration: 1500; easing.type: Easing.Linear }
            }

            Behavior on width {
                NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
            }
        }
    }

    // Percentage Label
    Label {
        id: percentageLabel
        x: 50
        y: 420
        font.pixelSize: 16 // 12pt
        color: "#000000"
        text: qsTr("%1%").arg(Math.round((installationProgressPage.currentProgress / installationProgressPage.totalSteps) * 100))
    }

    // Elapsed Time Label
    Label {
        id: elapsedTimeLabel
        x: 50
        y: 440
        font.pixelSize: 16 // 12pt
        color: "#555555"
        text: qsTr("Elapsed Time: %1s").arg(installationProgressPage.elapsedTime)
    }

    // Log Window
    TextArea {
        id: logWindow
        x: 50
        y: 200
        width: 540
        height: 180
        font.pixelSize: 13 // 10pt
        color: "#333333"
        readOnly: true
        wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
        // Scrollable by default
    }

    // Cancel Button
    Button {
        id: cancelButton
        text: qsTr("Cancel")
        x: 540
        y: 460
        width: 80
        height: 30
        enabled: installationProgressPage.installationRunning // Enabled when installation is running
        tooltip: qsTr("Cancel installation (disabled during critical operations)")
        focus: true // Explicitly set focus for accessibility

        background: Rectangle {
            color: cancelButton.pressed ? "#DC3545" : (cancelButton.hovered ? "#C82333" : "#FF0000")
            radius: 5
            border.color: cancelButton.focus ? "#FFD700" : "transparent"
            border.width: cancelButton.focus ? 2 : 0
            Behavior on color { ColorAnimation { duration: 100 } }
        }
        contentItem: Label {
            text: cancelButton.text
            font.pixelSize: 14
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        onClicked: {
            // In a real scenario, this would signal the installer to stop
            installationProgressPage.installationRunning = false;
            logWindow.append(qsTr("\nInstallation cancelled by user."));
            // Optionally, show a confirmation dialog
            // installer.abort(); // Abort installation
        }
    }

    // Error Handling Message Dialog
    MessageDialog {
        id: errorDialog
        visible: installationProgressPage.showErrorModal
        title: qsTr("Installation Error")
        text: installationProgressPage.errorMessage
        icon: StandardIcon.Critical

        standardButtons: StandardButton.Retry | StandardButton.Abort

        onButtonClicked: function(button) {
            if (button === StandardButton.Retry) {
                installationProgressPage.showErrorModal = false;
                // Signal to install.qs to retry installation
                installer.setValue("InstallationRetry", true);
            } else if (button === StandardButton.Abort) {
                installer.setValue("InstallationAborted", true);
                installer.abort(); // Abort installation
            }
        }
    }

    // Timer for simulating elapsed time (can be removed if actual time is tracked by installer.qs)
    Timer {
        id: elapsedTimeTimer
        interval: 1000 // Update every second
        running: installationProgressPage.installationRunning
        repeat: true
        onTriggered: {
            installationProgressPage.elapsedTime++;
        }
    }

    property bool complete: false
}
