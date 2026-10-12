import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string glyph: ""
    property string toolName: ""
    property string hint: ""
    property bool selected: false
    property bool hasMenu: false
    property string groupId: ""
    signal contextRequested()
    signal alternateClicked()
    onPressAndHold: if (hasMenu) contextRequested()
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
    contentItem: Item {
        ToolIcon {
            anchors.centerIn: parent
            width: 24
            height: 24
            toolName: control.toolName
            fallback: control.glyph.length ? control.glyph : control.text
            fallbackPixelSize: control.glyph.length ? 19 : 12
            iconColor: !control.enabled ? "#545d68" : control.selected ? "#c5f2cf" : "#c0c7d0"
        }
    }
    Text {
        visible: control.hasMenu
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 3
        text: "◢"
        color: control.selected ? "#c5f2cf" : "#89939e"
        font.pixelSize: 7
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton && !(control.hasMenu && (mouse.modifiers & Qt.AltModifier))) mouse.accepted=false
        }
        onClicked: function(mouse) {
            if (mouse.button===Qt.RightButton) control.contextRequested()
            else control.alternateClicked()
        }
    }
    ToolTip.visible: hovered && hint.length > 0
    ToolTip.text: hint
    ToolTip.delay: 500
}
