import QtQuick
import QtQuick.Layouts
import Theme

RowLayout {
    id: typingIndicator
    spacing: 5

    Repeater {
        model: 3
        delegate: Rectangle {
            width: 10
            height: 10
            radius: 5
            color: Theme.color("secondary")

            SequentialAnimation {
                running: true
                loops: Animation.Infinite
                delay: index * 100
                
                NumberAnimation {
                    target: delegate
                    property: "y"
                    to: -5
                    duration: 300
                    easing.type: Easing.OutQuad
                }
                NumberAnimation {
                    target: delegate
                    property: "y"
                    to: 0
                    duration: 300
                    easing.type: Easing.InQuad
                }
                PauseAnimation { duration: 400 }
            }
        }
    }
}
