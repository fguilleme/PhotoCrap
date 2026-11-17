import QtQuick
import "Studio.js" as Studio
import QtQuick.Controls

SpinBox {
    id: control

    property string suffix: ""
    property bool scrubbing: false

    implicitWidth: 104
    implicitHeight: 32
    leftPadding: 25
    rightPadding: 25
    editable: true

    textFromValue: function(value, locale) {
        return Number(value).toLocaleString(locale, "f", 0)
    }
    valueFromText: function(text, locale) {
        return Number.fromLocaleString(locale, text.trim())
    }

    // SpinBox commits the text of its contentItem. Wrapping TextInput in an
    // Item disconnects Return/focus-loss validation from valueModified.
    contentItem: TextInput {
        id: valueInput
        z: 2
        rightPadding: suffixLabel.visible ? suffixLabel.width + 4 : 0
        text: control.displayText
        color: control.enabled ? Studio.ink : Studio.disabled
        selectionColor: Studio.accent
        selectedTextColor: Studio.onAccent
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true

        Text {
            id: suffixLabel
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            visible: control.suffix.length > 0
            text: control.suffix.trim()
            color: Studio.muted
            font: control.font
        }
    }

    down.indicator: Rectangle {
        x: 1
        y: 1
        width: 23
        height: control.height - 2
        radius: 5
        color: control.down.pressed ? Studio.selected
                                    : control.down.hovered ? Studio.hover : "transparent"

        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: parent.height - 10
            color: Studio.line
        }
        Text {
            anchors.centerIn: parent
            text: "−"
            color: control.enabled ? Studio.secondary : Studio.disabled
            font.pixelSize: 16
        }
    }

    up.indicator: Rectangle {
        x: control.width - width - 1
        y: 1
        width: 23
        height: control.height - 2
        radius: 5
        color: control.up.pressed ? Studio.selected
                                  : control.up.hovered ? Studio.hover : "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: parent.height - 10
            color: Studio.line
        }
        Text {
            anchors.centerIn: parent
            text: "+"
            color: control.enabled ? Studio.secondary : Studio.disabled
            font.pixelSize: 15
        }
    }

    background: Rectangle {
        radius: 4
        color: control.activeFocus ? Studio.raised : Studio.field
        border.width: 1
        border.color: control.activeFocus ? Studio.accent : control.hovered ? Studio.muted : Studio.line
    }
}
