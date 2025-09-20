import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Page {
    id: checksPage
    title: qsTr("Pre-installation Checks")

    property bool checksRunning: true
    property bool checksPassed: false

    Component.onCompleted: {
        checksPage.complete = false;
        runChecks();
    }

    function runChecks() {
        // This function will be called to start the checks.
        // The actual logic is in control.js, which will update installer values.
        // Here, we just trigger it and then update the UI based on the results.
        installer.executeFunction("runPreInstallChecks", [], function(result) {
            checksPassed = result;
            checksRunning = false;
            checksPage.complete = checksPassed;
        });
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20

        Text {
            Layout.fillWidth: true
            text: qsTr("Checking system compatibility...")
            font.pointSize: 14
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Item { Layout.preferredHeight: 20 }

        // -- Check Items --
        CheckItem { id: diskSpaceCheck; text: qsTr("Disk space (>= 200MB)") }
        CheckItem { id: writePermCheck; text: qsTr("Write permissions for installation path") }
        CheckItem { id: bluetoothCheck; text: qsTr("Bluetooth adapter and drivers") }
        CheckItem { id: depsCheck; text: qsTr("System dependencies (VC++ Redist, etc.)") }

        Item { Layout.fillHeight: true } // Spacer

        // -- Overall Status --
        Text {
            id: statusLabel
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: 12
            color: checksPassed ? "green" : (checksRunning ? "blue" : "red")
            text: checksRunning ? qsTr("Running checks...") : (checksPassed ? qsTr("All checks passed!") : qsTr("One or more checks failed."))
        }

        ProgressBar {
            id: progressBar
            Layout.fillWidth: true
            indeterminate: checksRunning
            value: checksRunning ? 0 : (checksPassed ? 1 : 0)
        }
    }

    // We need a component for each check item to show its status
    Component {
        id: checkItemComponent
        CheckItem {}
    }
}

// Custom component for displaying a single check
Item {
    id: checkItemRoot
    width: parent.width
    height: 30

    property string text: ""
    property int status: -1 // -1: pending, 0: fail, 1: pass

    RowLayout {
        anchors.fill: parent

        Text {
            text: checkItemRoot.text
            Layout.fillWidth: true
        }

        Text {
            text: status === 1 ? "✔" : (status === 0 ? "✖" : "...")
            font.pointSize: 16
            color: status === 1 ? "green" : (status === 0 ? "red" : "gray")
        }
    }
}
