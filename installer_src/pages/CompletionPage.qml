import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12

WizardPage {
    id: completionPage
    title: qsTr("Installation Complete")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 50
        spacing: 15

        Label {
            text: qsTr("BlueConnect Desktop has been successfully installed!")
            font.pixelSize: 20
            font.bold: true
            color: "#1E90FF"
            Layout.alignment: Qt.AlignHCenter
        }

        // Launch App Checkbox
        CheckBox {
            id: launchAppCheckbox
            text: qsTr("Launch BlueConnect Desktop")
            checked: true
            tooltip: qsTr("Start app immediately after installation")
            Layout.alignment: Qt.AlignHCenter
            onCheckedChanged: {
                installer.setValue("LaunchApp", checked);
            }
        }

        // Release Notes Link
        Label {
            id: releaseNotesLink
            text: qsTr("<a href=\"https://yourcompany.com/releases\">View Release Notes</a>")
            textFormat: Text.RichText
            font.pixelSize: 16
            color: "#1E90FF"
            Layout.alignment: Qt.AlignHCenter
            onLinkActivated: Qt.openUrlExternally(link)
        }

        // Support Link
        Label {
            id: supportLink
            text: qsTr("<a href=\"https://yourcompany.com/support\">Get Support</a>")
            textFormat: Text.RichText
            font.pixelSize: 16
            color: "#1E90FF"
            Layout.alignment: Qt.AlignHCenter
            onLinkActivated: Qt.openUrlExternally(link)
        }

        // Finish Button
        Button {
            id: finishButton
            text: qsTr("Finish")
            width: 120
            height: 40
            Layout.alignment: Qt.AlignHCenter

            background: Rectangle {
                color: finishButton.pressed ? "#007BFF" : (finishButton.hovered ? "#4CAF50" : "#1E90FF")
                radius: 5
                border.color: finishButton.focus ? "#FFD700" : "transparent"
                border.width: finishButton.focus ? 2 : 0
                Behavior on color { ColorAnimation { duration: 100 } }
            }
            contentItem: Label {
                text: finishButton.text
                font.pixelSize: 18
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                if (launchAppCheckbox.checked) {
                    // In a real scenario, this would launch the installed application
                    // installer.execute("start " + installer.value("TargetDir") + "/BlueConnectDesktop.exe"); // Example for Windows
                    console.log(qsTr("Launching BlueConnect Desktop..."));
                }
                installer.finish(); // Complete the installation process
            }

            // Fade animation for the button
            OpacityAnimator {
                target: finishButton
                from: 0.0
                to: 1.0
                duration: 1000 // 1s fade
                running: true
            }
        }
    }

    property bool complete: true
}
