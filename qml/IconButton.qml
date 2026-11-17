import QtQuick
import QtQuick.Controls
import "Studio.js" as Studio

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
        color: control.down ? Studio.line : control.selected ? Studio.selected : control.hovered && control.enabled ? Studio.hover : "transparent"
        radius: 4
        border.width: control.visualFocus ? 2 : 0
        border.color: Studio.accent
        Rectangle {
            visible: control.selected
            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
            width: 3; height: 15; radius: 1; color: Studio.accent
        }
    }
    contentItem: Item {
        ToolIcon {
            anchors.centerIn: parent
            width: 24
            height: 24
            toolName: control.toolName
            fallback: control.glyph.length ? control.glyph : control.text
            fallbackPixelSize: control.glyph.length ? 19 : 12
            iconColor: !control.enabled ? Studio.disabled : control.selected || control.hovered ? Studio.ink : Studio.secondary
        }
    }
    Text {
        visible: control.hasMenu
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 3
        text: "◢"
        color: control.selected ? Studio.accent : Studio.muted
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
