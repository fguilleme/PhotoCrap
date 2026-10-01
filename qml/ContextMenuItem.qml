import QtQuick
import QtQuick.Controls

MenuItem {
    id: control

    implicitWidth: 310
    implicitHeight: 36
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: enabled

    contentItem: Text {
        text: control.text
        color: control.enabled ? "#e4e8ed" : "#626970"
        opacity: control.enabled ? 1.0 : 0.72
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 5
        color: control.enabled && control.highlighted ? "#2866c8" : "transparent"
    }
}
