import QtQuick
import QtQuick.Layouts
import "Studio.js" as Studio

RowLayout {
    id: root

    property bool layerVisible: true
    property bool clippingMask: false
    property bool canClip: true
    property bool canDelete: true
    signal visibilityToggled()
    signal clippingToggled()
    signal resetRequested()
    signal deleteRequested()

    spacing: 2

    IconButton {
        objectName: "layerPropertiesClipButton"
        toolName: "clipLayer"
        hint: root.clippingMask ? qsTr("Libérer le masque d’écrêtage")
                                : qsTr("Créer un masque d’écrêtage")
        selected: root.clippingMask
        enabled: root.canClip
        onClicked: root.clippingToggled()
    }
    IconButton {
        objectName: "layerPropertiesVisibilityButton"
        toolName: root.layerVisible ? "eye" : "eyeOff"
        hint: root.layerVisible ? qsTr("Désactiver le calque") : qsTr("Activer le calque")
        onClicked: root.visibilityToggled()
    }
    IconButton {
        objectName: "layerPropertiesResetButton"
        toolName: "resetAdjustment"
        hint: qsTr("Réinitialiser le réglage")
        onClicked: root.resetRequested()
    }
    IconButton {
        id: deleteButton
        objectName: "layerPropertiesDeleteButton"
        hint: qsTr("Supprimer le calque")
        enabled: root.canDelete
        contentItem: Item {
            TrashIcon {
                anchors.centerIn: parent
                width: implicitWidth
                height: implicitHeight
                iconColor: !deleteButton.enabled ? Studio.disabled : deleteButton.hovered ? "#ffaca6" : "#f27f78"
            }
        }
        onClicked: root.deleteRequested()
    }
}
