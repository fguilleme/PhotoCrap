import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    required property var document
    property string operation: "expand"
    property var previousValues: ({expand: 1, contract: 1, border: 1, smooth: 1, feather: 5})
    title: operation === "expand" ? qsTr("Dilater la sélection")
         : operation === "contract" ? qsTr("Contracter la sélection")
         : operation === "border" ? qsTr("Frange de sélection")
         : operation === "feather" ? qsTr("Contour progressif") : qsTr("Lisser la sélection")
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 350
    function edit(kind) {
        operation=kind
        amount.value=previousValues[kind] || 1
        open()
    }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Label { text: root.operation === "border" ? qsTr("Largeur :") : qsTr("Rayon :"); color: "#dbe1e8" }
            DarkSpinBox { id: amount; objectName: "selectionModifyAmount"; from: 1; to: root.operation === "feather" ? 1000 : root.operation === "border" ? 200 : 100; suffix: " px"; editable: true }
        }
        Label { text: qsTr("Cette opération modifie uniquement le contour de la sélection."); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#aab6c4" }
        Label { visible: root.operation === "border"; text: qsTr("La frange crée une bande autour du contour."); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#aab6c4" }
    }
    footer: Item {
        implicitHeight: 44
        RowLayout {
            anchors.right: parent.right; anchors.bottom: parent.bottom
            anchors.rightMargin: 12; anchors.bottomMargin: 8; spacing: 8
            DialogActionButton { text: qsTr("Annuler"); onClicked: root.reject() }
            DialogActionButton { objectName: "selectionModifyApply"; text: qsTr("OK"); defaultButton: true; onClicked: root.accept() }
        }
    }
    onAccepted: {
        previousValues[operation]=amount.value
        document.modifySelection(operation,amount.value)
    }
}
