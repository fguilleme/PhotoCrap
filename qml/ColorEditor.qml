import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "colorEditorRoot"
    property var parameters: ({})
    property string pickerMode: ""
    readonly property int controlCount: controls.count
    signal pickerSelected(string mode)
    signal editStarted()
    signal previewed(string key, real value)
    signal committed(string key, real value)
    spacing: 7

    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Balance des blancs"); color: "#d5dbe0"; Layout.fillWidth: true }
        IconButton {
            objectName: "colorWhiteBalancePicker"
            glyph: "⌁"
            hint: qsTr("Pipette de balance des blancs")
            selected: root.pickerMode === "whiteBalance"
            onClicked: root.pickerSelected(selected ? "" : "whiteBalance")
        }
    }
    Repeater {
        id: controls
        model: [
            {key:"temperature", label:qsTr("Température"), left:"#3977d4", right:"#e5aa3c"},
            {key:"tint", label:qsTr("Teinte"), left:"#54a96c", right:"#cf5ab3"},
            {key:"vibrance", label:qsTr("Vibrance"), left:"#69737c", right:"#e96545"},
            {key:"saturation", label:qsTr("Saturation"), left:"#777777", right:"#ea4c3c"}
        ]
        delegate: ColumnLayout {
            required property var modelData
            Layout.fillWidth: true
            spacing: 2
            RowLayout {
                Layout.fillWidth: true
                Label { text: modelData.label; color: "#d5dbe0"; Layout.fillWidth: true }
                Label { text: Number(slider.value).toFixed(0); color: "#aab4bd"; Layout.preferredWidth: 42; horizontalAlignment: Text.AlignRight }
            }
            Slider {
                id: slider
                Layout.fillWidth: true
                Layout.preferredHeight: 24
                from: -100; to: 100; stepSize: 1
                value: root.parameters[modelData.key] === undefined ? 0 : Number(root.parameters[modelData.key])
                background: Rectangle {
                    x: slider.leftPadding; y: slider.topPadding + (slider.availableHeight-height)/2
                    width: slider.availableWidth; height: 8; radius: 4
                    gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: modelData.left } GradientStop { position: 1; color: modelData.right } }
                    border.width: 1; border.color: "#d4d9dd"
                }
                handle: Rectangle { x: slider.leftPadding + slider.visualPosition*(slider.availableWidth-width); y: slider.topPadding+(slider.availableHeight-height)/2; width: 18; height: 18; radius: 9; color: "#e8edf0"; border.width: 2; border.color: "#20262d" }
                onPressedChanged: { if (pressed) root.editStarted(); else root.committed(modelData.key,value) }
                onMoved: root.previewed(modelData.key,value)
            }
        }
    }
    function applyWhiteBalance(color) {
        const red = color.r * 255, green = color.g * 255, blue = color.b * 255
        const temperature = Math.max(-100, Math.min(100, (blue-red) * .65))
        const tint = Math.max(-100, Math.min(100, ((red+blue)/2-green) * .65))
        root.editStarted(); root.previewed("temperature", temperature); root.previewed("tint", tint); root.committed("temperature", temperature); root.committed("tint", tint)
        root.pickerSelected("")
    }
}
