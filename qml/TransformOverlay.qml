import QtQuick

Item {
    id: root
    required property var document
    required property Item artboard
    required property real zoom
    required property point scroll
    required property real angle
    property bool panning: false
    signal focusRequested()
    readonly property var options: document.transformOptions
    objectName: "transformOverlay"
    clip: true
    visible: document.transformActive

    function project(point) { return artboard.mapToItem(root, point.x * zoom, point.y * zoom) }
    function unproject(x, y) {
        const p = root.mapToItem(artboard, x, y)
        return Qt.point(p.x / zoom, p.y / zoom)
    }
    onOptionsChanged: frame.requestPaint()
    onScrollChanged: frame.requestPaint()
    onZoomChanged: frame.requestPaint()
    onAngleChanged: frame.requestPaint()
    onVisibleChanged: if (visible) frame.requestPaint()
    Connections {
        target: root.artboard
        function onXChanged() { frame.requestPaint() }
        function onYChanged() { frame.requestPaint() }
    }
    Canvas {
        id: frame
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)
            const options = root.options
            if (!root.visible || !options.quad || options.quad.length !== 4) return
            const quad = options.quad.map(root.project)
            function line(points, closed) {
                if (!points.length) return
                ctx.beginPath()
                ctx.moveTo(points[0].x, points[0].y)
                for (let i = 1; i < points.length; ++i) ctx.lineTo(points[i].x, points[i].y)
                if (closed) ctx.closePath()
                ctx.stroke()
            }
            function marker(point, size, selected) {
                ctx.fillStyle = selected ? "#338bd7" : "#e8f5ff"
                ctx.strokeStyle = "#172738"
                ctx.lineWidth = 1.2
                ctx.fillRect(point.x - size / 2, point.y - size / 2, size, size)
                ctx.strokeRect(point.x - size / 2, point.y - size / 2, size, size)
            }
            if (options.outlines) {
                for (const outline of options.outlines) {
                    const path = outline.map(root.project)
                    ctx.lineWidth = 1.5
                    ctx.strokeStyle = "#172738"
                    line(path, false)
                    ctx.strokeStyle = "#f2f6fa"
                    ctx.setLineDash([4, 4])
                    line(path, false)
                    ctx.setLineDash([])
                }
            }
            if (options.mode === "warp") {
                if (options.showGuides) {
                    ctx.strokeStyle = "#9edaff"
                    ctx.lineWidth = 1.25
                    for (const guide of options.guides) line(guide.map(root.project), false)
                }
                if (options.node >= 0) {
                    const anchor = root.project(options.nodes[options.node])
                    ctx.strokeStyle = "#9edaff"
                    ctx.lineWidth = 1.25
                    for (const handle of options.handles) {
                        const p = root.project(handle.point)
                        line([anchor, p], false)
                        ctx.beginPath()
                        ctx.arc(p.x, p.y, 3.5, 0, 2 * Math.PI)
                        ctx.fillStyle = "#e8f5ff"
                        ctx.fill()
                        ctx.stroke()
                    }
                }
                for (let i = 0; i < options.nodes.length; ++i)
                    marker(root.project(options.nodes[i]), i === options.node ? 9 : 7, i === options.node)
            } else {
                ctx.strokeStyle = "#152536"
                ctx.lineWidth = 3
                line(quad, true)
                ctx.strokeStyle = "#9edaff"
                ctx.lineWidth = 1.25
                line(quad, true)
                if (options.mode !== "rotate") {
                    for (let i = 0; i < 4; ++i) {
                        const a = quad[i], b = quad[(i + 1) % 4]
                        if (options.mode !== "skew") marker(a, 8, false)
                        if (options.mode !== "perspective")
                            marker(Qt.point((a.x + b.x) / 2, (a.y + b.y) / 2), 7, false)
                    }
                }
                if (options.showReference || options.mode === "rotate") {
                    const p = root.project(options.pivot)
                    ctx.strokeStyle = "#e8f5ff"
                    ctx.lineWidth = 1.4
                    ctx.beginPath()
                    ctx.arc(p.x, p.y, 5, 0, 2 * Math.PI)
                    ctx.stroke()
                    line([Qt.point(p.x - 8, p.y), Qt.point(p.x + 8, p.y)], false)
                    line([Qt.point(p.x, p.y - 8), Qt.point(p.x, p.y + 8)], false)
                }
            }
        }
    }
    MouseArea {
        id: pointer
        objectName: "transformPointer"
        anchors.fill: parent
        enabled: root.visible && !root.panning
        hoverEnabled: enabled
        preventStealing: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        readonly property var hits: [Qt.SizeFDiagCursor, Qt.SizeBDiagCursor, Qt.SizeFDiagCursor,
            Qt.SizeBDiagCursor, Qt.SizeVerCursor, Qt.SizeHorCursor, Qt.SizeVerCursor, Qt.SizeHorCursor]
        readonly property string hit: {
            const state = root.options, scroll = root.scroll, angle = root.angle
            return containsMouse && root.visible
                ? root.document.transformHit(root.unproject(mouseX, mouseY), 8 / root.zoom) : ""
        }
        cursorShape: hit.startsWith("handle:") ? hits[Number(hit.slice(7))]
                     : hit === "move" || hit === "pivot" || hit.startsWith("node:") || hit.startsWith("tangent:") || hit === "mesh"
                       ? Qt.SizeAllCursor
                       : hit === "rotate" || hit === "split" ? Qt.CrossCursor : Qt.ArrowCursor
        onPressed: function(mouse) {
            root.focusRequested()
            if (mouse.button === Qt.RightButton) {
                menu.popup(mouse.x, mouse.y)
                return
            }
            root.document.beginTransformGesture(root.unproject(mouse.x, mouse.y), 8 / root.zoom, mouse.modifiers)
        }
        onPositionChanged: function(mouse) {
            if (pressedButtons & Qt.LeftButton)
                root.document.updateTransformGesture(root.unproject(mouse.x, mouse.y), mouse.modifiers)
        }
        onReleased: function(mouse) {
            if (mouse.button === Qt.LeftButton) {
                root.document.updateTransformGesture(root.unproject(mouse.x, mouse.y), mouse.modifiers)
                root.document.endTransformGesture()
            }
        }
        onCanceled: root.document.endTransformGesture()
        onDoubleClicked: function(mouse) {
            if (mouse.button === Qt.LeftButton && hit === "move") root.document.applyTransform()
        }
    }
    TransformMenu {
        id: menu
        document: root.document
        contextMenu: true
    }
}
