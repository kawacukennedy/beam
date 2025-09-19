import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

WizardPage {
    id: preInstallationChecksPage
    title: qsTr("Pre-installation Checks")

    property alias statusText: statusLabel.text
    property alias detailedMessages: detailedMessagesArea.text
    property alias progressValue: progressBarFill.width // Bind to the fill width for custom progress bar
    property alias progressMaximum: progressBarRect.width // Bind to the total width

    // New properties for error handling
    property bool showErrorModal: false
    property string errorMessage: ""

    // Status Label
    Label {
        id: statusLabel
        x: 50
        y: 420
        width: 540 // Matches progress bar width
        text: qsTr("Initializing checks...")
        font.pixelSize: 16 // 12pt
        color: "#000000"
    }

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
            width: 0 // Will be animated
            height: parent.height
            anchors.left: parent.left

            gradient: LinearGradient {
                id: progressGradient
                start: QtPoint(0, 0)
                end: QtPoint(progressBarFill.width, 0)
                GradientStop { position: 0.0; color: "#1E90FF" }
                GradientStop { position: 1.0; color: "#00BFFF" }
            }

            // Indeterminate animation
            SequentialAnimation on x {
                id: indeterminateAnimation
                running: preInstallationChecksPage.progressValue === 0 // Run when progress is 0
                loops: Animation.Infinite
                PropertyAnimation { from: -progressBarFill.width; to: parent.width; duration: 1500; easing.type: Easing.Linear }
            }

            // Actual progress animation (will override indeterminate when value is set)
            Behavior on width {
                NumberAnimation { duration: 500; easing.type: Easing.OutQuad }
            }
        }
    }

    // Detailed Messages Area
    TextArea {
        id: detailedMessagesArea
        x: 50
        y: 440
        width: 540
        height: 180 // Adjusted height to fit within 640x480 window, assuming some padding
        text: ""
        font.pixelSize: 13 // 10pt
        color: "#333333"
        readOnly: true
        wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
        // Scrollable by default
    }

    property bool complete: false

    // New MessageDialog for error handling
    MessageDialog {
        id: errorDialog
        visible: preInstallationChecksPage.showErrorModal // Use the page's property
        title: qsTr("Pre-installation Check Failed")
        text: preInstallationChecksPage.errorMessage // Display the error message from the page
        icon: StandardIcon.Critical

        standardButtons: StandardButton.Retry | StandardButton.Abort // Abort for Rollback

        onButtonClicked: function(button) {
            if (button === StandardButton.Retry) {
                preInstallationChecksPage.showErrorModal = false;
                // Signal to install.qs to re-run checks
                installer.setValue("PreInstallationChecksRetry", true);
            } else if (button === StandardButton.Abort) {
                installer.setValue("InstallationAborted", true); // Custom flag
                installer.abort(); // Abort installation
            }
        }
    }
}