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
            objectName: "artHistorySourcePicker"
            model: root.document.history
            currentIndex: root.document.historyBrushSourceIndex
            displayText: root.document.historyBrushSourceLabel
            implicitWidth: 170; implicitHeight: 30
            onActivated: root.document.setHistoryBrushSource(currentIndex)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Choisir un état sans annuler les modifications actuelles")
        }
        Button {
            objectName: "artHistoryCurrentSource"
            text: qsTr("État actuel")
            implicitHeight: 30
            onClicked: root.document.setHistoryBrushSource(root.document.historyIndex)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Utiliser l’image actuelle comme source, puis peindre pour la styliser")
        }
        Label { text: qsTr("Mode"); color: "#aebbc8" }
        ComboBox {
            objectName: "artHistoryBlend"
            model: root.blendModes
            currentIndex: root.document.brushBlend
            implicitWidth: 110; implicitHeight: 30
            onActivated: root.document.brushBlend = currentIndex
        }
        Label { text: qsTr("Style"); color: "#aebbc8" }
        ComboBox {
            objectName: "artHistoryStyle"
            model: [qsTr("Serré court"), qsTr("Serré moyen"), qsTr("Serré long"), qsTr("Relâché moyen"), qsTr("Relâché long"), qsTr("Touches"), qsTr("Boucles serrées"), qsTr("Boucles serrées longues"), qsTr("Boucles relâchées"), qsTr("Boucles relâchées longues")]
            currentIndex: root.document.artHistoryStyle
            implicitWidth: 180; implicitHeight: 30
            onActivated: root.document.artHistoryStyle = currentIndex
        }
        Label { text: qsTr("Zone"); color: "#aebbc8" }
        DarkSpinBox {
            objectName: "artHistoryArea"
            from: 1; to: 500; value: root.document.artHistoryArea
            suffix: " px"; implicitWidth: 90
            onValueModified: root.document.artHistoryArea = value
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Une zone plus grande produit davantage de touches autour du pinceau")
        }
        Label { text: qsTr("Tolérance"); color: "#aebbc8" }
        DarkSpinBox {
            objectName: "artHistoryTolerance"
            from: 0; to: 100; value: root.document.artHistoryTolerance
            suffix: " %"; implicitWidth: 90
            onValueModified: root.document.artHistoryTolerance = value
            ToolTip.visible: hovered
            ToolTip.text: qsTr("0 : peindre partout ; une valeur élevée limite les touches aux zones différentes de la source")
        }
        Label {
            visible: !root.document.historyBrushSourceAvailable
            text: qsTr("Source incompatible")
            color: "#e1b682"; Layout.fillWidth: true; elide: Text.ElideRight
        }
        Item { Layout.fillWidth: true }
    }
}
