import QtQuick

Item {
    id: root
    required property var document
    required property var viewportItem
    property real zoom: 1
    signal painted()
    readonly property bool gpuBackend: GraphicsInfo.api===GraphicsInfo.Metal
        || GraphicsInfo.api===GraphicsInfo.Vulkan || GraphicsInfo.api===GraphicsInfo.OpenGL
        || GraphicsInfo.api===GraphicsInfo.Direct3D11 || GraphicsInfo.api===GraphicsInfo.Direct3D12
    readonly property rect screenBounds: visibleArea.bounds
    ScreenOverlayBounds {
        id: visibleArea
        artboard: root.parent
        viewportItem: root.viewportItem
        zoom: root.zoom
    }
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
    readonly property rect pointBounds: {
        if (!points.length)return Qt.rect(0,0,0,0)
        let left=Infinity, top=Infinity, right=-Infinity, bottom=-Infinity
        for (const p of points) {
            left=Math.min(left,p.x*zoom); top=Math.min(top,p.y*zoom)
            right=Math.max(right,p.x*zoom); bottom=Math.max(bottom,p.y*zoom)
        }
        return Qt.rect(left,top,right-left,bottom-top)
    }
    function clippedBounds(box, margin) {
        const left=Math.max(screenBounds.x,Math.floor(box.x-margin))
        const top=Math.max(screenBounds.y,Math.floor(box.y-margin))
        const right=Math.min(screenBounds.x+screenBounds.width,Math.ceil(box.x+box.width+margin))
        const bottom=Math.min(screenBounds.y+screenBounds.height,Math.ceil(box.y+box.height+margin))
        return Qt.rect(left,top,Math.max(0,right-left),Math.max(0,bottom-top))
    }
    readonly property rect guideBounds: {
        if(!points.length)return Qt.rect(0,0,0,0)
        let box=pointBounds
        if (document.polygonLassoActive && pointerInside) {
            const l=Math.min(box.x,pointerX),t=Math.min(box.y,pointerY)
            const r=Math.max(box.x+box.width,pointerX),b=Math.max(box.y+box.height,pointerY)
            box=Qt.rect(l,t,r-l,b-t)
        }
        // QPainter's miter limit is expressed in pen widths; allow the full
        // sharp join and its antialias/filter fringe, not only the point handles.
        return clippedBounds(box,document.hasSelection ? 17 : document.polygonLassoActive ? 12 : 3)
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
    // The fixed prefix and marker model do not depend on the live endpoint.
    readonly property rect fixedBounds: clippedBounds(pointBounds,12)
    readonly property string fixedPath: {
        if(!gpuBackend || document.hasSelection || !points.length)return ""
        function xy(p){return (p.x*root.zoom-root.fixedBounds.x)+" "+(p.y*root.zoom-root.fixedBounds.y)}
        let path="M"+xy(points[0])
        for(let i=1;i<points.length;++i)path+="L"+xy(points[i])
        return path
    }
    readonly property var visiblePoints: gpuBackend && !document.hasSelection
        && document.polygonLassoActive ? points.filter(function(p) {
        const x=p.x*zoom,y=p.y*zoom,b=screenBounds
        return x+4>=b.x && x-4<=b.x+b.width && y+4>=b.y && y-4<=b.y+b.height
    }) : []
    // CurveRenderer and QPainter cut extremely acute miters differently.
    // Keep their historical raster appearance rather than beveling a spike.
    function acuteJoin(a,b,c) {
        const ux=a.x-b.x,uy=a.y-b.y,vx=c.x-b.x,vy=c.y-b.y
        const norm=Math.hypot(ux,uy)*Math.hypot(vx,vy)
        return norm>1e-12 && ux*vx+uy*vy>.995*norm
    }
    readonly property bool fixedAcuteJoin: {
        if(!gpuBackend || document.hasSelection)return false
        for(let i=1;i+1<points.length;++i)
            if(acuteJoin(points[i-1],points[i],points[i+1]))return true
        return false
    }
    readonly property bool curveSafe: !gpuBackend || document.hasSelection
        || (!fixedAcuteJoin && !(document.polygonLassoActive
        && pointerInside && points.length>=2
        && acuteJoin(points[points.length-2],points[points.length-1],{x:pointerX/zoom,y:pointerY/zoom})))
    readonly property bool gpuGuide: gpuBackend && curveSafe && !document.hasSelection
        && nativeGuide.item!==null && nativeGuide.item.curveRendererAvailable
    readonly property bool rasterActive: softwareGuide.item!==null
        && softwareGuide.item.context!==null && softwareGuide.item.context!==undefined
    function requestPaint() {
        if(visible && softwareGuide && softwareGuide.item)softwareGuide.item.requestPaint()
    }
    Loader {
        id: nativeGuide
        anchors.fill: parent
        clip: true
        active: root.visible && root.width>0 && root.height>0
            && root.gpuBackend && root.curveSafe && !root.document.hasSelection
        sourceComponent: LassoSolidGuide {
            fixedPath: root.fixedPath
            origin: Qt.point(root.fixedBounds.x-root.x,root.fixedBounds.y-root.y)
            tail: Qt.point(root.pointerX-root.fixedBounds.x,root.pointerY-root.fixedBounds.y)
            tailVisible: root.document.polygonLassoActive && root.pointerInside
            markers: root.visiblePoints
            zoom: root.zoom
            imageOrigin: Qt.point(root.x,root.y)
        }
    }
    Loader {
        id: softwareGuide
        anchors.fill: parent
        active: root.visible && root.width>0 && root.height>0 && !root.gpuGuide
        sourceComponent: Canvas {
            objectName: "lassoRasterGuide"
            Component.onCompleted: requestPaint()
            onPainted: root.painted()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c=getContext("2d")
                c.reset(); c.clearRect(0,0,width,height)
                // Reassert solid pens after a preceding animated candidate:
                // Context2D reset can retain a nonzero replay dash offset.
                c.setLineDash([])
                if (!root.points.length) return
                c.translate(-root.x,-root.y)
                c.beginPath(); c.moveTo(root.points[0].x*root.zoom,root.points[0].y*root.zoom)
                for (let i=1;i<root.points.length;++i) c.lineTo(root.points[i].x*root.zoom,root.points[i].y*root.zoom)
                if (root.document.polygonLassoActive && root.pointerInside)
                    c.lineTo(root.pointerX,root.pointerY)
                if (root.document.hasSelection) {
                    c.closePath()
                    c.lineWidth=1.5; c.setLineDash([5,5]); c.lineDashOffset=root.phase
                    c.strokeStyle="#111111"; c.stroke()
                    c.lineDashOffset=root.phase+5; c.strokeStyle="#ffffff"; c.stroke()
                    c.setLineDash([])
                } else {
                    c.strokeStyle="#f5f7f9"; c.lineWidth=1; c.stroke()
                }
                if (root.document.polygonLassoActive) {
                    for (const p of root.points) {
                        c.beginPath(); c.arc(p.x*root.zoom,p.y*root.zoom,3,0,Math.PI*2)
                        c.fillStyle="#f5f7f9"; c.fill()
                        c.strokeStyle="#172028"; c.lineWidth=1; c.stroke()
                    }
                }
            }
        }
    }
    onXChanged: requestPaint()
    onYChanged: requestPaint()
    Connections {
        target: root.document
        function onSelectionChanged() { if (root.visible) root.requestPaint() }
    }
}
