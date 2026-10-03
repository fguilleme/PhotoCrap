import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "blackWhiteEditorRoot"
    property var parameters: ({})
    readonly property int controlCount: controls.count
    signal editStarted()
    signal previewed(string key, real value)
    signal committed(string key, real value)
    signal presetRequested(var values)
    signal autoRequested()
    signal tintToggled(bool enabled)
    signal tintColorRequested()
    spacing: 7

    ComboBox {
        id: presetBox
        objectName: "blackWhitePreset"
        Layout.fillWidth: true
        model: [qsTr("Par défaut"), qsTr("Contraste fort"), qsTr("Filtre rouge"),
                qsTr("Filtre jaune"), qsTr("Filtre vert"), qsTr("Filtre bleu"),
                qsTr("Infrarouge")]
        onActivated: {
            const presets = [
                [40,60,40,60,20,80], [70,90,20,30,-20,60], [120,80,20,20,-20,60],
                [70,120,40,30,0,50], [20,50,120,70,10,40], [20,30,20,80,140,80],
                [140,110,-20,30,-40,100]
            ]
            root.presetRequested(presets[currentIndex])
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Button {
            objectName: "blackWhiteAutoButton"
            text: qsTr("Auto")
            onClicked: root.autoRequested()
        }
        Item { Layout.fillWidth: true }
        CheckBox {
            id: tintCheck
            objectName: "blackWhiteTintCheck"
            text: qsTr("Teinte")
            checked: Boolean(root.parameters.tintEnabled)
            onClicked: root.tintToggled(checked)
        }
        Rectangle {
            width: 34; height: 24; radius: 4
            color: root.parameters.tintColor || "#d6aa78"
            border.width: 1; border.color: "#cbd2d8"
            opacity: tintCheck.checked ? 1 : .55
            MouseArea { anchors.fill: parent; onClicked: root.tintColorRequested() }
        }
    }

    Repeater {
        id: controls
        model: [
            {key:"reds", label:qsTr("Rouges"), color:"#d84a43", fallback:40},
            {key:"yellows", label:qsTr("Jaunes"), color:"#d7c33f", fallback:60},
            {key:"greens", label:qsTr("Verts"), color:"#46ad5b", fallback:40},
            {key:"cyans", label:qsTr("Cyans"), color:"#3ab7c7", fallback:60},
            {key:"blues", label:qsTr("Bleus"), color:"#4275cc", fallback:20},
            {key:"magentas", label:qsTr("Magentas"), color:"#c54ca5", fallback:80}
        ]
        delegate: ColumnLayout {
            required property var modelData
            Layout.fillWidth: true; spacing: 2
            RowLayout {
                Layout.fillWidth: true
                Label { text: modelData.label; color: "#d5dbe0"; Layout.fillWidth: true }
                Label { text: Number(slider.value).toFixed(0); color: "#aab4bd"; Layout.preferredWidth: 42; horizontalAlignment: Text.AlignRight }
            }
            Slider {
                id: slider
                Layout.fillWidth: true; Layout.preferredHeight: 24
                from: -200; to: 300; stepSize: 1
                value: root.parameters[modelData.key] === undefined ? modelData.fallback : Number(root.parameters[modelData.key])
                background: Rectangle {
                    x: slider.leftPadding; y: slider.topPadding+(slider.availableHeight-height)/2
                    width: slider.availableWidth; height: 8; radius: 4
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "#101216" }
                        GradientStop { position: .5; color: modelData.color }
                        GradientStop { position: 1; color: "#f0f1f2" }
                    }
                    border.width: 1; border.color: "#cfd4d8"
                }
                handle: Rectangle {
                    x: slider.leftPadding+slider.visualPosition*(slider.availableWidth-width)
                    y: slider.topPadding+(slider.availableHeight-height)/2
                    width: 18; height: 18; radius: 9; color: "#eef2f4"; border.width: 2; border.color: "#20262d"
                }
                onPressedChanged: { if (pressed) root.editStarted(); else root.committed(modelData.key,value) }
                onMoved: root.previewed(modelData.key,value)
            }
        }
    }
}
