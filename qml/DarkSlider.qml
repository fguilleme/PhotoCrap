import QtQuick
import QtQuick.Controls
import "Studio.js" as Studio

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
        height: 4
        radius: 2
        color: Studio.line

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: control.enabled ? Studio.secondary : Studio.disabled
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 14
        implicitHeight: 14
        radius: 7
        color: !control.enabled ? Studio.disabled : control.pressed ? Studio.accent : control.hovered ? Studio.ink : Studio.secondary
        border.width: 2
        border.color: control.visualFocus ? Studio.accent : Studio.field
    }
}
