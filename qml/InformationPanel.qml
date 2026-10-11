import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    objectName: "informationPanelContent"
    property point pointerPosition: Qt.point(0, 0)
    property color pointerColor: "transparent"
    property bool pointerValid: false
    property var samples: []
    property point rulerStart: Qt.point(0, 0)
    property point rulerEnd: Qt.point(0, 0)
    property bool rulerVisible: false
    property rect selection: Qt.rect(0, 0, 0, 0)
    property bool selectionVisible: false
    property int documentWidth: 0
    property int documentHeight: 0
    readonly property real dx: rulerEnd.x - rulerStart.x
    readonly property real dy: rulerEnd.y - rulerStart.y
    readonly property real distance: Math.sqrt(dx * dx + dy * dy)
    readonly property real angle: Math.atan2(dy, dx) * 180 / Math.PI

    clip: true
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    function rgb(color) {
        return "R " + Math.round(color.r * 255) + "   V " + Math.round(color.g * 255)
                + "   B " + Math.round(color.b * 255)
    }
    function pixels(value) { return value.toFixed(1) + " px" }

    ColumnLayout {
        width: root.availableWidth
        spacing: 8
        Item { Layout.preferredHeight: 1 }
        Label {
            Layout.leftMargin: 10
            text: qsTr("Sous le pointeur")
            color: "#93a5b7"
            font.pixelSize: 11
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 10; Layout.rightMargin: 10
            spacing: 8
            Rectangle {
                implicitWidth: 24; implicitHeight: 24; radius: 3
                color: root.pointerValid ? root.pointerColor : "transparent"
                border.color: "#71808e"
            }
            Label {
                objectName: "informationPointerRgb"
                Layout.fillWidth: true
                text: root.pointerValid ? root.rgb(root.pointerColor) : "R —   V —   B —"
                color: "#d5dee7"
                font.pixelSize: 11
            }
        }
        Label {
            objectName: "informationPointerCoordinates"
            Layout.leftMargin: 10
            text: root.pointerValid ? "X " + root.pointerPosition.x + " px   Y " + root.pointerPosition.y + " px"
                                    : "X —   Y —"
            color: "#bac8d5"
            font.pixelSize: 11
        }
        Rectangle { Layout.fillWidth: true; Layout.leftMargin: 10; Layout.rightMargin: 10; height: 1; color: "#3a444f" }
        ColumnLayout {
            visible: root.rulerVisible
            Layout.fillWidth: true
            Layout.leftMargin: 10; Layout.rightMargin: 10
            spacing: 6
            Label { text: qsTr("Règle"); color: "#93a5b7"; font.pixelSize: 11 }
            GridLayout {
                columns: 2
                Layout.fillWidth: true
                columnSpacing: 10; rowSpacing: 5
                Repeater {
                    model: [{name: "X", value: root.pixels(root.rulerStart.x)},
                            {name: "Y", value: root.pixels(root.rulerStart.y)},
                            {name: "L", value: root.pixels(root.dx)},
                            {name: "H", value: root.pixels(root.dy)},
                            {name: "A", value: root.angle.toFixed(1) + " °"},
                            {name: "D1", value: root.pixels(root.distance)}]
                    delegate: Label {
                        required property var modelData
                        text: modelData.name + "  " + modelData.value
                        color: "#d5dee7"
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                }
            }
        }
        ColumnLayout {
            visible: root.selectionVisible
            Layout.fillWidth: true; Layout.leftMargin: 10; Layout.rightMargin: 10
            Label { text: qsTr("Sélection"); color: "#93a5b7"; font.pixelSize: 11 }
            Label {
                text: "L " + root.pixels(root.selection.width) + "   H " + root.pixels(root.selection.height)
                color: "#d5dee7"; font.pixelSize: 11
            }
        }
        Label {
            visible: root.samples.length > 0
            Layout.leftMargin: 10
            text: qsTr("Échantillons de couleur")
            color: "#93a5b7"; font.pixelSize: 11
        }
        Repeater {
            model: root.samples
            delegate: ColumnLayout {
                required property var modelData
                required property int index
                Layout.fillWidth: true
                Layout.leftMargin: 10; Layout.rightMargin: 10
                spacing: 3
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "#" + (index + 1); color: "#93a5b7"; font.pixelSize: 11 }
                    Rectangle { implicitWidth: 16; implicitHeight: 16; radius: 2; color: modelData.color; border.color: "#71808e" }
                    Label { text: root.rgb(modelData.color); color: "#d5dee7"; font.pixelSize: 11; Layout.fillWidth: true }
                }
                Label {
                    text: "X " + Math.floor(modelData.x) + " px   Y " + Math.floor(modelData.y) + " px"
                    color: "#8697a8"; font.pixelSize: 10
                }
            }
        }
        Label {
            Layout.leftMargin: 10; Layout.rightMargin: 10; Layout.bottomMargin: 10
            Layout.fillWidth: true
            text: qsTr("Document : %1 × %2 px").arg(root.documentWidth).arg(root.documentHeight)
            color: "#8697a8"; font.pixelSize: 10
        }
    }
}
