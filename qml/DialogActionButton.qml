import QtQuick
import QtQuick.Controls
import "Studio.js" as Studio

Button {
    id: control

    property bool defaultButton: false
    readonly property bool macStyle: Qt.platform.os === "osx"

    implicitWidth: Math.max(86, contentItem.implicitWidth + 28)
    implicitHeight: macStyle ? 30 : 32
    leftPadding: 14
    rightPadding: 14
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    highlighted: defaultButton

    Accessible.name: text
    Accessible.role: Accessible.Button

    function editableFieldHasFocus() {
        const window = control.Window.window
        if (!window)
            return false
        const item = window.activeFocusItem
        return item instanceof TextInput || item instanceof TextEdit
    }

    Keys.onSpacePressed: function(event) {
        if (control.activeFocus && control.enabled) {
            control.clicked()
            event.accepted = true
        }
    }

    Shortcut {
        enabled: control.defaultButton && control.visible && control.enabled
                 && control.editableFieldHasFocus()
        sequences: [StandardKey.InsertParagraphSeparator]
        context: Qt.WindowShortcut
        onActivated: {
            const window = control.Window.window
            if (window && window.activeFocusItem)
                window.activeFocusItem.focus = false
        }
    }

    Shortcut {
        enabled: control.defaultButton && control.visible && control.enabled
                 && !control.editableFieldHasFocus()
        sequences: [StandardKey.InsertParagraphSeparator]
        context: Qt.WindowShortcut
        onActivated: control.clicked()
    }

    background: Rectangle {
        radius: control.macStyle ? 6 : 5
        color: {
            if (!control.enabled)
                return Studio.panel
            if (control.defaultButton)
                return control.down ? Studio.accentPressed : control.hovered ? Studio.accentHover : Studio.accent
            return control.down ? Studio.line : control.hovered ? Studio.hover : Studio.raised
        }
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Studio.ink : control.defaultButton ? Studio.accent : Studio.line
    }

    contentItem: Text {
        text: control.text
        color: !control.enabled ? Studio.disabled : control.defaultButton ? Studio.onAccent : Studio.ink
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
