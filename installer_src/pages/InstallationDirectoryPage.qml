import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.2 // For FileDialog

WizardPage {
    id: installationDirectoryPage
    title: qsTr("Installation Directory")

    property alias installationPath: pathTextBox.text
    property alias diskSpaceText: diskSpaceLabel.text
    property alias validationErrorText: validationErrorLabel.text

    // Path Textbox
    TextField {
        id: pathTextBox
        x: 50
        y: 80
        width: 400
        height: 25
        placeholderText: qsTr("Select installation path")
        font.pixelSize: 16 // 12pt
        text: installer.value("TargetDir")
        focus: true // Explicitly set focus for accessibility
        onTextChanged: {
            installer.setValue("TargetDir", text);
            validatePath();
        }
    }

    // Browse Button
    Button {
        id: browseButton
        text: qsTr("Browse...")
        x: 460
        y: 80
        width: 80
        height: 25
        tooltip: qsTr("Browse for folder")
        onClicked: fileDialog.open()
    }

    // Disk Space Label
    Label {
        id: diskSpaceLabel
        x: 50
        y: 120
        text: qsTr("Available: Calculating...")
        font.pixelSize: 16 // 12pt
        color: "#333333"
    }

    // Validation Error Label
    Label {
        id: validationErrorLabel
        x: 50
        y: 150
        text: ""
        font.pixelSize: 16 // 12pt
        color: "#FF0000"
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Select Installation Directory")
        selectFolder: true
        onAccepted: {
            pathTextBox.text = fileDialog.folder.replace("file://", "");
        }
    }

    property bool complete: false

    function validatePath() {
        var path = pathTextBox.text;
        var minRequiredSpaceMB = 200;
        var isValid = true;
        var errorMsg = "";

        // Simulate disk space check
        // In a real scenario, use installer.checkFreeDiskSpace(path)
        var availableSpaceMB = Math.floor(Math.random() * 500) + 100; // Simulate 100-600MB available
        diskSpaceLabel.text = qsTr("Available: %1MB").arg(availableSpaceMB);

                    if (availableSpaceMB < minRequiredSpaceMB) {
                        errorMsg += qsTr("Insufficient disk space. ");
                        isValid = false;
                    }
        
                    // Simulate writability check
                    // In a real scenario, you'd try to write a temp file or use platform-specific checks
                    var isWritable = Math.random() > 0.1; // 90% chance of success
                    if (!isWritable) {
                        errorMsg += qsTr("Directory is not writable. ");
                        isValid = false;
                    }
        
                    if (!isValid) {
                        validationErrorLabel.text = qsTr("Directory is not writable or has insufficient space.");
                    } else {
                        validationErrorLabel.text = "";
                    }        installationDirectoryPage.complete = isValid;
    }

    Component.onCompleted: {
        validatePath();
    }
}