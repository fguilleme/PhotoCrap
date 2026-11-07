import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.TextField {
    id: field
    implicitHeight: 32
    leftPadding: 9
    rightPadding: 9
    font.pixelSize: 13
    selectByMouse: true
    color: enabled ? "#e5eaf0" : "#788493"
    placeholderTextColor: "#7f8b99"
    selectionColor: "#4b7865"
    background: Rectangle {
        color: "#181e25"
        radius: 5
        border.width: field.activeFocus ? 2 : 1
        border.color: field.activeFocus ? "#8eb39a" : "#3b4551"
    }
}
