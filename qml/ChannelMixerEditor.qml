import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "channelMixerEditorRoot"
    property var parameters: ({})
    property int outputChannel: 0
    readonly property bool monochrome: Boolean(parameters.monochrome)
    readonly property var channelNames: [qsTr("Rouge"), qsTr("Vert"), qsTr("Bleu")]
    readonly property var channelPrefixes: ["red", "green", "blue"]
    readonly property var sourceSuffixes: ["Red", "Green", "Blue"]
    readonly property var sourceColors: ["#d84b45", "#42ad61", "#4879d5"]
    property var liveSources: [100, 0, 0]
    readonly property int total: Math.round(liveSources[0] + liveSources[1] + liveSources[2])
    signal editStarted()
    signal previewed(string key, var value)
    signal committed(string key, var value)
    signal presetRequested(var values)
    spacing: 9

    function sourceKey(index) {
        return monochrome ? "mono" + sourceSuffixes[index] :
                            channelPrefixes[outputChannel] + sourceSuffixes[index]
    }
    function sourceValue(index) {
        const fallback = monochrome ? [40,40,20][index] : (index === outputChannel ? 100 : 0)
        const key = sourceKey(index)
        return parameters[key] === undefined ? fallback : Number(parameters[key])
    }
    function constantKey() {
        return monochrome ? "monoConstant" : channelPrefixes[outputChannel] + "Constant"
    }
    function constantValue() {
        return parameters[constantKey()] === undefined ? 0 : Number(parameters[constantKey()])
    }
    function setLiveSource(index, value) {
        const values = liveSources.slice()
        values[index] = Number(value)
        liveSources = values
    }
    function presetValues(monochrome, red, green, blue) {
        return {"redRed":100, "redGreen":0, "redBlue":0,
                "greenRed":0, "greenGreen":100, "greenBlue":0,
                "blueRed":0, "blueGreen":0, "blueBlue":100,
                "redConstant":0, "greenConstant":0, "blueConstant":0,
                "monochrome":monochrome, "monoRed":red, "monoGreen":green,
                "monoBlue":blue, "monoConstant":0}
    }

    ComboBox {
        id: presetBox
        objectName: "channelMixerPreset"
        Layout.fillWidth: true
        model: [qsTr("Par défaut"), qsTr("Noir et blanc infrarouge"),
                qsTr("Noir et blanc · filtre bleu"), qsTr("Noir et blanc · filtre vert"),
                qsTr("Noir et blanc · filtre orange"), qsTr("Noir et blanc · filtre rouge"),
                qsTr("Noir et blanc · filtre jaune")]
        onActivated: {
            const mixes = [[false,40,40,20], [true,-70,200,-30], [true,0,0,100],
                           [true,0,100,0], [true,50,50,0], [true,100,0,0],
                           [true,34,66,0]]
            const mix = mixes[currentIndex]
            root.presetRequested(root.presetValues(mix[0], mix[1], mix[2], mix[3]))
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Canal de sortie"); color: "#d5dbe0" }
        ComboBox {
            id: outputBox
            objectName: "channelMixerOutput"
            Layout.fillWidth: true
            model: root.channelNames
            currentIndex: root.outputChannel
            enabled: !root.monochrome
            onActivated: root.outputChannel = currentIndex
        }
    }

    CheckBox {
        objectName: "channelMixerMonochrome"
        text: qsTr("Monochrome")
        checked: root.monochrome
        onClicked: {
            root.editStarted()
            root.previewed("monochrome", checked)
            root.committed("monochrome", checked)
        }
    }

    Repeater {
        model: 3
        delegate: ColumnLayout {
            required property int index
            Layout.fillWidth: true; spacing: 2
            RowLayout {
                Layout.fillWidth: true
                Label { text: root.channelNames[index]; color: root.sourceColors[index]; Layout.fillWidth: true }
                Label { text: Math.round(channelSlider.value) + " %"; color: "#aab4bd"; Layout.preferredWidth: 54; horizontalAlignment: Text.AlignRight }
            }
            Slider {
                id: channelSlider
                Layout.fillWidth: true; Layout.preferredHeight: 24
                from: -200; to: 200; stepSize: 1
                value: root.sourceValue(index)
                onValueChanged: root.setLiveSource(index, value)
                background: Rectangle {
                    x: channelSlider.leftPadding; y: channelSlider.topPadding + (channelSlider.availableHeight-height)/2
                    width: channelSlider.availableWidth; height: 8; radius: 4
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "#15191e" }
                        GradientStop { position: .5; color: root.sourceColors[index] }
                        GradientStop { position: 1; color: "#f0f2f3" }
                    }
                    border.width: 1; border.color: "#cfd4d8"
                }
                handle: Rectangle {
                    x: channelSlider.leftPadding + channelSlider.visualPosition*(channelSlider.availableWidth-width)
                    y: channelSlider.topPadding + (channelSlider.availableHeight-height)/2
                    width: 18; height: 18; radius: 9; color: "#eef2f4"; border.width: 2; border.color: "#20262d"
                }
                onPressedChanged: {
                    if (pressed) root.editStarted()
                    else root.committed(root.sourceKey(index), value)
                }
                onMoved: root.previewed(root.sourceKey(index), value)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Total"); color: "#aab4bd"; Layout.fillWidth: true }
        Label {
            objectName: "channelMixerTotal"
            text: (root.total > 100 ? "⚠  " : "") + root.total + " %"
            color: root.total > 100 ? "#f0b45b" : "#aab4bd"
        }
    }

    ColumnLayout {
        Layout.fillWidth: true; spacing: 2
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Constante"); color: "#d5dbe0"; Layout.fillWidth: true }
            Label { text: Math.round(constantSlider.value) + " %"; color: "#aab4bd"; Layout.preferredWidth: 54; horizontalAlignment: Text.AlignRight }
        }
        DarkSlider {
            id: constantSlider
            objectName: "channelMixerConstant"
            Layout.fillWidth: true
            from: -200; to: 200; stepSize: 1
            value: root.constantValue()
            onPressedChanged: {
                if (pressed) root.editStarted()
                else root.committed(root.constantKey(), value)
            }
            onMoved: root.previewed(root.constantKey(), value)
        }
    }

}
