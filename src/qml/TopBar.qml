import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme

Rectangle {
    id: topbar
    height: 60
    color: Theme.color("surface")
    Layout.fillWidth: true

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.gridUnit * 2
        anchors.rightMargin: Theme.gridUnit * 2
        spacing: Theme.gridUnit * 2

        // Bluetooth Toggle
        Switch {
            id: bluetoothToggle
            text: "Bluetooth"
            checked: true // TODO: Bind to actual Bluetooth status
            onCheckedChanged: {
                console.log("Bluetooth Toggled: " + checked);
                // TODO: Call C++ backend to enable/disable Bluetooth
            }
        }

        Item {
            Layout.fillWidth: true
        }

        // Search Bar
        TextField {
            id: searchInput
            placeholderText: "Search..."
            Layout.preferredWidth: 200
            height: 40
            
            background: Rectangle {
                radius: 8
                border.width: searchInput.focus ? 2 : 1
                border.color: {
                    if (searchInput.focus) return Theme.color("primary");
                    return Theme.color("border");
                }
                Behavior on border.color { ColorAnimation { duration: 150 } }
            }
        }

        Item {
            Layout.fillWidth: true
        }

        // Profile Avatar (Placeholder)
        Rectangle {
            width: 40
            height: 40
            radius: 20
            color: Theme.color("primary")
            
            Text {
                anchors.centerIn: parent
                text: "U" // "User"
                color: "white"
                font.pixelSize: 20
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    console.log("Profile Avatar Clicked");
                    // TODO: Show profile menu
                }
            }
        }
    }
}
