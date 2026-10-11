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
        CheckBox {
            objectName: "removeSubtract"
            text: qsTr("Retirer des marques")
            checked: root.document.removeSubtract
            onToggled: root.document.removeSubtract = checked
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Alt permet aussi de retirer une zone marquée")
        }
        CheckBox {
            objectName: "removeAllLayers"
            text: qsTr("Tous les calques")
            checked: root.document.removeSampleAllLayers
            onToggled: root.document.removeSampleAllLayers = checked
        }
        CheckBox {
            objectName: "removeAutomatic"
            text: qsTr("Après chaque tracé")
            checked: root.document.removeAfterEachStroke
            onToggled: root.document.removeAfterEachStroke = checked
        }
        Button {
            objectName: "removeApply"
            text: qsTr("Appliquer")
            implicitHeight: 30
            enabled: root.document.removeHasPending && !root.document.removeBusy
            onClicked: root.document.applyRemoval()
        }
        Button {
            objectName: "removeCancel"
            text: qsTr("Annuler")
            implicitHeight: 30
            enabled: root.document.removeHasPending || root.document.removeBusy
            onClicked: root.document.cancelRemoval()
        }
        Label {
            text: root.document.removeBusy ? qsTr("Reconstruction en cours…")
                                          : qsTr("Peignez ou entourez l’objet à supprimer")
            color: "#87929e"
            Layout.fillWidth: true; elide: Text.ElideRight
        }
    }
}
