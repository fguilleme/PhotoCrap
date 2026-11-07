import QtQuick
import QtQuick.Controls

Item {
    id: root
    required property var document
    required property real zoom
    signal colorRequested(int index, color value)
    visible: document.gradientDragging || document.dynamicGradientActive
    readonly property var stops: document.dynamicGradientStops
    readonly property point first: Qt.point(document.gradientStart.x * zoom, document.gradientStart.y * zoom)
    readonly property point last: Qt.point(document.gradientEnd.x * zoom, document.gradientEnd.y * zoom)
    readonly property real dx: last.x - first.x
    readonly property real dy: last.y - first.y
    readonly property real length: Math.hypot(dx, dy)
    readonly property real angle: Math.atan2(dy, dx) * 180 / Math.PI

    property Item hoveredArea: null
    property Item grabbedArea: null
    readonly property Item pointerArea: grabbedArea || hoveredArea
    readonly property bool pointerActive: visible && pointerArea !== null
        && pointerArea.enabled && (pointerArea.containsMouse || pointerArea.pressed)
    readonly property int pointerCursor: pointerActive ? pointerArea.cursorShape : Qt.CrossCursor

    onVisibleChanged: {
        if (!visible) {
            hoveredArea = null
            grabbedArea = null
        }
    }

    component Handle: Rectangle {
        id: handle
        required property int handleIndex
        required property real anchorX
        required property real anchorY
        property bool stop: handleIndex >= 0
        property bool canMove: true
        x: anchorX - width / 2
        y: anchorY - height / 2
        width: stop ? 13 : 11
        height: width
        radius: stop ? 2 : width / 2
        border.color: "#202932"
        color: "#f5f7fa"

        MouseArea {
            id: handleArea
            anchors.fill: parent
            anchors.margins: -4
            enabled: root.document.dynamicGradientActive
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            preventStealing: true
            cursorShape: handle.stop
                ? (dragging ? Qt.ClosedHandCursor : Qt.PointingHandCursor)
                : Qt.SizeAllCursor
            property bool dragging: false
            onEntered: root.hoveredArea = handleArea
            onExited: if (root.hoveredArea === handleArea) root.hoveredArea = null
            onEnabledChanged: {
                if (!enabled) {
                    if (root.hoveredArea === handleArea) root.hoveredArea = null
                    if (root.grabbedArea === handleArea) root.grabbedArea = null
                }
            }
            ToolTip.visible: containsMouse && !pressed
            ToolTip.text: handle.stop
                ? (handle.canMove ? qsTr("Glisser pour déplacer la couleur ; double-clic pour la modifier ; clic droit pour supprimer")
                                  : qsTr("Double-clic pour modifier la couleur"))
                : qsTr("Glisser pour modifier la direction et la longueur du dégradé")
            onPressed: function(event) {
                if (event.button === Qt.LeftButton && handle.canMove) {
                    dragging = root.document.beginGradientHandle(handle.handleIndex)
                    if (dragging) root.grabbedArea = handleArea
                }
            }
            onPositionChanged: function(event) {
                if (!dragging || !pressed) return
                const p = mapToItem(root, event.x, event.y)
                root.document.updateGradientHandle(p.x / root.zoom, p.y / root.zoom)
            }
            onReleased: {
                if (dragging) root.document.endStroke()
                dragging = false
                if (root.grabbedArea === handleArea) root.grabbedArea = null
            }
            onCanceled: {
                if (dragging) root.document.cancelStroke()
                dragging = false
                if (root.grabbedArea === handleArea) root.grabbedArea = null
            }
            onClicked: function(event) {
                if (handle.stop && event.button === Qt.RightButton)
                    root.document.removeGradientStop(handle.handleIndex)
            }
            onDoubleClicked: function(event) {
                if (handle.stop && event.button === Qt.LeftButton)
                    root.colorRequested(handle.handleIndex, handle.color)
            }
        }
    }

    Item {
        x: root.first.x
        y: root.first.y - height / 2
        width: root.length
        height: 12
        transformOrigin: Item.Left
        rotation: root.angle
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: 3
            color: "#f5f7fa"
            border.color: "#303842"
        }
        MouseArea {
            id: lineArea
            anchors.fill: parent
            enabled: root.document.dynamicGradientActive && width > 1
            hoverEnabled: true
            cursorShape: Qt.CrossCursor
            onEntered: root.hoveredArea = lineArea
            onExited: if (root.hoveredArea === lineArea) root.hoveredArea = null
            ToolTip.visible: containsMouse && !pressed
            ToolTip.text: qsTr("Cliquer pour ajouter un point de couleur")
            onClicked: function(event) { root.document.addGradientStop(event.x / width) }
        }
    }

    Handle {
        objectName: "gradientStartHandle"
        handleIndex: -2
        anchorX: root.first.x
        anchorY: root.first.y
    }
    Handle {
        objectName: "gradientEndHandle"
        handleIndex: -1
        anchorX: root.last.x
        anchorY: root.last.y
    }
    Repeater {
        model: root.stops.length
        Handle {
            required property int index
            objectName: "gradientColorStop_" + index
            handleIndex: index
            canMove: index > 0 && index < root.stops.length - 1
            readonly property var entry: root.stops[index]
            readonly property real t: entry ? entry.position : 0
            color: entry ? entry.color : "transparent"
            anchorX: root.first.x + t * root.dx - 16 * root.dy / Math.max(1, root.length)
            anchorY: root.first.y + t * root.dy + 16 * root.dx / Math.max(1, root.length)
        }
    }
}
