import QtQuick 2.12
import QtQuick.Controls 2.12

Page {
    id: licensePage
    title: qsTr("License Agreement")

    property bool eulaScrolledToEnd: false

    Component.onCompleted: {
        // This ensures the page's completeness is false until conditions are met.
        licensePage.complete = false;
    }

    function updateNextButtonState() {
        var accepted = licenseCheckbox.checked;
        var scrolled = eulaScrolledToEnd;
        licensePage.complete = accepted && scrolled;
    }

    ScrollView {
        id: scrollBox
        x: 50
        y: 80
        width: 540
        height: 300
        clip: true

        ScrollBar.vertical.policy: ScrollBar.AlwaysOn

        onAtYEndChanged: {
            if (atYEnd) {
                licensePage.eulaScrolledToEnd = true;
                updateNextButtonState();
            }
        }

        TextArea {
            id: eulaText
            width: scrollBox.width
            readOnly: true
            text: installer.isInstaller ? installer.loadText(installer.value("LicenseFile")) : "EULA not available in maintenance mode."
            font.family: "System Default"
            font.pointSize: 12
            wrapMode: Text.WordWrap
            color: "#333333"
            background: Rectangle {
                color: "#FFFFFF"
                border.color: "#CCCCCC"
                border.width: 1
            }
        }
    }

    CheckBox {
        id: licenseCheckbox
        x: 50
        y: 390
        text: qsTr("I accept the terms of the License Agreement")
        enabled: eulaScrolledToEnd // Disabled until user scrolls down

        ToolTip.visible: hovered
        ToolTip.text: qsTr("You must scroll to the end of the license and accept the terms to continue.")

        onCheckedChanged: {
            updateNextButtonState();
        }
    }

    Text {
        id: validationErrorLabel
        x: 50
        y: 415
        width: 480
        wrapMode: Text.WordWrap
        text: qsTr("You must accept the EULA to proceed.")
        color: "#FF0000"
        font.pointSize: 10
        visible: !licenseCheckbox.checked && eulaScrolledToEnd
    }

    // The default wizard buttons are used, so we don't need to create them here.
    // The 'complete' property of the page will enable/disable the Next button.
}
