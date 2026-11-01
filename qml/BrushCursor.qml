import QtQuick

// Keep the footprint bounded to the viewport. RHI draws it procedurally;
// software retains the local raster and reuses it during unclipped motion.
Item {
    id: root
    required property var viewportItem
    property real zoom: 1
    property real pointerX: 0
    property real pointerY: 0
    property real diameter: 24
    property real roundness: 1
    property real angle: 0
    property int selectionSign: 0
    enabled: false

    readonly property rect viewportBounds: {
        if (!parent || !viewportItem)
            return Qt.rect(0, 0, 0, 0)
        const scrollX = viewportItem.contentX, scrollY = viewportItem.contentY
        const scale = zoom, rotation = parent.rotation
        const originX = parent.x, originY = parent.y
        const sourceWidth = parent.width, sourceHeight = parent.height
        const w = viewportItem.width, h = viewportItem.height
        const points = [viewportItem.mapToItem(parent, 0, 0),
                        viewportItem.mapToItem(parent, w, 0),
                        viewportItem.mapToItem(parent, w, h),
                        viewportItem.mapToItem(parent, 0, h)]
        // Include canvas margins: a brush near the image edge keeps its whole
        // footprint whenever that footprint is still inside the viewport.
        const left = Math.floor(Math.min.apply(null, points.map(p => p.x)))
        const top = Math.floor(Math.min.apply(null, points.map(p => p.y)))
        const right = Math.ceil(Math.max.apply(null, points.map(p => p.x)))
        const bottom = Math.ceil(Math.max.apply(null, points.map(p => p.y)))
        return Qt.rect(left, top, Math.max(0, right-left), Math.max(0, bottom-top))
    }
    readonly property real fullSize: Math.max(12, diameter+12)
    readonly property real clipLeft: Math.max(0, viewportBounds.x-(pointerX-fullSize/2))
    readonly property real clipTop: Math.max(0, viewportBounds.y-(pointerY-fullSize/2))
    readonly property real clipRight: Math.max(0, pointerX+fullSize/2-viewportBounds.x-viewportBounds.width)
    readonly property real clipBottom: Math.max(0, pointerY+fullSize/2-viewportBounds.y-viewportBounds.height)
    readonly property real localCenterX: fullSize/2-clipLeft
    readonly property real localCenterY: fullSize/2-clipTop
    x: pointerX-fullSize/2+clipLeft
    y: pointerY-fullSize/2+clipTop
    width: Math.max(0, fullSize-clipLeft-clipRight)
    height: Math.max(0, fullSize-clipTop-clipBottom)

    signal painted()
    readonly property bool gpuCursor: GraphicsInfo.api === GraphicsInfo.Metal
        || GraphicsInfo.api === GraphicsInfo.Vulkan || GraphicsInfo.api === GraphicsInfo.OpenGL
        || GraphicsInfo.api === GraphicsInfo.Direct3D11 || GraphicsInfo.api === GraphicsInfo.Direct3D12
    readonly property bool gpuReady: nativeCursor.item !== null
        && nativeCursor.item.status === ShaderEffect.Compiled
    readonly property bool shaderFailed: nativeCursor.item !== null
        && nativeCursor.item.status === ShaderEffect.Error

    function requestPaint() {
        if (softwareCursor && softwareCursor.item)
            softwareCursor.item.requestPaint()
    }
    Loader {
        id: nativeCursor
        anchors.fill: parent
        active: root.visible && root.gpuCursor
        visible: !root.shaderFailed
        sourceComponent: ShaderEffect {
            objectName: "brushCursorShader"
            property size cursorSize: Qt.size(width, height)
            property point cursorCenter: Qt.point(root.localCenterX, root.localCenterY)
            property vector4d brushParameters: Qt.vector4d(root.diameter / 2,
                Math.max(.01, Math.min(1, root.roundness)),
                Math.cos(root.angle * Math.PI / 180), Math.sin(root.angle * Math.PI / 180))
            property real selectionSign: root.selectionSign
            vertexShader: "qrc:/PhotoCrap/src/render/shaders/brushcursor.vert.qsb"
            fragmentShader: "qrc:/PhotoCrap/src/render/shaders/brushcursor.frag.qsb"
            blending: true
        }
    }
    Loader {
        id: softwareCursor
        anchors.fill: parent
        active: root.visible && (!root.gpuCursor || root.shaderFailed)
        sourceComponent: Canvas {
            objectName: "brushCursorRaster"
            Component.onCompleted: requestPaint()
            onPainted: root.painted()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c = getContext("2d")
                c.reset()
                c.clearRect(0, 0, width, height)
                c.translate(root.localCenterX, root.localCenterY)
                c.save()
                const ratio = Math.max(.01, Math.min(1, root.roundness))
                c.rotate(root.angle*Math.PI/180)
                c.scale(1, ratio)
                const radius = root.diameter/2
                c.beginPath()
                c.arc(0, 0, radius, 0, Math.PI*2)
                c.lineWidth = 2/ratio
                c.strokeStyle = "rgba(0,0,0,.75)"
                c.stroke()
                c.lineWidth = 1/ratio
                c.strokeStyle = "rgba(255,255,255,.95)"
                c.stroke()
                c.restore()
                if (root.selectionSign !== 0) {
                    // Keep the sign upright and inside the footprint's local bounds.
                    const px = radius*.5, py = -radius*.5
                    c.beginPath()
                    c.moveTo(px-3, py)
                    c.lineTo(px+3, py)
                    if (root.selectionSign > 0) {
                        c.moveTo(px, py-3)
                        c.lineTo(px, py+3)
                    }
                    c.lineWidth = 3
                    c.strokeStyle = "rgba(0,0,0,.85)"
                    c.stroke()
                    c.lineWidth = 1
                    c.strokeStyle = "#ffffff"
                    c.stroke()
                }
            }
        }
    }
    onLocalCenterXChanged: if (visible) requestPaint()
    onLocalCenterYChanged: if (visible) requestPaint()
    onWidthChanged: if (visible) requestPaint()
    onHeightChanged: if (visible) requestPaint()
    onDiameterChanged: if (visible) requestPaint()
    onRoundnessChanged: if (visible) requestPaint()
    onAngleChanged: if (visible) requestPaint()
    onSelectionSignChanged: if (visible) requestPaint()
    onVisibleChanged: if (visible) requestPaint()
}
