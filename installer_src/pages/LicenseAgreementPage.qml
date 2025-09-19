import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

WizardPage {
    id: licenseAgreementPage
    title: qsTr("License Agreement")

    // New property to track EULA scroll completion
    property bool eulaScrolledToEnd: false

    // Bind complete property to both checkbox and scroll state
    // This property is used by the framework's default 'Next' button.
    // If custom buttons are used, their enabled state must be managed separately.
    property bool complete: licenseCheckbox.checked && eulaScrolledToEnd

    Rectangle {
        x: 50
        y: 80
        width: 540
        height: 300
        border.color: "#CCCCCC"
        border.width: 1
        radius: 3

        ScrollView {
            id: eulaScrollView
            anchors.fill: parent
            clip: true // Ensure content outside bounds is clipped

            // Update eulaScrolledToEnd when scroll position changes
            onAtYEndChanged: {
                licenseAgreementPage.eulaScrolledToEnd = eulaScrollView.atYEnd;
            }

            TextEdit {
                id: eulaText
                width: eulaScrollView.width // TextEdit width should match ScrollView width
                readOnly: true
                textFormat: TextEdit.PlainText
                font.pixelSize: 16 // Approximately 12pt
                wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
                text: installer.loadText("resources/EULA.txt")
                // No need for internal ScrollBar.vertical as it's handled by ScrollView
            }
        }
    }

    CheckBox {
        id: licenseCheckbox
        x: 50
        y: 390
        text: qsTr("I accept the terms")
        tooltip: qsTr("Accept the license to enable Next button")
        focus: true
        checked: false // Ensure it's unchecked by default
        enabled: licenseAgreementPage.eulaScrolledToEnd // Only enable checkbox after scrolling
    }

    // Error message label
    Label {
        id: errorMessageLabel
        x: 50
        y: 415 // Position below checkbox
        color: "#FF0000"
        font.pixelSize: 14
        text: qsTr("You must accept the EULA to proceed.")
        visible: !licenseCheckbox.checked && !licenseAgreementPage.eulaScrolledToEnd // Show if not accepted and not scrolled
    }

    // Custom Back Button
    Button {
        id: backButton
        text: qsTr("Back")
        x: 440
        y: 420
        width: 80
        height: 30
        enabled: true
        focus: true // Explicitly set focus for accessibility

        background: Rectangle {
            color: backButton.pressed ? "#007BFF" : (backButton.hovered ? "#4CAF50" : "#1E90FF")
            radius: 5
            border.color: backButton.focus ? "#FFD700" : "transparent"
            border.width: backButton.focus ? 2 : 0
            Behavior on color { ColorAnimation { duration: 100 } }
        }
        contentItem: Label {
            text: backButton.text
            font.pixelSize: 14
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        onClicked: {
            // This would typically navigate back in the wizard
            // installer.back();
        }
    }

    // Custom Next Button
    Button {
        id: nextButton
        text: qsTr("Next")
        x: 540
        y: 420
        width: 80
        height: 30
        enabled: licenseCheckbox.checked && licenseAgreementPage.eulaScrolledToEnd // Enabled based on EULA acceptance and scroll
        tooltip: qsTr("Click Next to continue")
        // No explicit focus: true here, as the checkbox should be focused first, and then tab order will handle it.
        // However, if this is the only interactive element, it should have focus: true.
        // For now, I will leave it without explicit focus: true, assuming tab order from checkbox.

        background: Rectangle {
            color: nextButton.pressed ? "#007BFF" : (nextButton.hovered ? "#4CAF50" : "#1E90FF")
            radius: 5
            border.color: nextButton.focus ? "#FFD700" : "transparent"
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
        onClicked: {
            // This would typically navigate forward in the wizard
            // installer.next();
        }
    }
}}