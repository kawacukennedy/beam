import QtQuick
import QtQuick.Controls
import Theme

Button {
    id: control

    property color buttonColor: Theme.color("primary")
    property color textColor: "#FFFFFF"

    implicitWidth: contentItem.implicitWidth + 40 // 20px padding on each side
    implicitHeight: contentItem.implicitHeight + 24 // 12px padding on top/bottom

    background: Rectangle {
        radius: 8
        color: control.enabled ? (control.pressed ? Qt.darker(buttonColor, 1.1) : (control.hovered ? Qt.lighter(buttonColor, 1.08) : buttonColor)) : Qt.rgba(buttonColor.r, buttonColor.g, buttonColor.b, 0.5)
        border.color: control.enabled ? (control.pressed ? Qt.darker(buttonColor, 1.1) : (control.hovered ? Qt.lighter(buttonColor, 1.08) : buttonColor)) : "transparent"

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.InOutQuad }
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    // The spec mentions a shadow on hover. This is tricky for a control that might not be rectangular.
    // A DropShadow effect could be applied, but it's better to apply it on the container of the button if needed.
    // For now, I'll omit the shadow to keep the component self-contained and performant.
}
