import QtQuick

DropArea {
    id: root
    required property var document
    required property ListView listView
    property int insertion: 0
    property real indicatorY: 2
    property real pointerY: 0
    property bool validDrop: false
    keys: ["text/uri-list"]

    function updateTarget(y) {
        pointerY = y
        const contentY = y + listView.contentY
        let row = listView.indexAt(listView.width / 2, contentY)
        if (row < 0) row = listView.indexAt(listView.width / 2, contentY + listView.spacing)
        if (row < 0) row = listView.indexAt(listView.width / 2, contentY - listView.spacing)
        const item = row < 0 ? null : listView.itemAtIndex(row)
        if (!item) {
            const aboveContent = contentY < listView.originY
            insertion = listView.count && aboveContent ? document.layers[0].layerIndex + 1 : 0
            indicatorY = Math.max(2, Math.min(height - 4, aboveContent
                                  ? listView.originY - listView.contentY
                                  : listView.contentHeight - listView.contentY))
            return
        }
        const above = contentY < item.y + item.height / 2
        // Use the next visible row below a collapsed group, skipping its children.
        insertion = above ? document.layers[row].layerIndex + 1
                          : row + 1 < listView.count ? document.layers[row + 1].layerIndex + 1 : 0
        indicatorY = Math.max(2, Math.min(height - 4,
                              item.y - listView.contentY + (above ? 0 : item.height + listView.spacing / 2)))
    }

    onEntered: function(drag) {
        validDrop = drag.hasUrls && document.canImportLayerFiles(drag.urls)
        drag.accepted = validDrop
        if (validDrop) updateTarget(drag.y)
    }
    onPositionChanged: function(drag) { if (validDrop) updateTarget(drag.y) }
    onExited: validDrop = false
    onDropped: function(drop) {
        if (!validDrop || !drop.hasUrls) return
        updateTarget(drop.y)
        document.requestImportLayers(drop.urls, insertion)
        drop.accept(Qt.CopyAction)
        validDrop = false
    }

    Rectangle {
        objectName: "layerImageDropIndicator"
        visible: root.validDrop && root.containsDrag
        x: 1; y: root.indicatorY
        width: parent.width - 2; height: 3
        radius: 1
        color: "#bce8c8"
    }
    Timer {
        interval: 40
        repeat: true
        running: root.validDrop && root.containsDrag
        onTriggered: {
            const delta = root.pointerY < 28 ? -12 : root.pointerY > root.height - 28 ? 12 : 0
            if (!delta) return
            const view = root.listView
            view.contentY = Math.max(view.originY, Math.min(
                view.originY + Math.max(0, view.contentHeight - view.height), view.contentY + delta))
            root.updateTarget(root.pointerY)
        }
    }
}
