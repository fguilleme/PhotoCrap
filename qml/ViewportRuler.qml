import QtQuick
import QtQuick.Shapes
import PhotoCrap.Native 1.0

Canvas {
    id: root
    property bool vertical: false
    property real documentLength: 0
    property real zoom: 1
    property real offset: 0
    enabled: false
    readonly property bool gpuBackend: GraphicsInfo.api === GraphicsInfo.Metal
        || GraphicsInfo.api === GraphicsInfo.Vulkan || GraphicsInfo.api === GraphicsInfo.OpenGL
        || GraphicsInfo.api === GraphicsInfo.Direct3D11 || GraphicsInfo.api === GraphicsInfo.Direct3D12
    readonly property bool integralSize: width===Math.round(width) && height===Math.round(height)
    readonly property bool gpuRuler: nativeTicks.item !== null && nativeTicks.item.curveRendererAvailable
    readonly property bool rasterActive: context !== null && context !== undefined
    readonly property int step: zoom<.3 ? 500 : zoom<.65 ? 200 : zoom<1.5 ? 100 : 50
    // Labels may extend past an offscreen tick, especially on the vertical axis.
    readonly property real labelMargin: Math.max(64,String(Math.floor(documentLength)).length*12+8)
    readonly property real spacing: step*zoom
    readonly property int firstTick: spacing>0 ? Math.max(0,Math.ceil((-offset-labelMargin)/spacing)) : 0
    readonly property int lastTick: spacing>0 ? Math.min(Math.floor(documentLength/step),
        Math.floor(((vertical?height:width)-offset+labelMargin)/spacing)) : -1
    readonly property int tickCount: Math.max(0,lastTick-firstTick+1)
    Loader {
        id: nativeTicks
        anchors.fill: parent
        active: root.visible && root.gpuBackend && root.integralSize && width>0 && height>0
        onLoaded: if (!item.curveRendererAvailable) root.requestPaint()
        sourceComponent: Item {
            readonly property bool curveRendererAvailable: capability.curveRendererAvailable
            visible: curveRendererAvailable
            GpuGuideShape { id: capability }
            objectName: "viewportRulerGpu"
            width: root.width; height: root.height
            layer.enabled: true
            clip: true
            Rectangle { width: root.width; height: root.height; color: "#20262e"; antialiasing: true }
            Repeater {
                model: root.tickCount
                delegate: Item {
                    id: tick
                    required property int index
                    readonly property int value: (root.firstTick+index)*root.step
                    readonly property real at: root.offset+value*root.zoom
                    GpuGuideShape {
                        objectName: "viewportRulerLabel"
                        width: 100; height: 24
                        transformOrigin: Item.TopLeft
                        rotation: root.vertical ? -90 : 0
                        x: root.vertical ? 11 : tick.at+4
                        y: root.vertical ? tick.at+4 : 12
                        ShapePath {
                            strokeWidth: -1
                            strokeColor: "transparent"
                            fillColor: "#788491"
                            fillRule: ShapePath.WindingFill
                            PathSvg { path: CanvasFonts.rulerOutline(tick.value) }
                        }
                    }
                    Rectangle {
                        objectName: "viewportRulerTick"
                        x: root.vertical ? 17 : tick.at-.5
                        y: root.vertical ? tick.at-.5 : 17
                        width: root.vertical ? 7 : 1
                        height: root.vertical ? 1 : 7
                        color: "#46505b"
                        antialiasing: true
                    }
                }
            }
        }
    }
    onPaint: {
        if (GraphicsInfo.api===GraphicsInfo.Unknown || (gpuBackend && integralSize && (!nativeTicks.item || nativeTicks.item.curveRendererAvailable))) return
        const c=getContext("2d")
        c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height));c.fillStyle="#20262e";c.fillRect(0,0,width,height)
        c.fillStyle="#788491";c.strokeStyle="#46505b";c.font="9px sans-serif"
        for(let n=firstTick;n<=lastTick;++n) {
            const value=n*step,at=offset+value*zoom
            if(vertical) {
                c.save();c.translate(11,at+4);c.rotate(-Math.PI/2);c.fillText(value,0,0);c.restore()
                c.beginPath();c.moveTo(17,at);c.lineTo(24,at);c.stroke()
            } else {
                c.fillText(value,at+4,12)
                c.beginPath();c.moveTo(at,17);c.lineTo(at,24);c.stroke()
            }
        }
    }
    onDocumentLengthChanged: if(!gpuRuler)requestPaint()
    onZoomChanged: if(!gpuRuler)requestPaint()
    onOffsetChanged: if(!gpuRuler)requestPaint()
    onVerticalChanged: if(!gpuRuler)requestPaint()
    onVisibleChanged: if(visible && !gpuRuler)requestPaint()
}
