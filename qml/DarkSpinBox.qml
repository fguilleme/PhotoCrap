import QtQuick
import QtQuick.Controls

SpinBox {
    id: control

    property string suffix: ""

    implicitWidth: 112
    implicitHeight: 32
    leftPadding: 31
    rightPadding: 31
    editable: true

    textFromValue: function(value, locale) {
        return Number(value).toLocaleString(locale, "f", 0) + control.suffix
    }
    valueFromText: function(text, locale) {
        const number = text.replace(control.suffix, "").trim()
        return Number.fromLocaleString(locale, number)
    }

    contentItem: TextInput {
        z: 2
        text: control.displayText
        color: "#edf2f6"
        selectionColor: "#2866c8"
        selectedTextColor: "#ffffff"
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }

    down.indicator: Rectangle {
        x: 1
        y: 1
        width: 29
        height: control.height - 2
        radius: 5
        color: control.down.pressed ? "#2866c8"
                                    : control.down.hovered ? "#3a444f" : "transparent"

        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: parent.height - 10
            color: "#48535f"
        }
        Text {
            anchors.centerIn: parent
            text: "−"
            color: control.enabled ? "#cbd4dc" : "#69727b"
            font.pixelSize: 16
        }
    }

    up.indicator: Rectangle {
        x: control.width - width - 1
        y: 1
        width: 29
        height: control.height - 2
        radius: 5
        color: control.up.pressed ? "#2866c8"
                                  : control.up.hovered ? "#3a444f" : "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: parent.height - 10
            color: "#48535f"
        }
        Text {
            anchors.centerIn: parent
            text: "+"
            color: control.enabled ? "#cbd4dc" : "#69727b"
            font.pixelSize: 15
        }
    }

    background: Rectangle {
        radius: 6
        color: control.activeFocus ? "#252e37" : "#1b2128"
        border.width: 1
        border.color: control.activeFocus ? "#6fa785" : control.hovered ? "#5a6673" : "#3d4752"
    }
}
