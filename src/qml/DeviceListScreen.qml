import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme
import DeviceCard

Rectangle {
    id: deviceListScreen
    color: Theme.color("background")
    Layout.fillWidth: true
    Layout.fillHeight: true

    ListView {
        id: deviceList
        anchors.fill: parent
        anchors.margins: Theme.gridUnit * 2
        spacing: Theme.gridUnit
        model: 10 // Placeholder model for now
        delegate: DeviceCard {
            deviceName: "Device " + index
            deviceMac: "00:11:22:33:44:" + index + index
            deviceRssi: -50 - index
            devicePaired: index % 2 === 0
        }

        ScrollIndicator.vertical: ScrollIndicator {
            id: verticalIndicator
        }
    }
}