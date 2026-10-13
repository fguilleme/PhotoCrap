import QtQuick
import QtQuick.Controls

DarkMenu {
    id: menu
    required property var document
    required property var layerInfo
    property bool propertiesAvailable: false
    readonly property bool singleLayer: document.selectedLayerCount === 1
    signal propertiesRequested()
    signal renameRequested()

    ContextMenuItem {
        objectName: "contextLayerProperties"
        text: qsTr("Propriétés…")
        enabled: menu.singleLayer && menu.propertiesAvailable
        onTriggered: menu.propertiesRequested()
    }
    ContextMenuItem {
        text: qsTr("Options de fusion… (NYI)")
        enabled: false
    }
    MenuSeparator {}
    ContextMenuItem {
        text: qsTr("Dupliquer le calque")
        enabled: menu.singleLayer && menu.layerInfo.kind !== "group"
        onTriggered: menu.document.duplicateLayer()
    }
    ContextMenuItem {
        text: qsTr("Renommer…")
        enabled: menu.singleLayer
        onTriggered: menu.renameRequested()
    }
    ContextMenuItem {
        text: qsTr("Supprimer le calque")
        enabled: menu.singleLayer && menu.document.layers.length > 1 && !menu.layerInfo.locked
        onTriggered: menu.document.removeLayer()
    }
    ContextMenuItem {
        objectName: "contextLayerLock"
        text: menu.layerInfo.locked ? qsTr("Déverrouiller le calque") : qsTr("Verrouiller le calque")
        enabled: menu.singleLayer
        onTriggered: menu.document.setLayerLocked(menu.document.activeLayer, !menu.layerInfo.locked)
    }
    ContextMenuItem { text: qsTr("Couleur du calque… (NYI)"); enabled: false }
    MenuSeparator {}
    ContextMenuItem {
        text: menu.layerInfo.clippingMask ? qsTr("Libérer le masque d’écrêtage")
                                         : qsTr("Créer un masque d’écrêtage")
        enabled: menu.singleLayer && menu.layerInfo.kind !== "group" &&
                 (menu.layerInfo.clippingMask || menu.document.activeLayer > 0)
        onTriggered: menu.document.setClippingMask(!menu.layerInfo.clippingMask)
    }
    ContextMenuItem {
        text: qsTr("Grouper les calques sélectionnés")
        enabled: menu.document.canGroupSelected
        onTriggered: menu.document.groupSelectedLayers()
    }
    ContextMenuItem {
        text: qsTr("Sortir du groupe")
        enabled: menu.singleLayer && menu.document.canUngroupActive
        onTriggered: menu.document.ungroupActiveLayers()
    }
    DarkMenu {
        title: qsTr("Ajouter un masque")
        enabled: menu.singleLayer && menu.layerInfo.kind !== "group" &&
                 !menu.layerInfo.hasMask && !menu.layerInfo.locked
        ContextMenuItem {
            text: menu.document.hasSelection ? qsTr("Faire apparaître la sélection") : qsTr("Tout faire apparaître")
            onTriggered: menu.document.addLayerMask(true)
        }
        ContextMenuItem {
            text: menu.document.hasSelection ? qsTr("Masquer la sélection") : qsTr("Tout masquer")
            onTriggered: menu.document.addLayerMask(false)
        }
    }
    MenuSeparator {}
    ContextMenuItem { text: qsTr("Convertir en objet dynamique (NYI)"); enabled: false }
    ContextMenuItem { text: qsTr("Pixelliser le calque (NYI)"); enabled: false }
    MenuSeparator {}
    ContextMenuItem {
        text: qsTr("Fusionner avec le calque inférieur")
        enabled: menu.singleLayer && menu.document.activeLayer > 0 &&
                 menu.layerInfo.kind === "pixel" && !menu.layerInfo.locked
        onTriggered: menu.document.mergeDown()
    }
    ContextMenuItem { text: qsTr("Fusionner les calques sélectionnés (NYI)"); enabled: false }
    ContextMenuItem { text: qsTr("Fusionner les calques visibles (NYI)"); enabled: false }
    ContextMenuItem { text: qsTr("Aplatir l’image (NYI)"); enabled: false }
    MenuSeparator {}
    ContextMenuItem {
        text: menu.layerInfo.visible ? qsTr("Masquer le calque") : qsTr("Afficher le calque")
        enabled: menu.singleLayer
        onTriggered: menu.document.setLayerVisible(menu.document.activeLayer, !menu.layerInfo.visible)
    }
}
