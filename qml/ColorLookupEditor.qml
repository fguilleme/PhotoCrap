import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "colorLookupEditorRoot"
    property var parameters: ({})
    signal editStarted()
    signal previewed(string key, var value)
    signal committed(string key, var value)
    signal presetRequested(int index)
    spacing: 12

    property var presets: [
        {name: qsTr("Ambre doux"), left: "#354b55", right: "#efb55f"},
        {name: qsTr("Cinéma chaud"), left: "#263f4d", right: "#e99861"},
        {name: qsTr("Bleu arctique"), left: "#263759", right: "#8fd4e9"},
        {name: qsTr("Mat"), left: "#4b4d55", right: "#c4b9aa"},
        {name: qsTr("Sarcelle et orange"), left: "#17636b", right: "#ed8b4c"},
        {name: qsTr("Pellicule passée"), left: "#5a4d59", right: "#d7b78e"}
    ]

    Label {
        Layout.fillWidth: true
        text: qsTr("Choisissez une ambiance colorimétrique, puis dosez son intensité.")
        color: "#9eabb6"
        wrapMode: Text.WordWrap
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 8
        rowSpacing: 8
        Repeater {
            model: root.presets
            delegate: Rectangle {
                required property var modelData
                required property int index
                Layout.fillWidth: true
                Layout.preferredHeight: 54
                radius: 6
                color: "#262d35"
                border.width: Number(root.parameters.preset || 0) === index ? 2 : 1
                border.color: Number(root.parameters.preset || 0) === index ? "#8fc9a4" : "#46515c"
                Rectangle {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    anchors.margins: 5; height: 22; radius: 4
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: modelData.left }
                        GradientStop { position: 1; color: modelData.right }
                    }
                }
                Label {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    anchors.leftMargin: 7; anchors.rightMargin: 7; anchors.bottomMargin: 5
                    text: modelData.name
                    color: "#dce3e8"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.presetRequested(index)
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true; spacing: 3
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Intensité"); color: "#d5dbe0"; Layout.fillWidth: true }
            Label {
                text: Math.round(strengthSlider.value) + " %"
                color: "#aab4bd"; Layout.preferredWidth: 48
                horizontalAlignment: Text.AlignRight
            }
        }
        DarkSlider {
            id: strengthSlider
            objectName: "colorLookupStrength"
            Layout.fillWidth: true
            from: 0; to: 100; stepSize: 1
            value: root.parameters.strength === undefined ? 100 : Number(root.parameters.strength)
            onPressedChanged: {
                if (pressed) root.editStarted()
                else root.committed("strength", value)
            }
            onMoved: root.previewed("strength", value)
        }
    }

    CheckBox {
        objectName: "colorLookupPreserveLuminosity"
        text: qsTr("Préserver la luminosité")
        checked: Boolean(root.parameters.preserveLuminosity)
        onClicked: {
            root.editStarted()
            root.previewed("preserveLuminosity", checked)
            root.committed("preserveLuminosity", checked)
        }
    }
}
