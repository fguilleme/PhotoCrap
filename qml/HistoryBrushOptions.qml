import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    required property var blendModes
    color: "#20262e"
    implicitHeight: 44
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18; anchors.rightMargin: 18
        spacing: 8
        Label { text: qsTr("Source"); color: "#aebbc8" }
        ComboBox {
            objectName: "historyBrushSourcePicker"
            model: root.document.history
            currentIndex: root.document.historyBrushSourceIndex
            displayText: root.document.historyBrushSourceLabel
            implicitWidth: 240; implicitHeight: 30
            onActivated: root.document.setHistoryBrushSource(currentIndex)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Choisir un état sans annuler les modifications actuelles")
        }
        Label { text: qsTr("Mode"); color: "#aebbc8" }
        ComboBox {
            objectName: "historyBrushBlend"
            model: root.blendModes
            currentIndex: root.document.brushBlend
            implicitWidth: 135; implicitHeight: 30
            onActivated: root.document.brushBlend = currentIndex
        }
        Label {
            text: root.document.historyBrushSourceAvailable
                  ? qsTr("Peignez pour restaurer cet état aux mêmes coordonnées")
                  : qsTr("Choisissez un état avec ce calque et les mêmes dimensions")
            color: root.document.historyBrushSourceAvailable ? "#87929e" : "#e1b682"
            Layout.fillWidth: true; elide: Text.ElideRight
        }
    }
}
