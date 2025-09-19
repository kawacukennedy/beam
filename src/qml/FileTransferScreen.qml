import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme

Rectangle {
    id: fileTransferScreen
    color: Theme.color("background")
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gridUnit * 2
        spacing: Theme.gridUnit

        Label {
            text: "File Transfers"
            font.pixelSize: Theme.fonts.h1
            font.weight: Theme.fonts.h1Weight
            color: Theme.color("text_primary")
            Layout.fillWidth: true
        }

        TableView {
            id: fileTransferTable
            Layout.fillWidth: true
            Layout.fillHeight: true

            model: 5 // Placeholder model

            columnWidthProvider: function(column) {
                if (column === 0) return 200 // File
                if (column === 1) return 100 // Size
                if (column === 2) return 150 // Device
                if (column === 3) return 200 // Progress
                if (column === 4) return 100 // Status
                return 100
            }

            delegate: Rectangle {
                height: 40
                color: Theme.color("surface")
                border.color: Theme.color("border")
                border.width: 1

                Text {
                    visible: column !== 3 // Hide text for progress column
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.gridUnit
                    text: {
                        if (column === 0) return "File " + index
                        if (column === 1) return (index * 100) + " MB"
                        if (column === 2) return "Device " + index
                        if (column === 4) return index % 2 === 0 ? "Transferring" : "Completed"
                        return ""
                    }
                    color: Theme.color("text_primary")
                }

                // Progress bar for the Progress column
                Rectangle {
                    visible: column === 3
                    anchors.verticalCenter: parent.verticalCenter
                    height: 8
                    width: parent.width - 16
                    anchors.horizontalCenter: parent.horizontalCenter
                    radius: 4
                    clip: true
                    color: Theme.color("border")

                    Rectangle {
                        id: progressBar
                        width: parent.width * (index * 20 / 100)
                        height: parent.height
                        color: Theme.color("primary")
                        radius: 4

                        Row {
                            id: stripes
                            clip: true
                            width: parent.width
                            height: parent.height
                            spacing: 10

                            Repeater {
                                model: 20
                                delegate: Rectangle {
                                    width: 5
                                    height: 8
                                    color: Qt.lighter(Theme.color("primary"), 1.2)
                                    transform: Rotation { angle: -45 }
                                }
                            }
                        }
                        NumberAnimation on x {
                            target: stripes
                            from: 0
                            to: -20
                            duration: 500
                            loops: Animation.Infinite
                        }
                    }
                }
            }

            header: TableViewHeader {
                Rectangle {
                    height: 40
                    color: Theme.color("surface")
                    border.color: Theme.color("border")
                    border.width: 1

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.gridUnit
                        text: {
                            if (column === 0) return "File"
                            if (column === 1) return "Size"
                            if (column === 2) return "Device"
                            if (column === 3) return "Progress"
                            if (column === 4) return "Status"
                            return ""
                        }
                        font.weight: Theme.fonts.bodyWeight
                        color: Theme.color("text_primary")
                    }
                }
            }
        }
    }
}
