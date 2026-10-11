import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    color: "#20262e"
    implicitHeight: 44
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18; anchors.rightMargin: 18
        spacing: 8
        Label { text: qsTr("Mode"); color: "#aebbc8" }
        ComboBox {
            objectName: "contentMoveMode"
            model: [qsTr("Déplacement"), qsTr("Extension")]
            currentIndex: root.document.contentMoveMode
            implicitWidth: 150; implicitHeight: 30
            onActivated: root.document.contentMoveMode = currentIndex
        }
        Label { text: qsTr("Structure"); color: "#aebbc8" }
        DarkSpinBox {
            objectName: "contentMoveStructure"
            from: 1; to: 7; implicitWidth: 80
            value: root.document.contentMoveStructure
            onValueModified: root.document.contentMoveStructure = value
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Faible : raccord souple ; élevée : préserver les motifs")
        }
        Label { text: qsTr("Couleur"); color: "#aebbc8" }
        DarkSpinBox {
            objectName: "contentMoveColor"
            from: 0; to: 10; implicitWidth: 80
            value: root.document.contentMoveColor
            onValueModified: root.document.contentMoveColor = value
            ToolTip.visible: hovered
            ToolTip.text: qsTr("0 : couleur d’origine ; 10 : adaptation maximale au fond")
        }
        CheckBox {
            objectName: "contentMoveAllLayers"
            text: qsTr("Tous les calques")
            checked: root.document.contentMoveSampleAllLayers
            onToggled: root.document.contentMoveSampleAllLayers = checked
        }
        Label {
            text: root.document.contentMoveBusy ? qsTr("Reconstruction en cours…")
                                               : qsTr("Entourez un objet, puis faites glisser la sélection")
            color: "#87929e"
            Layout.fillWidth: true; Layout.leftMargin: 12; elide: Text.ElideRight
        }
    }
}
