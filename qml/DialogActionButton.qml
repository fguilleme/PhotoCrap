import QtQuick
import QtQuick.Controls

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
                return "#282d34"
            if (control.defaultButton)
                return control.down ? "#276db5" : control.hovered ? "#438ed9" : "#347fc8"
            return control.down ? "#414954" : control.hovered ? "#39414b" : "#303741"
        }
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#7eb7f0" : control.defaultButton ? "#65a3df" : "#515c68"
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? "#f5f7f9" : "#747d87"
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
