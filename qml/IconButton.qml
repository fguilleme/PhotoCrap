import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string glyph: ""
    property string hint: ""
    property bool selected: false
    implicitWidth: 38
    implicitHeight: 36
    hoverEnabled: true
    Accessible.name: hint.length ? hint : text
    background: Rectangle {
        color: control.selected ? "#365549" : control.down ? "#3c424b" : control.hovered ? "#30363e" : "transparent"
        radius: 5
        border.width: control.selected ? 1 : 0
        border.color: "#668f78"
    }
    contentItem: Text {
        text: control.glyph.length ? control.glyph : control.text
        color: !control.enabled ? "#545d68" : control.selected ? "#c5f2cf" : "#c0c7d0"
        font.pixelSize: control.glyph.length ? 19 : 12
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    ToolTip.visible: hovered && hint.length > 0
    ToolTip.text: hint
    ToolTip.delay: 500
}
