import QtQuick
import "Studio.js" as Studio

Item {
    id: root
    required property var panel
    required property real zoom
    required property real imageWidth
    required property real imageHeight
    readonly property real imageLeft: (width - imageWidth) / 2
    readonly property real imageTop: (height - imageHeight) / 2
    signal dragStarted()
    signal dragCanceled()

    Repeater {
        model: root.visible ? root.panel.lights.length : 0
        delegate: Rectangle {
            id: handle
            required property int index
            objectName: "relightHandle" + index
            readonly property var light: root.panel.lights[index]
            readonly property var center: light ? root.panel.canvasPoint(light) : Qt.point(0, 0)
            readonly property bool selected: root.panel.index === index
            property point draggingCenter
            property point grabOffset
            property bool dragging: false
            property bool moved: false
            width: 24; height: 24; radius: 12
            x: root.imageLeft + (dragging ? draggingCenter.x : center.x) * root.zoom - width / 2
            y: root.imageTop + (dragging ? draggingCenter.y : center.y) * root.zoom - height / 2
            z: selected ? 1 : 0
            color: Studio.field
            border.color: selected ? Studio.accent : "white"
            border.width: selected ? 3 : 1
            Text {
                anchors.centerIn: parent
                text: handle.index + 1
                color: "white"
                font.pixelSize: 12
                font.bold: handle.selected
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
                    root.panel.selected = handle.index
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
                    root.panel.placeOnCanvas(handle.draggingCenter.x, handle.draggingCenter.y, false)
                }
                onReleased: mouse => {
                    if (!handle.dragging) return
                    const p = position(mouse)
                    if (handle.moved) root.panel.placeOnCanvas(p.x, p.y, true)
                    handle.dragging = false
                }
                onCanceled: {
                    if (handle.dragging && handle.moved) root.dragCanceled()
                    handle.dragging = false
                }
            }
        }
    }
}
