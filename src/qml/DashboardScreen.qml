import QtQuick
import QtQuick.Layouts
import Theme
import AdapterCard
import RecentDevicesCard

Rectangle {
    id: dashboardScreen
    color: Theme.color("background")
    Layout.fillWidth: true
    Layout.fillHeight: true

    ScrollView {
        anchors.fill: parent
        
        ColumnLayout {
            width: parent.width
            anchors.margins: Theme.gridUnit * 2
            spacing: Theme.gridUnit * 2

            AdapterCard {
            }

            Text {
                text: "Recent Devices"
                font.pixelSize: Theme.fonts.h2
                color: Theme.color("text_primary")
            }

            Flow {
                width: parent.width
                spacing: Theme.gridUnit * 2
                
                Repeater {
                    model: 5 // Placeholder
                    delegate: RecentDevicesCard {
                    }
                }
            }
        }
    }
}
