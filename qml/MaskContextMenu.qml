import QtQuick
import QtQuick.Controls

DarkMenu {
    id: menu
    required property var document
    required property var layerInfo
    signal propertiesRequested()

    ContextMenuItem {
        objectName: "contextMaskProperties"
        text: qsTr("Propriétés du masque…")
        enabled: menu.layerInfo.hasMask
        onTriggered: menu.propertiesRequested()
    }
    ContextMenuItem {
        objectName: "contextMaskView"
        text: menu.document.maskOnlyLayer === menu.document.activeLayer
              ? qsTr("Afficher l’image") : qsTr("Afficher le masque")
        enabled: menu.layerInfo.hasMask
        onTriggered: menu.document.toggleMaskOnlyView(menu.document.activeLayer)
    }
    MenuSeparator {}
    ContextMenuItem {
        objectName: "contextMaskEnabled"
        text: menu.layerInfo.maskEnabled ? qsTr("Désactiver le masque") : qsTr("Activer le masque")
        enabled: menu.layerInfo.hasMask && !menu.layerInfo.locked
        onTriggered: menu.document.setMaskEnabled(!menu.layerInfo.maskEnabled)
    }
    ContextMenuItem {
        objectName: "contextMaskInvert"
        text: qsTr("Inverser le masque")
        enabled: menu.layerInfo.hasMask && !menu.layerInfo.locked
        onTriggered: menu.document.invertLayerMask()
    }
    ContextMenuItem { text: qsTr("Appliquer le masque (NYI)"); enabled: false }
    ContextMenuItem {
        objectName: "contextMaskDelete"
        text: qsTr("Supprimer le masque")
        enabled: menu.layerInfo.hasMask && !menu.layerInfo.locked
        onTriggered: menu.document.removeLayerMask()
    }
    ContextMenuItem { text: qsTr("Dissocier le masque (NYI)"); enabled: false }
    MenuSeparator {}
    ContextMenuItem { text: qsTr("Charger le masque comme sélection (NYI)"); enabled: false }
    ContextMenuItem { text: qsTr("Sélectionner et masquer… (NYI)"); enabled: false }
}
