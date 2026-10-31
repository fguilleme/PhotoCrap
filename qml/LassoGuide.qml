import QtQuick

ScreenOverlayCanvas {
    id: root
    required property var document
    property real phase: 0
    property real pointerX: 0
    property real pointerY: 0
    property bool pointerInside: false
    enabled: false

    readonly property bool guideNeeded: document.selectionEdgesVisible && document.lassoPreviewActive
        && (document.polygonLassoActive || document.selectionShape !== 4 || !document.hasSelection)
    // Completed/shared GPU contours must not serialize their native points here.
    readonly property var points: guideNeeded ? document.selectionPath : []
    visible: guideNeeded && points.length > 0
    function clippedBounds(hull, margin) {
        if (!hull.length) return Qt.rect(0,0,0,0)
        let left=Infinity, top=Infinity, right=-Infinity, bottom=-Infinity
        for (const p of hull) {
            left=Math.min(left,p.x*zoom); top=Math.min(top,p.y*zoom)
            right=Math.max(right,p.x*zoom); bottom=Math.max(bottom,p.y*zoom)
        }
        left=Math.max(screenBounds.x,Math.floor(left-margin))
        top=Math.max(screenBounds.y,Math.floor(top-margin))
        right=Math.min(screenBounds.x+screenBounds.width,Math.ceil(right+margin))
        bottom=Math.min(screenBounds.y+screenBounds.height,Math.ceil(bottom+margin))
        return Qt.rect(left,top,Math.max(0,right-left),Math.max(0,bottom-top))
    }
    readonly property rect guideBounds: {
        const hull=points.slice()
        if (document.polygonLassoActive && pointerInside)
            hull.push({x:pointerX/zoom,y:pointerY/zoom})
        return clippedBounds(hull,document.hasSelection ? 8 : document.polygonLassoActive ? 6 : 3)
    }
    x: guideBounds.x; y: guideBounds.y
    width: guideBounds.width; height: guideBounds.height

    function updatePointer() {
        if (visible && document.polygonLassoActive) requestPaint()
    }
    onPointerXChanged: updatePointer()
    onPointerYChanged: updatePointer()
    onPointerInsideChanged: updatePointer()
    onPhaseChanged: if (visible && document.hasSelection) requestPaint()
    onVisibleChanged: if (visible) requestPaint()
    onZoomChanged: if (visible) requestPaint()
    onPaint: {
        const c=getContext("2d")
        c.reset(); c.clearRect(0,0,width,height)
        if (!points.length) return
        c.translate(-x,-y)
        c.beginPath(); c.moveTo(points[0].x*zoom,points[0].y*zoom)
        for (let i=1;i<points.length;++i) c.lineTo(points[i].x*zoom,points[i].y*zoom)
        if (document.polygonLassoActive && pointerInside)
            c.lineTo(pointerX,pointerY)
        if (document.hasSelection) {
            c.closePath()
            c.lineWidth=1.5; c.setLineDash([5,5]); c.lineDashOffset=phase
            c.strokeStyle="#111111"; c.stroke()
            c.lineDashOffset=phase+5; c.strokeStyle="#ffffff"; c.stroke()
            c.setLineDash([])
        } else {
            c.strokeStyle="#f5f7f9"; c.lineWidth=1; c.stroke()
        }
        if (document.polygonLassoActive) {
            for (const p of points) {
                c.beginPath(); c.arc(p.x*zoom,p.y*zoom,3,0,Math.PI*2)
                c.fillStyle="#f5f7f9"; c.fill()
                c.strokeStyle="#172028"; c.lineWidth=1; c.stroke()
            }
        }
    }
    Connections {
        target: root.document
        function onSelectionChanged() { if (root.visible) root.requestPaint() }
    }
}
