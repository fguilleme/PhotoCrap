import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "colorBalanceEditorRoot"

    property var parameters: ({})
    readonly property int controlCount: controls.count
    signal editStarted()
    signal previewed(string key, real value)
    signal committed(string key, real value)

    spacing: 9

    Repeater {
        id: controls
        model: [
            {key:"cyanRed", label:qsTr("Cyan ↔ Rouge"), left:"#18bfd0", right:"#e34b45"},
            {key:"magentaGreen", label:qsTr("Magenta ↔ Vert"), left:"#d94bb0", right:"#45b95e"},
            {key:"yellowBlue", label:qsTr("Jaune ↔ Bleu"), left:"#e4c93d", right:"#3978d7"}
        ]
        delegate: ColumnLayout {
            required property var modelData
            Layout.fillWidth: true
            spacing: 2
            RowLayout {
                Layout.fillWidth: true
                Label { text: modelData.label; color: "#d5dbe0"; Layout.fillWidth: true }
                Label {
                    text: Number(slider.value) > 0 ? "+" + Number(slider.value).toFixed(0)
                                                   : Number(slider.value).toFixed(0)
                    color: "#aab4bd"; Layout.preferredWidth: 42
                    horizontalAlignment: Text.AlignRight
                }
            }
            Slider {
                id: slider
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                from: -100; to: 100; stepSize: 1
                value: root.parameters[modelData.key] === undefined ? 0
                                                                    : Number(root.parameters[modelData.key])
                background: Rectangle {
                    x: slider.leftPadding
                    y: slider.topPadding + (slider.availableHeight-height)/2
                    width: slider.availableWidth; height: 9; radius: 4
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: modelData.left }
                        GradientStop { position: .5; color: "#8b8f93" }
                        GradientStop { position: 1; color: modelData.right }
                    }
                    border.width: 1; border.color: "#d4d9dd"
                }
                handle: Rectangle {
                    x: slider.leftPadding + slider.visualPosition*(slider.availableWidth-width)
                    y: slider.topPadding+(slider.availableHeight-height)/2
                    width: 19; height: 19; radius: 10
                    color: "#eef2f4"; border.width: 2; border.color: "#20262d"
                }
                onPressedChanged: {
                    if (pressed) root.editStarted()
                    else root.committed(modelData.key, value)
                }
                onMoved: root.previewed(modelData.key, value)
            }
        }
    }
}
