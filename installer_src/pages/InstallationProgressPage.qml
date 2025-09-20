import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Page {
    id: progressPage
    title: qsTr("Installation Progress")

    property int elapsedTime: 0
    property Timer timer: Timer {
        interval: 1000
        repeat: true
        onTriggered: progressPage.elapsedTime++
    }

    Component.onCompleted: {
        installer.installationStarted.connect(function() {
            progressPage.timer.start();
        });
        installer.installationFinished.connect(function() {
            progressPage.timer.stop();
        });
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20

        Label {
            id: progressLabel
            Layout.fillWidth: true
            text: qsTr("Installing BlueConnect Desktop...")
            font.pointSize: 14
        }

        TextArea {
            id: logWindow
            Layout.fillWidth: true
            Layout.fillHeight: true
            readOnly: true
            font.family: "monospace"
            font.pointSize: 10
            background: Rectangle {
                color: "#f0f0f0"
                border.color: "#cccccc"
            }
        }

        ProgressBar {
            id: progressBar
            Layout.fillWidth: true
            from: 0
            to: 100
            value: installer.progress
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                id: percentageLabel
                text: qsTr("%1%").arg(installer.progress)
            }

            Label {
                id: elapsedTimeLabel
                Layout.alignment: Qt.AlignRight
                text: qsTr("Elapsed time: %1s").arg(progressPage.elapsedTime)
            }
        }
    }

    // Connections to update the log window
    Connections {
        target: installer
        function onMessage(message) {
            logWindow.append(message);
        }
    }
}