import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "pathsPanelContent"
    required property var document
    property int keyboardModifiers: 0
    signal editCommitted()
    signal editingRequested()
    signal modalStateChanged(bool open)
    readonly property bool hasPath: document.activePenPath >= 0
    readonly property var activePath: hasPath ? document.penPaths[document.activePenPath] : null
    readonly property bool hasPoints: activePath !== null && activePath.points.length > 0
    readonly property bool rasterTarget: {
        for (const layer of document.layers)
            if (layer.layerIndex === document.activeLayer)
                return layer.kind === "pixel" || layer.editingMask
        return false
    }
    readonly property bool maskTarget: {
        if (document.editingVectorMask) return true
        for (const layer of document.layers)
            if (layer.layerIndex === document.activeLayer)
                return layer.editingMask
        return false
    }
    readonly property bool canMakeShape: hasPoints && !activePath.vectorMask && activePath.points.length >= 2 && !maskTarget
    function makeShape() {
        if (document.penPathToShape()) editingRequested()
        editCommitted()
    }
    property bool creating: false
    function namePath(create) {
        if (!create && activePath.layerMask) return
        creating = create
        pathName.text = create ? "" : activePath.name
        rename.open()
    }
    // Selecting a path rebuilds the list synchronously. Keep the remaining
    // click handling in the panel's context, which outlives its delegates.
    function selectEntry(index, button, modifiers) {
        document.setActivePenPath(index)
        if (button === Qt.RightButton) menu.popup()
        else if (modifiers & Qt.ControlModifier)
            document.penPathToSelection(selectionMode(modifiers))
        else editingRequested()
        editCommitted()
    }
    function selectionMode(modifiers) {
        return modifiers & Qt.ShiftModifier ? (modifiers & Qt.AltModifier ? 3 : 1)
                                             : (modifiers & Qt.AltModifier ? 2 : 0)
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        ListView {
            id: list
            objectName: "pathList"
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            model: root.document.penPaths
            currentIndex: root.document.activePenPath
            spacing: 2
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                objectName: "pathEntry_" + index
                width: list.width; height: 54
                color: index === root.document.activePenPath ? "#354d43" : "#292f38"
                border.width: index === root.document.activePenPath ? 1 : 0
                border.color: "#78a589"
                RowLayout {
                    anchors.fill: parent; anchors.margins: 6; spacing: 10
                    PathThumbnail {
                        pathData: row.modelData
                        documentWidth: root.document.width
                        documentHeight: root.document.height
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.name
                            color: "#d6e0db"; elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.layerMask ? qsTr("Masque vectoriel · contour modifiable") : row.modelData.vectorMask ? qsTr("Forme · remplissage modifiable") : qsTr("Tracé seul · sans remplissage lié")
                            color: "#aab7c5"; font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(mouse) {
                        root.selectEntry(row.index, mouse.button, mouse.modifiers)
                    }
                    onDoubleClicked: function(mouse) {
                        if (!(mouse.modifiers & Qt.ControlModifier)) root.namePath(false)
                    }
                }
            }
            // Blank space deselects the outline without discarding it.
            MouseArea {
                x: 0; y: Math.max(0,list.contentHeight-list.contentY)
                width: list.width; height: Math.max(0,list.height-y)
                onClicked: { root.document.setActivePenPath(-1); root.editCommitted() }
            }
            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("Dessinez avec la Plume (P)")
                color: "#8d9ba8"
                font.pixelSize: 11
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#424b54" }
        RowLayout {
            visible: root.hasPath && !root.activePath.vectorMask && !root.maskTarget
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Pour un remplissage qui suit les points :")
                wrapMode: Text.WordWrap; color: "#aab7c5"; font.pixelSize: 10
            }
            Button {
                objectName: "pathMakeShapeButton"
                text: qsTr("Créer une forme")
                implicitHeight: 28
                enabled: root.canMakeShape
                onClicked: root.makeShape()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 0
            IconButton { objectName: "pathFillButton"; glyph: "●"; hint: qsTr("Peindre le remplissage dans les pixels (sans lien avec le tracé)"); enabled: root.hasPoints && root.rasterTarget; implicitWidth: 32; onClicked: {root.document.fillPenPath();root.editCommitted()} }
            IconButton { glyph: "○"; hint: qsTr("Contour du tracé avec le pinceau"); enabled: root.hasPoints && root.rasterTarget; implicitWidth: 32; onClicked: {root.document.strokePenPath();root.editCommitted()} }
            IconButton { objectName: "pathSelectionPanelButton"; glyph: "◌"; hint: qsTr("Convertir en sélection (Maj : ajouter, Alt : soustraire)"); enabled: root.hasPoints; implicitWidth: 32; onClicked: {root.document.penPathToSelection(root.selectionMode(root.keyboardModifiers));root.editCommitted()} }
            Item { Layout.fillWidth: true }
            IconButton { objectName: "newPathPanelButton"; glyph: "+"; hint: qsTr("Nouveau tracé"); implicitWidth: 32; onClicked: {root.document.createPenPath();root.editCommitted()} }
            IconButton { objectName: "deletePathPanelButton"; hint: qsTr("Supprimer le tracé"); enabled: root.hasPath; implicitWidth: 32; contentItem: TrashIcon {} onClicked: {root.document.deletePenPath();root.editCommitted()} }
            IconButton { glyph: "…"; hint: qsTr("Options des tracés"); implicitWidth: 32; onClicked: menu.popup() }
        }
    }
    DarkMenu {
        id: menu
        ContextMenuItem { text: qsTr("Nouveau tracé…"); onTriggered: root.namePath(true) }
        ContextMenuItem { text: qsTr("Renommer le tracé…"); enabled: root.hasPath && !root.activePath.layerMask; onTriggered: root.namePath(false) }
        ContextMenuItem { text: qsTr("Dupliquer le tracé"); enabled: root.hasPath; onTriggered: {root.document.duplicatePenPath();root.editCommitted()} }
        ContextMenuItem { text: qsTr("Monter le tracé"); enabled: root.document.activePenPath > 0; onTriggered: root.document.movePenPath(root.document.activePenPath-1) }
        ContextMenuItem { text: qsTr("Descendre le tracé"); enabled: root.hasPath && root.document.activePenPath < list.count-1; onTriggered: root.document.movePenPath(root.document.activePenPath+1) }
        MenuSeparator {}
        ContextMenuItem { text: qsTr("Définir une sélection"); enabled: root.hasPoints; onTriggered: {root.document.penPathToSelection();root.editCommitted()} }
        ContextMenuItem { text: qsTr("Créer une forme depuis le tracé"); enabled: root.canMakeShape; onTriggered: root.makeShape() }
        ContextMenuItem { text: qsTr("Peindre le remplissage dans les pixels…"); enabled: root.hasPoints && root.rasterTarget; onTriggered: {root.document.fillPenPath();root.editCommitted()} }
        ContextMenuItem { text: qsTr("Contour du tracé avec le pinceau"); enabled: root.hasPoints && root.rasterTarget; onTriggered: {root.document.strokePenPath();root.editCommitted()} }
        MenuSeparator {}
        ContextMenuItem { text: qsTr("Supprimer le tracé"); enabled: root.hasPath; onTriggered: {root.document.deletePenPath();root.editCommitted()} }
    }
    Dialog {
        id: rename
        objectName: "pathNameDialog"
        parent: Overlay.overlay; anchors.centerIn: parent
        title: root.creating ? qsTr("Nouveau tracé") : qsTr("Renommer le tracé")
        modal: true
        onOpened: {root.modalStateChanged(true);pathName.forceActiveFocus();pathName.selectAll()}
        onClosed: root.modalStateChanged(false)
        TextField { id: pathName; objectName: "pathNameField"; selectByMouse: true; maximumLength: 256; onAccepted: rename.accept() }
        footer: DialogButtonBox {
            DialogActionButton { text: qsTr("Annuler"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole; onClicked: rename.reject() }
            DialogActionButton { text: qsTr("OK"); defaultButton: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: rename.accept() }
        }
        onAccepted: {
            if (root.creating) root.document.createPenPath(pathName.text)
            else root.document.renamePenPath(pathName.text)
            root.editCommitted()
        }
    }
}
