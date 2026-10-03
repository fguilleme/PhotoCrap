import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "photoFilterEditorRoot"
    property var parameters: ({})
    readonly property color selectedColor: parameters.color || "#ec8a00"
    signal editStarted()
    signal previewed(string key, var value)
    signal committed(string key, var value)
    signal presetRequested(int index, string color)
    signal colorRequested()
    spacing: 10

    property var presets: [
        {name: qsTr("Filtre réchauffant (85)"), color: "#ec8a00"},
        {name: qsTr("Filtre réchauffant (LBA)"), color: "#f5a65b"},
        {name: qsTr("Filtre réchauffant (81)"), color: "#eab013"},
        {name: qsTr("Filtre refroidissant (80)"), color: "#00b5ff"},
        {name: qsTr("Filtre refroidissant (LBB)"), color: "#8ac7ff"},
        {name: qsTr("Filtre refroidissant (82)"), color: "#00a5ef"},
        {name: qsTr("Rouge"), color: "#e52a2a"},
        {name: qsTr("Orange"), color: "#f27d16"},
        {name: qsTr("Jaune"), color: "#f3ce20"},
        {name: qsTr("Vert"), color: "#38a45c"},
        {name: qsTr("Cyan"), color: "#18abb8"},
        {name: qsTr("Bleu"), color: "#3569ce"},
        {name: qsTr("Violet"), color: "#814fc0"},
        {name: qsTr("Magenta"), color: "#cf429b"},
        {name: qsTr("Sépia"), color: "#a66b35"},
        {name: qsTr("Sous-marin"), color: "#16a69e"}
    ]

    RowLayout {
        Layout.fillWidth: true
        RadioButton {
            id: presetRadio
            objectName: "photoFilterPresetRadio"
            text: qsTr("Filtre")
            checked: !Boolean(root.parameters.useCustomColor)
            onClicked: root.presetRequested(presetBox.currentIndex,
                                             root.presets[presetBox.currentIndex].color)
        }
        ComboBox {
            id: presetBox
            objectName: "photoFilterPreset"
            Layout.fillWidth: true
            textRole: "name"
            model: root.presets
            currentIndex: Math.max(0, Math.min(root.presets.length - 1,
                                               Number(root.parameters.preset || 0)))
            onActivated: {
                root.presetRequested(currentIndex, root.presets[currentIndex].color)
            }
        }
        Rectangle {
            width: 25; height: 25; radius: 5
            color: root.selectedColor
            border.width: 1; border.color: "#d5dbe0"
        }
    }

    RowLayout {
        Layout.fillWidth: true
        RadioButton {
            id: colorRadio
            objectName: "photoFilterColorRadio"
            text: qsTr("Couleur")
            checked: Boolean(root.parameters.useCustomColor)
            onClicked: root.colorRequested()
        }
        Rectangle {
            Layout.preferredWidth: 56; Layout.preferredHeight: 32
            radius: 6; color: root.selectedColor
            border.width: 2; border.color: colorRadio.checked ? "#65a7ff" : "#87929c"
            MouseArea { anchors.fill: parent; onClicked: root.colorRequested() }
        }
        Label {
            Layout.fillWidth: true
            text: root.selectedColor.toString().toUpperCase()
            color: "#aab4bd"
        }
    }

    ColumnLayout {
        Layout.fillWidth: true; spacing: 2
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Densité"); color: "#d5dbe0"; Layout.fillWidth: true }
            Label {
                text: Math.round(densitySlider.value) + " %"
                color: "#aab4bd"; Layout.preferredWidth: 48
                horizontalAlignment: Text.AlignRight
            }
        }
        Slider {
            id: densitySlider
            objectName: "photoFilterDensity"
            Layout.fillWidth: true; Layout.preferredHeight: 26
            from: 0; to: 100; stepSize: 1
            value: root.parameters.density === undefined ? 25 : Number(root.parameters.density)
            background: Rectangle {
                x: densitySlider.leftPadding
                y: densitySlider.topPadding + (densitySlider.availableHeight - height) / 2
                width: densitySlider.availableWidth; height: 9; radius: 4
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#303741" }
                    GradientStop { position: 1; color: root.selectedColor }
                }
                border.width: 1; border.color: "#d5dbe0"
            }
            handle: Rectangle {
                x: densitySlider.leftPadding + densitySlider.visualPosition *
                   (densitySlider.availableWidth - width)
                y: densitySlider.topPadding + (densitySlider.availableHeight - height) / 2
                width: 18; height: 18; radius: 9
                color: "#eef2f4"; border.width: 2; border.color: "#20262d"
            }
            onPressedChanged: {
                if (pressed) root.editStarted()
                else root.committed("density", value)
            }
            onMoved: root.previewed("density", value)
        }
    }

    CheckBox {
        objectName: "photoFilterPreserveLuminosity"
        text: qsTr("Préserver la luminosité")
        checked: root.parameters.preserveLuminosity === undefined ? true :
                 Boolean(root.parameters.preserveLuminosity)
        onClicked: {
            root.editStarted()
            root.previewed("preserveLuminosity", checked)
            root.committed("preserveLuminosity", checked)
        }
    }
}
