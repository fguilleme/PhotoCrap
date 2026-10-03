import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "lightEditorRoot"

    property var parameters: ({})
    property Item exposureField: null
    readonly property int controlCount: lightControls.count
    signal editStarted()
    signal previewed(string key, real value)
    signal committed(string key, real value)

    spacing: 7

    function formattedValue(value, decimals) {
        const number = Number(value)
        return (number > 0 ? "+" : "") + number.toFixed(decimals)
    }

    Repeater {
        id: lightControls
        model: [
            {key: "exposure", label: qsTr("Exposition"), from: -5, to: 5, step: .1, decimals: 1},
            {key: "contrast", label: qsTr("Contraste"), from: -100, to: 100, step: 1, decimals: 0},
            {key: "highlights", label: qsTr("Hautes lumières"), from: -100, to: 100, step: 1, decimals: 0},
            {key: "shadows", label: qsTr("Ombres"), from: -100, to: 100, step: 1, decimals: 0},
            {key: "whites", label: qsTr("Blancs"), from: -100, to: 100, step: 1, decimals: 0},
            {key: "blacks", label: qsTr("Noirs"), from: -100, to: 100, step: 1, decimals: 0}
        ]

        delegate: ColumnLayout {
            required property var modelData
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    text: modelData.label
                    color: "#d5dbe0"
                    Layout.fillWidth: true
                }
                TextField {
                    id: valueField
                    objectName: "lightValue_" + modelData.key
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 25
                    leftPadding: 5
                    rightPadding: 5
                    color: "#e7ebef"
                    selectionColor: "#2866c8"
                    selectedTextColor: "#ffffff"
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    selectByMouse: true
                    validator: DoubleValidator {
                        bottom: modelData.from
                        top: modelData.to
                        decimals: modelData.decimals
                        notation: DoubleValidator.StandardNotation
                    }
                    background: Rectangle {
                        radius: 3
                        color: valueField.activeFocus ? "#252e37" : "#171c22"
                        border.width: 1
                        border.color: valueField.activeFocus ? "#6fa785" : "#424b54"
                    }
                    Component.onCompleted: {
                        if (modelData.key === "exposure")
                            root.exposureField = valueField
                    }
                    Keys.onReturnPressed: function(event) {
                        event.accepted = true
                        focus = false
                    }
                    Keys.onEnterPressed: function(event) {
                        event.accepted = true
                        focus = false
                    }
                    onEditingFinished: {
                        const parsed = Number.fromLocaleString(Qt.locale(), text.replace("+", ""))
                        if (isNaN(parsed))
                            return
                        const nextValue = Math.max(modelData.from, Math.min(modelData.to, parsed))
                        root.editStarted()
                        lightSlider.value = nextValue
                        root.previewed(modelData.key, nextValue)
                        root.committed(modelData.key, nextValue)
                    }
                    Binding {
                        target: valueField
                        property: "text"
                        value: root.formattedValue(lightSlider.value, modelData.decimals)
                        when: !valueField.activeFocus
                    }
                }
            }
            DarkSlider {
                id: lightSlider
                objectName: "lightSlider_" + modelData.key
                Layout.fillWidth: true
                Layout.preferredHeight: 24
                from: modelData.from
                to: modelData.to
                stepSize: modelData.step
                value: root.parameters[modelData.key] === undefined
                       ? 0 : Number(root.parameters[modelData.key])
                onPressedChanged: {
                    if (pressed)
                        root.editStarted()
                    else
                        root.committed(modelData.key, value)
                }
                onMoved: root.previewed(modelData.key, value)
            }
        }
    }
}
