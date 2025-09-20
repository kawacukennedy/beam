import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Page {
    id: installationDirectoryPage
    title: qsTr("Installation Directory")

    property bool pathIsValid: false

    Component.onCompleted: {
        targetDirectory.text = installer.value("TargetDir");
        validatePath(targetDirectory.text);
        installationDirectoryPage.complete = pathIsValid;
    }

    function validatePath(path) {
        var hasEnoughSpace = installer.freeDiskSpace(path) > 200 * 1024 * 1024;
        var isWritable = installer.isWritable(path);
        pathIsValid = hasEnoughSpace && isWritable;

        if (!isWritable) {
            validationErrorLabel.text = qsTr("Directory is not writable.");
        } else if (!hasEnoughSpace) {
            validationErrorLabel.text = qsTr("Not enough free disk space.");
        } else {
            validationErrorLabel.text = "";
        }

        diskSpaceLabel.text = qsTr("Required space: 200 MB | Available space: %1 GB").arg((installer.freeDiskSpace(path) / (1024*1024*1024)).toFixed(2));
        installationDirectoryPage.complete = pathIsValid;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20

        Label {
            text: qsTr("Please select the directory where you want to install BlueConnect Desktop.")
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: targetDirectory
                Layout.fillWidth: true
                placeholderText: qsTr("Select installation path")
                onTextChanged: validatePath(text)
            }

            Button {
                id: browseButton
                text: qsTr("Browse...")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Browse for a folder")
                onClicked: {
                    var newDir = QFileDialog.getExistingDirectory(gui, "Select Installation Directory", targetDirectory.text);
                    if (newDir) {
                        targetDirectory.text = newDir;
                        installer.setValue("TargetDir", newDir);
                    }
                }
            }
        }

        Label {
            id: diskSpaceLabel
            font.pointSize: 10
        }

        Label {
            id: validationErrorLabel
            color: "#FF0000"
            font.pointSize: 10
            wrapMode: Text.WordWrap
        }
    }
}
