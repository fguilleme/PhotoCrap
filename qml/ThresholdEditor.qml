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
        histogramCanvas.requestPaint()
    }
    onParametersChanged: sync()
    onHistogramChanged: histogramCanvas.requestPaint()
    Component.onCompleted: sync()

    Label {
        Layout.fillWidth: true
        text: qsTr("Les tons sous le seuil deviennent noirs ; les autres deviennent blancs.")
        color: "#9eabb6"
        wrapMode: Text.WordWrap
    }

    Canvas {
        id: histogramCanvas
        objectName: "thresholdHistogram"
        Layout.fillWidth: true
        Layout.preferredHeight: 190
        property real leftEdge: 12
        property real rightEdge: width - 12
        property real topEdge: 8
        property real bottomEdge: height - 30

        function px(value) {
            return leftEdge + value / 255 * (rightEdge - leftEdge)
        }
        function valueAt(pixel) {
            return Math.round(Math.max(0, Math.min(255,
                (pixel - leftEdge) / (rightEdge - leftEdge) * 255)))
        }
        onWidthChanged: requestPaint()
        onPaint: {
            const c = getContext("2d")
            c.reset()
            c.fillStyle = "#151a20"
            c.fillRect(0, 0, width, height)
            c.strokeStyle = "#303943"
            for (let i = 0; i <= 4; ++i) {
                const x = leftEdge + (rightEdge - leftEdge) * i / 4
                c.beginPath(); c.moveTo(x, topEdge); c.lineTo(x, bottomEdge); c.stroke()
            }
            if (root.histogram && root.histogram.length === 256) {
                c.beginPath(); c.moveTo(leftEdge, bottomEdge)
                for (let bin = 0; bin < 256; ++bin)
                    c.lineTo(px(bin), bottomEdge - Number(root.histogram[bin]) *
                             (bottomEdge - topEdge) * .92)
                c.lineTo(rightEdge, bottomEdge); c.closePath()
                c.fillStyle = "rgba(206,216,225,.48)"; c.fill()
            }
            const split = px(root.level)
            c.fillStyle = "#050607"; c.fillRect(leftEdge, bottomEdge + 5, split - leftEdge, 12)
            c.fillStyle = "#f2f3f4"; c.fillRect(split, bottomEdge + 5, rightEdge - split, 12)
            c.strokeStyle = "#70c995"; c.lineWidth = 2
            c.beginPath(); c.moveTo(split, topEdge); c.lineTo(split, bottomEdge + 19); c.stroke()
            c.beginPath(); c.moveTo(split, bottomEdge + 20)
            c.lineTo(split - 7, bottomEdge + 28); c.lineTo(split + 7, bottomEdge + 28)
            c.closePath(); c.fillStyle = "#70c995"; c.fill()
            c.lineWidth = 1; c.strokeStyle = "#66727d"
            c.strokeRect(leftEdge, topEdge, rightEdge - leftEdge, bottomEdge - topEdge)
        }

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
                histogramCanvas.requestPaint()
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
                histogramCanvas.requestPaint()
            }
        }
        Item { Layout.fillWidth: true }
        Label { text: qsTr("Blanc"); color: "#dce1e5" }
    }
}
