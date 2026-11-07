import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Button {
    id: control
    property bool primary: false
    implicitHeight: 32
    implicitWidth: Math.max(78, contentItem.implicitWidth + 24)
    leftPadding: 12
    rightPadding: 12
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font.pixelSize: 13
    contentItem: Text {
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: !control.enabled ? "#76818e" : control.primary ? "#15231c" : "#e4eaf0"
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 5
        color: !control.enabled ? "#272e37" : control.primary ? (control.down ? "#779e84" : control.hovered ? "#a4c4ad" : "#8eb39a") : control.down ? "#222a34" : control.hovered ? "#37414e" : "#2d3641"
        border.color: control.activeFocus ? "#bddbca" : control.primary ? "#a4c4ad" : "#47525f"
        border.width: control.activeFocus ? 2 : 1
    }
}
