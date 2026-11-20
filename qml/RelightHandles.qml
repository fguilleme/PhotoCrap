import QtQuick
import "Studio.js" as Studio

Item {
    id: root
    required property var panel
    required property real zoom
    required property real imageWidth
    required property real imageHeight
    required property Item workspace
    required property Item settingsPanel
    required property Item rightDock
    readonly property real imageLeft: (width - imageWidth) / 2
    readonly property real imageTop: (height - imageHeight) / 2
    signal dragStarted()
    signal dragCanceled()

    Item {
        parent: root.workspace
        anchors.fill: parent
        z: 100
        visible: root.visible
        HoverHandler {
            id: pointer
            property point lastPosition: Qt.point(-1, -1)
            onPointChanged: {
                const pos = point.scenePosition
                if (pos.x === lastPosition.x && pos.y === lastPosition.y) return
                lastPosition = pos
                markerTimeout.restart()
            }
            onHoveredChanged: {
                if (hovered) markerTimeout.restart()
                else markerTimeout.stop()
            }
        }
    }
    Timer { id: markerTimeout; interval: 1500 }
    function pointerOver(item) {
        return item && item.visible && item.contains(item.mapFromItem(null, pointer.point.scenePosition))
    }
    readonly property bool overPanel: pointerOver(settingsPanel) || pointerOver(rightDock)

    component LightHandle: Item {
        id: handle
        required property int lampIndex
        property bool target: false
        objectName: target ? "relightTarget" : "relightHandle" + lampIndex
        readonly property var light: root.panel.lights[lampIndex]
        readonly property var center: light ? root.panel.canvasPoint(target ? {x: light.targetX ?? 50, y: light.targetY ?? 50} : light) : Qt.point(0, 0)
        readonly property bool selected: root.panel.index === lampIndex
        property point draggingCenter
        property point grabOffset
        property bool dragging: false
        property bool moved: false
        width: 24; height: 24
        x: root.imageLeft + (dragging ? draggingCenter.x : center.x) * root.zoom - width / 2
        y: root.imageTop + (dragging ? draggingCenter.y : center.y) * root.zoom - height / 2
        z: target ? 2 : selected ? 1 : 0
        // Keep the hit area available to pick up a light placed outside the photo.
        Rectangle {
            objectName: "relightMarker"
            anchors.fill: parent
            visible: pointer.hovered && !root.overPanel && (markerTimeout.running || handle.dragging)
            radius: width / 2
            color: handle.target ? "#90000000" : Studio.field
            border.color: handle.selected ? Studio.accent : "white"
            border.width: handle.selected ? 3 : 1
            Text {
                anchors.centerIn: parent
                text: handle.target ? "＋" : handle.lampIndex + 1
                color: "white"
                font.pixelSize: 12
                font.bold: handle.selected
            }
        }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            function position(mouse) {
                const p = mapToItem(root, mouse.x, mouse.y)
                return Qt.point((p.x - root.imageLeft) / root.zoom - handle.grabOffset.x,
                                (p.y - root.imageTop) / root.zoom - handle.grabOffset.y)
            }
            onPressed: mouse => {
                root.panel.selected = handle.lampIndex
                const p = mapToItem(root, mouse.x, mouse.y)
                handle.grabOffset = Qt.point((p.x - root.imageLeft) / root.zoom - handle.center.x,
                                             (p.y - root.imageTop) / root.zoom - handle.center.y)
                handle.draggingCenter = Qt.point(handle.center.x, handle.center.y)
                handle.moved = false
                handle.dragging = true
            }
            onPositionChanged: mouse => {
                if (!handle.dragging) return
                const p = position(mouse)
                if (!handle.moved) {
                    if (Math.abs(p.x - handle.center.x) + Math.abs(p.y - handle.center.y) < 0.01) return
                    handle.moved = true
                    root.dragStarted()
                }
                handle.draggingCenter = p
                root.panel.placeOnCanvas(handle.draggingCenter.x, handle.draggingCenter.y, false, handle.target)
            }
            onReleased: mouse => {
                if (!handle.dragging) return
                const p = position(mouse)
                if (handle.moved) root.panel.placeOnCanvas(p.x, p.y, true, handle.target)
                handle.dragging = false
            }
            onCanceled: {
                if (handle.dragging && handle.moved) root.dragCanceled()
                handle.dragging = false
            }
        }
    }
    Repeater {
        model: root.visible ? root.panel.lights.length : 0
        delegate: LightHandle {
            required property int index
            lampIndex: index
        }
    }
    LightHandle {
        lampIndex: root.panel.index
        visible: root.visible && !!root.panel.lamp.aimed
        target: true
    }
}
