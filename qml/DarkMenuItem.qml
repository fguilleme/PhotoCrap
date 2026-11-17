import QtQuick
import QtQuick.Controls
import "Studio.js" as Studio

MenuItem {
    id: control

    implicitHeight: 34
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: enabled

    contentItem: Item {
        readonly property real arrowPadding: control.subMenu && control.arrow
                                                  ? control.arrow.width + control.spacing : 0
        implicitWidth: itemLabel.implicitWidth + shortcutLabel.implicitWidth + arrowPadding + 28
        implicitHeight: 22

        Text {
            id: itemLabel
            anchors.left: parent.left
            anchors.leftMargin: control.checkable ? 22 : 0
            anchors.right: shortcutLabel.visible ? shortcutLabel.left : parent.right
            anchors.rightMargin: parent.arrowPadding
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: !control.enabled ? Studio.disabled
                                    : control.highlighted || control.down ? Studio.ink : Studio.ink
            font: control.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            visible: control.checkable && control.checked
            text: "✓"
            color: control.highlighted || control.down ? Studio.ink : Studio.accent
            font.pixelSize: 15
            font.bold: true
        }

        Text {
            id: shortcutLabel
            anchors.right: parent.right
            anchors.rightMargin: parent.arrowPadding
            anchors.verticalCenter: parent.verticalCenter
            visible: text.length > 0
            text: control.action && control.action.shortcut
                  ? control.action.shortcut.toString() : ""
            color: control.highlighted || control.down ? Studio.ink : Studio.muted
            font: control.font
        }
    }

    arrow: Text {
        x: control.mirrored ? control.leftPadding : control.width - width - control.rightPadding
        y: (control.height - height) / 2
        visible: control.subMenu
        text: control.mirrored ? "‹" : "›"
        color: control.highlighted || control.down ? Studio.ink : Studio.secondary
        font.pixelSize: 18
        font.bold: true
    }

    background: Rectangle {
        radius: 4
        color: control.enabled && (control.highlighted || control.down)
               ? Studio.selected
               : control.enabled && control.hovered ? Studio.hover : "transparent"
    }
}
