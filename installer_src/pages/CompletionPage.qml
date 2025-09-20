import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Page {
    id: completionPage
    title: qsTr("Installation Complete")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        Label {
            Layout.fillWidth: true
            text: qsTr("BlueConnect Desktop has been successfully installed.")
            font.pointSize: 14
            horizontalAlignment: Text.AlignHCenter
        }

        Item { Layout.preferredHeight: 20 }

        CheckBox {
            id: launchAppCheckBox
            text: qsTr("Launch BlueConnect Desktop")
            checked: true
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Start the application immediately after closing the installer.")
        }

        Item { Layout.fillHeight: true } // Spacer

        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            Label {
                text: "<a href=\"https://yourcompany.com/releases\">" + qsTr("Release Notes") + "</a>"
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Label {
                text: "<a href=\"https://yourcompany.com/support\">" + qsTr("Support") + "</a>"
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
    }

    // The default wizard buttons (Finish) are used.
    // We need to connect the launch checkbox to the finish button action.
    Component.onCompleted: {
        var finishButton = gui.pageById(QInstaller.FinishButton);
        finishButton.clicked.connect(function() {
            if (launchAppCheckBox.checked) {
                installer.executeDetached(installer.value("TargetDir") + "/bin/BlueConnect");
            }
        });
    }
}