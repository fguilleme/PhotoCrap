import QtQuick

Item {
    id: root
    required property var document
    required property var viewportItem
    property real zoom: 1
    readonly property bool shapePath: activePath !== null && Boolean(activePath.vectorMask)
    // Keep the editable vector visible. A single fine guide on shapes leaves
    // the painted stroke legible; ordinary paths retain the contrasting guide.
    readonly property bool curveVisible: points.length > 0
    readonly property color paintedStrokeColor: shapePath ? document.penShapeStyle.strokeColor : "transparent"
    readonly property real guideWidth: !shapePath ? 1.5
        : document.penShapeStyle.strokeEnabled && paintedStrokeColor.a > 0
          ? Math.min(.75, Math.max(.2, document.penShapeStyle.strokeWidth*zoom*.3)) : .75
    signal painted()
    readonly property bool gpuBackend: GraphicsInfo.api === GraphicsInfo.Metal
        || GraphicsInfo.api === GraphicsInfo.Vulkan || GraphicsInfo.api === GraphicsInfo.OpenGL
        || GraphicsInfo.api === GraphicsInfo.Direct3D11 || GraphicsInfo.api === GraphicsInfo.Direct3D12
    readonly property rect screenBounds: visibleArea.bounds
    ScreenOverlayBounds {
        id: visibleArea
        artboard: root.parent
        viewportItem: root.viewportItem
        zoom: root.zoom
    }
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
    // A cached SVG path depends on the fixed points, zoom and clipped origin,
    // never on the rubber-band hover position.
    readonly property string curvePath: {
        if (!gpuBackend || !curveVisible || !points.length) return ""
        function xy(x, y) { return (x*root.zoom-root.x)+" "+(y*root.zoom-root.y) }
        function mid(a, b) { return {x:(a.x+b.x)/2,y:(a.y+b.y)/2} }
        function point(x, y) { return {x:x*root.zoom-root.x,y:y*root.zoom-root.y} }
        function pair(p) { return p.x+" "+p.y }
        function quadratic(a, b, c, d, depth) {
            const dx=d.x-3*c.x+3*b.x-a.x,dy=d.y-3*c.y+3*b.y-a.y
            // Endpoint/midpoint interpolation has maximum positional error
            // |D|/(12*sqrt(3)), D=P3-3P2+3P1-P0. Keep it below .01 logical px.
            if (dx*dx+dy*dy <= .0001*432 || depth >= 12) {
                const q={x:(3*(b.x+c.x)-a.x-d.x)/4,y:(3*(b.y+c.y)-a.y-d.y)/4}
                return "Q"+pair(q)+" "+pair(d)
            }
            const ab=mid(a,b),bc=mid(b,c),cd=mid(c,d)
            const abc=mid(ab,bc),bcd=mid(bc,cd),center=mid(abc,bcd)
            return quadratic(a,ab,abc,center,depth+1)+quadratic(center,bcd,cd,d,depth+1)
        }
        function segment(a, b) {
            return quadratic(point(a.x,a.y),point(a.outX,a.outY),
                             point(b.inX,b.inY),point(b.x,b.y),0)
        }
        let path="M"+xy(points[0].x,points[0].y)
        for (let i=1; i<points.length; ++i) path+=segment(points[i-1],points[i])
        // The original Canvas returns to the first point without closePath().
        if (activePath.closed) path+=segment(points[points.length-1],points[0])
        return path
    }
    function requestPaint() {
        if (softwareCurve && softwareCurve.item) softwareCurve.item.requestPaint()
    }
    Loader {
        id: nativeCurve
        anchors.fill: parent
        clip: true
        active: root.visible && root.curveVisible && root.gpuBackend && width>0 && height>0
        sourceComponent: PenCurve {
            svg: root.curvePath
            shapeGuide: root.shapePath
            guideWidth: root.guideWidth
        }
    }
    Loader {
        id: softwareCurve
        anchors.fill: parent
        active: root.visible && root.curveVisible && (!root.gpuBackend
            || (nativeCurve.item && !nativeCurve.item.curveRendererAvailable))
        sourceComponent: Canvas {
            objectName: "penRasterCurve"
            antialiasing: true
            Component.onCompleted: requestPaint()
            onPainted: root.painted()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c = getContext("2d")
                c.reset(); c.clearRect(0,0,width,height)
                if (!root.points.length) return
                c.translate(-root.x,-root.y)
                function segment(a,b) {
                    c.bezierCurveTo(a.outX*root.zoom,a.outY*root.zoom,b.inX*root.zoom,b.inY*root.zoom,b.x*root.zoom,b.y*root.zoom)
                }
                c.beginPath(); c.moveTo(root.points[0].x*root.zoom,root.points[0].y*root.zoom)
                for (let i=1; i<root.points.length; ++i) segment(root.points[i-1],root.points[i])
                if (root.activePath.closed) segment(root.points[root.points.length-1],root.points[0])
                if (!root.shapePath) { c.strokeStyle="#1a2b3b"; c.lineWidth=3; c.stroke() }
                c.strokeStyle="#73c5ff"; c.lineWidth=root.guideWidth; c.stroke()
            }
        }
    }
    onXChanged: if (visible) requestPaint()
    onGuideWidthChanged: if (visible) requestPaint()
    onYChanged: if (visible) requestPaint()
    onWidthChanged: if (visible) requestPaint()
    onHeightChanged: if (visible) requestPaint()
    PenRubberBand {
        id: rubberBand
        objectName: "penRubberBandOverlay"
        z: 1
        visible: root.document.penDrawing && root.document.penRubberBand
                 && root.activePath !== null && !root.activePath.closed && root.points.length > 0
        readonly property var last: root.points.length ? root.points[root.points.length-1] : null
        startPoint: last ? Qt.point(last.x*root.zoom,last.y*root.zoom) : Qt.point(0,0)
        controlPoint: last ? Qt.point(last.outX*root.zoom,last.outY*root.zoom) : Qt.point(0,0)
        endPoint: Qt.point(root.document.penHover.x*root.zoom,root.document.penHover.y*root.zoom)
        viewBounds: root.screenBounds
        imageOrigin: Qt.point(root.rubberBounds.x,root.rubberBounds.y)
        x: root.rubberBounds.x-root.x; y: root.rubberBounds.y-root.y
        width: root.rubberBounds.width; height: root.rubberBounds.height
    }
    PenHandles {
        id: handles
        objectName: "penHandlesOverlay"
        z: 2
        visible: root.points.length > 0
        points: root.points
        zoom: root.zoom
        imageOrigin: Qt.point(root.handlesBounds.x,root.handlesBounds.y)
        x: root.handlesBounds.x-root.x; y: root.handlesBounds.y-root.y
        width: root.handlesBounds.width; height: root.handlesBounds.height
    }
    onHandlesBoundsChanged: if (handles && handles.visible) handles.requestPaint()
    onRubberBoundsChanged: if (rubberBand && rubberBand.visible) rubberBand.requestPaint()
    Connections {
        target: root.document
        function onPenPathsChanged() { root.repaintPath() }
        function onPenHoverChanged() { if (rubberBand.visible) rubberBand.requestPaint() }
    }
}
