import QtQuick
import QtQuick.Controls

Slider {
    id: control

    implicitWidth: 240
    implicitHeight: 30
    leftPadding: 2
    rightPadding: 2
    topPadding: 5
    bottomPadding: 5

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: 8
        radius: 4
        color: "#11171d"
        border.width: 1
        border.color: control.enabled ? "#82909c" : "#4b545d"

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: control.enabled ? "#62a97d" : "#46544d"
            border.width: 1
            border.color: control.enabled ? "#a4d8b4" : "#59635e"
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 20
        implicitHeight: 20
        radius: 10
        color: !control.enabled ? "#596169" : control.pressed ? "#d9f8e2" : control.hovered ? "#caefd5" : "#aed8ba"
        border.width: 2
        border.color: control.enabled ? "#f5f8fa" : "#747d85"

        Rectangle {
            anchors.centerIn: parent
            width: 6
            height: 6
            radius: 3
            color: control.enabled ? "#315d40" : "#454c52"
        }
    }
}
