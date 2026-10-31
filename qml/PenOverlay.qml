import QtQuick

ScreenOverlayCanvas {
    id: root
    required property var document
    enabled: false
    antialiasing: true

    // Keep the QVariant snapshot until the path changes, independently of hover.
    readonly property var activePath: {
        const paths = document.penPaths, index = document.activePenPath
        return index >= 0 && index < paths.length ? paths[index] : null
    }
    readonly property var points: activePath ? activePath.points : []
    readonly property var curveHull: {
        const hull = []
        for (const p of points)
            hull.push(p, {x:p.inX, y:p.inY}, {x:p.outX, y:p.outY})
        return hull
    }
    readonly property var handleHull: {
        const hull = []
        for (const p of points) {
            hull.push(p)
            if (p.selected)
                hull.push({x:p.inX, y:p.inY}, {x:p.outX, y:p.outY})
        }
        return hull
    }
    function clippedBounds(hull, margin) {
        if (!hull.length)
            return Qt.rect(0,0,0,0)
        let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity
        for (const p of hull) {
            left = Math.min(left, p.x*zoom); top = Math.min(top, p.y*zoom)
            right = Math.max(right, p.x*zoom); bottom = Math.max(bottom, p.y*zoom)
        }
        left = Math.max(screenBounds.x, Math.floor(left-margin))
        top = Math.max(screenBounds.y, Math.floor(top-margin))
        right = Math.min(screenBounds.x+screenBounds.width, Math.ceil(right+margin))
        bottom = Math.min(screenBounds.y+screenBounds.height, Math.ceil(bottom+margin))
        return Qt.rect(left, top, Math.max(0,right-left), Math.max(0,bottom-top))
    }
    readonly property rect pathBounds: clippedBounds(curveHull, 3)
    readonly property rect handlesBounds: clippedBounds(handleHull, 5)
    readonly property rect rubberBounds: {
        if (!points.length)
            return Qt.rect(0,0,0,0)
        const last = points[points.length-1]
        return clippedBounds([last, {x:last.outX,y:last.outY}, document.penHover], 3)
    }
    x: pathBounds.x; y: pathBounds.y
    width: pathBounds.width; height: pathBounds.height

    function repaintPath() {
        if (!visible) return
        requestPaint()
        if (handles) handles.requestPaint()
        if (rubberBand && rubberBand.visible) rubberBand.requestPaint()
    }
    onVisibleChanged: repaintPath()
    onZoomChanged: repaintPath()
    onPaint: {
        const c = getContext("2d")
        c.reset(); c.clearRect(0,0,width,height)
        if (!points.length) return
        c.translate(-x,-y)
        function segment(a,b) {
            c.bezierCurveTo(a.outX*zoom,a.outY*zoom,b.inX*zoom,b.inY*zoom,b.x*zoom,b.y*zoom)
        }
        c.beginPath(); c.moveTo(points[0].x*zoom,points[0].y*zoom)
        for (let i=1; i<points.length; ++i) segment(points[i-1],points[i])
        if (activePath.closed) segment(points[points.length-1],points[0])
        c.strokeStyle="#1a2b3b"; c.lineWidth=3; c.stroke()
        c.strokeStyle="#73c5ff"; c.lineWidth=1.5; c.stroke()
    }
    Canvas {
        id: rubberBand
        objectName: "penRubberBandOverlay"
        enabled: false; antialiasing: true; z: 1
        visible: root.document.penDrawing && root.document.penRubberBand
                 && root.activePath !== null && !root.activePath.closed && root.points.length > 0
        x: root.rubberBounds.x-root.x; y: root.rubberBounds.y-root.y
        width: root.rubberBounds.width; height: root.rubberBounds.height
        onWidthChanged: if (visible) requestPaint()
        onHeightChanged: if (visible) requestPaint()
        onVisibleChanged: if (visible) requestPaint()
        onPaint: {
            const c = getContext("2d")
            c.reset(); c.clearRect(0,0,width,height)
            if (!root.points.length) return
            c.translate(-root.rubberBounds.x,-root.rubberBounds.y)
            const last=root.points[root.points.length-1], to=root.document.penHover
            c.beginPath(); c.moveTo(last.x*root.zoom,last.y*root.zoom)
            c.bezierCurveTo(last.outX*root.zoom,last.outY*root.zoom,
                            to.x*root.zoom,to.y*root.zoom,to.x*root.zoom,to.y*root.zoom)
            c.setLineDash([4,3]); c.strokeStyle="#73c5ff"; c.lineWidth=1.5; c.stroke()
        }
    }
    Canvas {
        id: handles
        objectName: "penHandlesOverlay"
        enabled: false; antialiasing: true; z: 2
        visible: root.points.length > 0
        x: root.handlesBounds.x-root.x; y: root.handlesBounds.y-root.y
        width: root.handlesBounds.width; height: root.handlesBounds.height
        onWidthChanged: if (visible) requestPaint()
        onHeightChanged: if (visible) requestPaint()
        onVisibleChanged: if (visible) requestPaint()
        onPaint: {
            const c = getContext("2d")
            c.reset(); c.clearRect(0,0,width,height)
            c.translate(-root.handlesBounds.x,-root.handlesBounds.y)
            for (const a of root.points) {
                const px=a.x*root.zoom, py=a.y*root.zoom
                if (a.selected) {
                    for (const h of [{x:a.inX*root.zoom,y:a.inY*root.zoom},
                                     {x:a.outX*root.zoom,y:a.outY*root.zoom}]) {
                        if (Math.hypot(h.x-px,h.y-py)<.1) continue
                        c.beginPath(); c.moveTo(px,py); c.lineTo(h.x,h.y)
                        c.strokeStyle="#73c5ff"; c.lineWidth=1; c.stroke()
                        c.beginPath(); c.arc(h.x,h.y,3,0,2*Math.PI)
                        c.fillStyle="#eef8ff"; c.fill(); c.strokeStyle="#1b4968"; c.stroke()
                    }
                }
                c.fillStyle=a.selected?"#73c5ff":"#ffffff"; c.fillRect(px-3,py-3,6,6)
                c.strokeStyle="#183e59"; c.lineWidth=1; c.strokeRect(px-3,py-3,6,6)
            }
        }
    }
    onHandlesBoundsChanged: if (handles && handles.visible) handles.requestPaint()
    onRubberBoundsChanged: if (rubberBand && rubberBand.visible) rubberBand.requestPaint()
    Connections {
        target: root.document
        function onPenPathsChanged() { root.repaintPath() }
        function onPenHoverChanged() { if (rubberBand.visible) rubberBand.requestPaint() }
    }
}
