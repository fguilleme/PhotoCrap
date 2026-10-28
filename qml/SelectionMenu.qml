import QtQuick
import QtQuick.Controls

DarkMenu {
    id: root
    required property var document
    required property var manager
    required property Action selectAllAction
    required property Action deselectAction
    required property bool interactionEnabled
    signal modifyRequested(string operation)
    signal colorRequested()
    title: qsTr("Sélection")
    readonly property bool canModify: interactionEnabled && document.hasSelection && !document.selectionBusy
    readonly property Action reselectAction: Action {
        objectName: "reselectAction"; text: qsTr("Resélectionner")
        shortcut: (root.manager.revision, root.manager.sequence("reselect"))
        enabled: root.interactionEnabled && root.document.canReselect && !root.document.hasSelection
        onTriggered: root.document.reselect()
    }
    readonly property Action inverseAction: Action {
        objectName: "inverseSelectionAction"; text: qsTr("Intervertir")
        shortcut: (root.manager.revision, root.manager.sequence("invertSelection"))
        enabled: root.canModify; onTriggered: root.document.invertSelection()
    }
    readonly property Action allLayersAction: Action {
        text: qsTr("Tous les calques"); shortcut: (root.manager.revision, root.manager.sequence("selectAllLayers"))
        enabled: root.interactionEnabled; onTriggered: root.document.selectAllLayers()
    }
    readonly property Action similarLayersAction: Action {
        text: qsTr("Calques similaires"); shortcut: (root.manager.revision, root.manager.sequence("selectSimilarLayers"))
        enabled: root.interactionEnabled && root.document.activeLayer>=0
        onTriggered: root.document.selectSimilarLayers()
    }
    readonly property Action borderAction: Action {
        text: qsTr("Frange…"); shortcut: (root.manager.revision, root.manager.sequence("selectBorder")); enabled: root.canModify
        onTriggered: root.modifyRequested("border")
    }
    readonly property Action smoothAction: Action {
        text: qsTr("Lisser…"); shortcut: (root.manager.revision, root.manager.sequence("selectSmooth")); enabled: root.canModify
        onTriggered: root.modifyRequested("smooth")
    }
    readonly property Action expandAction: Action {
        objectName: "expandSelectionAction"; text: qsTr("Dilater…"); shortcut: (root.manager.revision, root.manager.sequence("selectExpand")); enabled: root.canModify
        onTriggered: root.modifyRequested("expand")
    }
    readonly property Action contractAction: Action {
        text: qsTr("Contracter…"); shortcut: (root.manager.revision, root.manager.sequence("selectContract")); enabled: root.canModify
        onTriggered: root.modifyRequested("contract")
    }
    readonly property Action featherAction: Action {
        objectName: "featherSelectionAction"; text: qsTr("Contour progressif…"); shortcut: (root.manager.revision, root.manager.sequence("feather")); enabled: root.canModify
        onTriggered: root.modifyRequested("feather")
    }
    readonly property Action maskAction: Action {
        text: qsTr("Sélectionner et masquer… (NYI)"); shortcut: (root.manager.revision, root.manager.sequence("selectMask")); enabled: false
    }
    readonly property Action quickMaskAction: Action {
        text: qsTr("Mode Masque (NYI)"); shortcut: (root.manager.revision, root.manager.sequence("quickMask")); enabled: false
    }
    readonly property Action edgesAction: Action {
        objectName: "selectionEdgesAction"; text: qsTr("Contour de la sélection")
        shortcut: (root.manager.revision, root.manager.sequence("selectionEdges"))
        enabled: root.interactionEnabled; checkable: true; checked: root.document.selectionEdgesVisible
        onTriggered: root.document.selectionEdgesVisible=checked
    }
    DarkMenuItem { action: root.selectAllAction }
    DarkMenuItem { action: root.deselectAction }
    DarkMenuItem { action: root.reselectAction }
    DarkMenuItem { action: root.inverseAction }
    MenuSeparator {}
    DarkMenuItem { action: root.allLayersAction }
    DarkMenuItem { text: qsTr("Désélectionner les calques (NYI)"); enabled: false }
    DarkMenuItem { action: root.similarLayersAction }
    DarkMenuItem { text: qsTr("Rechercher les calques… (NYI)"); enabled: false }
    DarkMenuItem { text: qsTr("Isoler les calques (NYI)"); enabled: false }
    MenuSeparator {}
    DarkMenuItem { action: Action {
        objectName: "colorSelectionAction"; text: qsTr("Plage de couleurs…")
        shortcut: (root.manager.revision,root.manager.sequence("selectColorRange")); enabled: root.interactionEnabled && !root.document.selectionBusy
        onTriggered: root.colorRequested()
    } }
    DarkMenuItem { text: qsTr("Zone de mise au point… (NYI)"); enabled: false }
    DarkMenuItem { text: qsTr("Sujet (NYI)"); enabled: false }
    DarkMenuItem { text: qsTr("Ciel (NYI)"); enabled: false }
    MenuSeparator {}
    DarkMenuItem { action: root.maskAction }
    DarkMenu {
        objectName: "selectionModifyMenu"; title: qsTr("Modifier"); enabled: root.canModify
        DarkMenuItem { action: root.borderAction }
        DarkMenuItem { action: root.smoothAction }
        DarkMenuItem { action: root.expandAction }
        DarkMenuItem { action: root.contractAction }
        DarkMenuItem { action: root.featherAction }
    }
    MenuSeparator {}
    DarkMenuItem { text: qsTr("Étendre (NYI)"); enabled: false }
    DarkMenuItem { text: qsTr("Généraliser (NYI)"); enabled: false }
    MenuSeparator {}
    DarkMenuItem { text: qsTr("Transformer la sélection (NYI)"); enabled: false }
    MenuSeparator {}
    DarkMenuItem { action: root.quickMaskAction }
    MenuSeparator {}
    DarkMenuItem { text: qsTr("Récupérer la sélection… (NYI)"); enabled: false }
    DarkMenuItem { text: qsTr("Mémoriser la sélection… (NYI)"); enabled: false }
}
