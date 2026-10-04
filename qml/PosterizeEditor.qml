import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "posterizeEditorRoot"
    property var parameters: ({})
    readonly property int levels: parameters.levels === undefined ? 4 : Number(parameters.levels)
    signal editStarted()
    signal previewed(string key, var value)
    signal committed(string key, var value)
    spacing: 12

    function applyLevel(value) {
        root.editStarted()
        root.previewed("levels", value)
        root.committed("levels", value)
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("Réduisez le nombre de tons pour créer des aplats plus francs.")
        color: "#9eabb6"
        wrapMode: Text.WordWrap
    }

    Rectangle {
        id: toneRamp
        objectName: "posterizeToneRamp"
        Layout.fillWidth: true
        Layout.preferredHeight: 34
        radius: 5
        color: "#1e242a"
        border.width: 1
        border.color: "#56616b"
        clip: true

        Repeater {
            model: Math.max(2, root.levels)
            delegate: Rectangle {
                required property int index
                readonly property int count: Math.max(2, root.levels)
                x: index * toneRamp.width / count
                y: 0
                width: Math.ceil(toneRamp.width / count) + 1
                height: toneRamp.height
                color: {
                    const value = index / (count - 1)
                    return Qt.rgba(value, value, value, 1)
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Label {
            text: qsTr("Niveaux par canal")
            color: "#d5dbe0"
            Layout.fillWidth: true
        }
        Label {
            text: Math.round(levelSlider.value)
            color: "#aab4bd"
            Layout.preferredWidth: 36
            horizontalAlignment: Text.AlignRight
        }
    }

    DarkSlider {
        id: levelSlider
        objectName: "posterizeLevelSlider"
        Layout.fillWidth: true
        from: 2
        to: 32
        stepSize: 1
        value: root.levels
        onPressedChanged: {
            if (pressed)
                root.editStarted()
            else
                root.committed("levels", value)
        }
        onMoved: root.previewed("levels", value)
    }

    RowLayout {
        objectName: "posterizeQuickLevels"
        Layout.fillWidth: true
        spacing: 6
        Label { text: qsTr("Valeurs rapides"); color: "#8f9ba5"; Layout.fillWidth: true }
        Repeater {
            model: [2, 4, 8, 16]
            delegate: Button {
                required property int modelData
                text: modelData
                implicitWidth: 38
                implicitHeight: 28
                highlighted: root.levels === modelData
                onClicked: root.applyLevel(modelData)
            }
        }
    }
}
