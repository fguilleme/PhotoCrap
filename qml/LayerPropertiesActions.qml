import QtQuick
import QtQuick.Layouts

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
        glyph: "↳"
        hint: root.clippingMask ? qsTr("Libérer le masque d’écrêtage")
                                : qsTr("Créer un masque d’écrêtage")
        selected: root.clippingMask
        enabled: root.canClip
        onClicked: root.clippingToggled()
    }
    IconButton {
        objectName: "layerPropertiesVisibilityButton"
        glyph: root.layerVisible ? "◉" : "○"
        hint: root.layerVisible ? qsTr("Désactiver le calque") : qsTr("Activer le calque")
        onClicked: root.visibilityToggled()
    }
    IconButton {
        objectName: "layerPropertiesResetButton"
        glyph: "↶"
        hint: qsTr("Réinitialiser le réglage")
        onClicked: root.resetRequested()
    }
    IconButton {
        objectName: "layerPropertiesDeleteButton"
        hint: qsTr("Supprimer le calque")
        enabled: root.canDelete
        contentItem: TrashIcon {
            iconColor: parent.enabled ? (parent.hovered ? "#ffffff" : "#c0c7d0") : "#545d68"
        }
        onClicked: root.deleteRequested()
    }
}
