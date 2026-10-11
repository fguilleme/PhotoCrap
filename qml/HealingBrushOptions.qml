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
            objectName: "healingMode"
            model: [qsTr("Normal"), qsTr("Remplacer"), qsTr("Produit"), qsTr("Écran"), qsTr("Obscurcir"), qsTr("Éclaircir"), qsTr("Couleur"), qsTr("Luminosité")]
            currentIndex: root.document.healingMode
            implicitWidth: 120; implicitHeight: 30
            onActivated: root.document.healingMode = currentIndex
        }
        CheckBox {
            objectName: "healingAligned"
            text: qsTr("Aligné")
            checked: root.document.healingAligned
            onToggled: root.document.healingAligned = checked
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Garder le décalage de source entre les gestes")
        }
        ComboBox {
            objectName: "healingSampleMode"
            model: [qsTr("Calque actif"), qsTr("Actif et inférieurs"), qsTr("Tous les calques")]
            currentIndex: root.document.healingSampleMode
            implicitWidth: 165; implicitHeight: 30
            onActivated: root.document.healingSampleMode = currentIndex
        }
        CheckBox {
            objectName: "healingIgnoreAdjustments"
            text: qsTr("Ignorer les réglages")
            enabled: root.document.healingSampleMode !== 0
            checked: root.document.healingIgnoreAdjustments
            onToggled: root.document.healingIgnoreAdjustments = checked
        }
        Label { text: qsTr("Diffusion"); color: "#aebbc8" }
        DarkSpinBox {
            objectName: "healingDiffusion"
            from: 1; to: 7
            value: root.document.healingDiffusion
            implicitWidth: 82
            onValueModified: root.document.healingDiffusion = value
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Faible : textures détaillées ; élevée : zones lisses")
        }
        Label {
            text: qsTr("Option/Alt-clic pour prélever la source")
            color: root.document.healingSourceValid ? "#87929e" : "#d6bd83"
            Layout.fillWidth: true; elide: Text.ElideRight
        }
    }
}
