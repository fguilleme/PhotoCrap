import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "thresholdEditorRoot"
    property var histogram: []
    property var parameters: ({})
    property real level: 128
    property bool dragging: false
    signal editStarted()
    signal previewed(string key, var value)
    signal committed(string key, var value)
    spacing: 9

    function sync() {
        if (!dragging)
            level = Number(parameters.level === undefined ? 128 : parameters.level)
    }
    onParametersChanged: sync()
    Component.onCompleted: sync()

    Label {
        Layout.fillWidth: true
        text: qsTr("Les tons sous le seuil deviennent noirs ; les autres deviennent blancs.")
        color: "#9eabb6"
        wrapMode: Text.WordWrap
    }

    ThresholdHistogram {
        id: histogramCanvas
        objectName: "thresholdHistogram"
        Layout.fillWidth: true
        Layout.preferredHeight: 190
        histogram: root.histogram
        level: root.level

        MouseArea {
            anchors.fill: parent
            preventStealing: true
            cursorShape: Qt.SizeHorCursor
            onPressed: function(mouse) {
                root.dragging = true
                root.editStarted()
                updateValue(mouse.x)
            }
            function updateValue(x) {
                root.level = histogramCanvas.valueAt(x)
                root.previewed("level", root.level)
            }
            onPositionChanged: function(mouse) { if (pressed) updateValue(mouse.x) }
            onReleased: {
                root.dragging = false
                root.committed("level", root.level)
            }
            onCanceled: root.dragging = false
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Noir"); color: "#9ba6af" }
        Item { Layout.fillWidth: true }
        Label { text: qsTr("Seuil"); color: "#d5dbe0" }
        DarkSpinBox {
            objectName: "thresholdValue"
            from: 0
            to: 255
            value: Math.round(root.level)
            implicitWidth: 96
            onValueModified: {
                root.level = value
                root.editStarted()
                root.previewed("level", value)
                root.committed("level", value)
            }
        }
        Item { Layout.fillWidth: true }
        Label { text: qsTr("Blanc"); color: "#dce1e5" }
    }
}
