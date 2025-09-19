import QtQuick
import QtQuick.Layouts
import Theme
import "./PrimaryButton.qml"

PrimaryButton {
    id: sendButton
    text: "Send"
    font.pixelSize: Theme.fonts.body
    Layout.preferredWidth: 80
    Layout.preferredHeight: 40 // From the input spec
    onClicked: {
        console.log("Send button clicked");
        // TODO: Emit signal to send message from InputField
    }
}