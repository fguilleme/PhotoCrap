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
            objectName: "spotHealingMode"
            model: [qsTr("Normal"), qsTr("Remplacer"), qsTr("Produit"), qsTr("Écran"), qsTr("Obscurcir"), qsTr("Éclaircir"), qsTr("Couleur"), qsTr("Luminosité")]
            currentIndex: root.document.spotHealingMode
            implicitWidth: 120; implicitHeight: 30
            onActivated: root.document.spotHealingMode = currentIndex
        }
        Label { text: qsTr("Correspondance de proximité"); color: "#c4cdd5" }
        CheckBox {
            objectName: "spotHealingSampleAllLayers"
            text: qsTr("Tous les calques")
            checked: root.document.spotHealingSampleAllLayers
            onToggled: root.document.spotHealingSampleAllLayers = checked
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Prélever tous les calques visibles pour retoucher sur un calque vide")
        }
        Label {
            text: qsTr("Peindre sur le défaut ; utiliser une brosse un peu plus grande")
            color: "#87929e"
            Layout.fillWidth: true; elide: Text.ElideRight
        }
    }
}
