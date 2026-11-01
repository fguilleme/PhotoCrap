import QtQuick

// This is the settings illustration, independent of the document paint engine.
Item {
    id: root
    property real brushSize: 32
    property real hardness: .8
    property real brushOpacity: 1
    property real flow: 1
    property real spacing: .25
    property real angle: 0
    property real roundness: 1
    readonly property real radius: Math.min(brushSize/2,38)
    readonly property real step: Math.max(1,brushSize*spacing)
    // Match the legacy loop's endpoint, including fractional accumulation.
    readonly property int dabCount: {
        let count=0
        for(let x=28;x<width-20;x+=step)++count
        return count
    }
    readonly property real radians: angle*Math.PI/180
    readonly property bool gpuBackend: GraphicsInfo.api===GraphicsInfo.Metal
        || GraphicsInfo.api===GraphicsInfo.Vulkan || GraphicsInfo.api===GraphicsInfo.OpenGL
        || GraphicsInfo.api===GraphicsInfo.Direct3D11 || GraphicsInfo.api===GraphicsInfo.Direct3D12
    // Keep thin tips and dense accumulation on the original raster path. This
    // bounds per-fragment work and preserves its sequential 8-bit rounding.
    readonly property bool gpuEligible: gpuBackend && radius*roundness>=2
        && step>=radius/8 && width===Math.round(width) && height===Math.round(height)
    readonly property bool shaderFailed: nativePreview.item!==null
        && nativePreview.item.status===ShaderEffect.Error
    readonly property bool gpuPreview: gpuEligible && !shaderFailed
    readonly property bool rasterActive: softwarePreview.item!==null
        && softwarePreview.item.context!==null && softwarePreview.item.context!==undefined
    signal painted()
    enabled: false
    function requestPaint() {
        if(visible && softwarePreview && softwarePreview.item)softwarePreview.item.requestPaint()
    }
    Loader {
        id: nativePreview
        anchors.fill: parent
        active: root.visible && root.gpuEligible
        visible: !root.shaderFailed
        sourceComponent: ShaderEffect {
            objectName: "brushTipPreviewShader"
            property size previewSize: Qt.size(width,height)
            property vector4d tipGeometry: Qt.vector4d(root.radius,root.roundness,
                                                       Math.cos(root.radians),Math.sin(root.radians))
            property vector4d tipPaint: Qt.vector4d(root.step,Math.min(.999,root.hardness),
                                                   root.brushOpacity,root.flow)
            property real dabCount: root.dabCount
            vertexShader: "qrc:/PhotoCrap/src/render/shaders/brushtippreview.vert.qsb"
            fragmentShader: "qrc:/PhotoCrap/src/render/shaders/brushtippreview.frag.qsb"
            blending: true
        }
    }
    Loader {
        id: softwarePreview
        anchors.fill: parent
        active: root.visible && !root.gpuPreview
        sourceComponent: Canvas {
            objectName: "brushTipPreviewRaster"
            Component.onCompleted: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPainted: root.painted()
            onPaint: {
                const c=getContext("2d")
                c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height))
                c.fillStyle="#151a20";c.fillRect(0,0,width,height)
                c.globalAlpha=root.brushOpacity
                for(let x=28;x<width-20;x+=root.step) {
                    c.save();c.translate(x,height/2);c.rotate(root.radians);c.scale(1,root.roundness)
                    const g=c.createRadialGradient(0,0,0,0,0,root.radius)
                    g.addColorStop(0,"rgba(190,236,203,"+root.flow+")")
                    g.addColorStop(Math.min(.999,root.hardness),"rgba(190,236,203,"+root.flow+")")
                    g.addColorStop(1,"rgba(190,236,203,0)")
                    c.fillStyle=g;c.beginPath();c.arc(0,0,root.radius,0,Math.PI*2);c.fill();c.restore()
                }
            }
        }
    }
    onBrushSizeChanged: requestPaint()
    onHardnessChanged: requestPaint()
    onBrushOpacityChanged: requestPaint()
    onFlowChanged: requestPaint()
    onSpacingChanged: requestPaint()
    onAngleChanged: requestPaint()
    onRoundnessChanged: requestPaint()
    onVisibleChanged: if(visible)requestPaint()
}
