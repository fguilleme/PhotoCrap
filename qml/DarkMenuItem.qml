import QtQuick
import QtQuick.Controls

MenuItem {
    id: control

    implicitHeight: 34
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: enabled

    contentItem: Text {
        text: control.text
        color: !control.enabled ? "#747c85"
                                : control.highlighted || control.down ? "#ffffff" : "#dbe1e8"
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 4
        color: control.enabled && (control.highlighted || control.down)
               ? "#2866c8"
               : control.enabled && control.hovered ? "#343d48" : "transparent"
    }
}
