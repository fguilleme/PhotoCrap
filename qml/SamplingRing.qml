import QtQuick

// RHI updates only the colours; the software backend keeps the original Canvas.
Item {
    id: root
    required property color sampleColor
    required property color previousColor
    width: 84
    height: 84
    enabled: false
    signal painted()
    readonly property bool gpuRing: GraphicsInfo.api === GraphicsInfo.Metal
        || GraphicsInfo.api === GraphicsInfo.Vulkan || GraphicsInfo.api === GraphicsInfo.OpenGL
        || GraphicsInfo.api === GraphicsInfo.Direct3D11 || GraphicsInfo.api === GraphicsInfo.Direct3D12
    readonly property bool gpuReady: nativeRing.item !== null && nativeRing.item.status===ShaderEffect.Compiled
    readonly property bool shaderFailed: nativeRing.item !== null && nativeRing.item.status===ShaderEffect.Error
    function requestPaint(){if(softwareRing && softwareRing.item)softwareRing.item.requestPaint()}
    Loader {
        id: nativeRing
        anchors.fill: parent
        active: root.visible && root.gpuRing
        visible: !root.shaderFailed
        sourceComponent: ShaderEffect {
            objectName: "samplingRingShader"
            property color sampleColor: root.sampleColor
            property color previousColor: root.previousColor
            property size ringSize: Qt.size(width,height)
            vertexShader: "qrc:/PhotoCrap/src/render/shaders/samplingring.vert.qsb"
            fragmentShader: "qrc:/PhotoCrap/src/render/shaders/samplingring.frag.qsb"
            blending: true
        }
    }
    Loader {
        id: softwareRing
        anchors.fill: parent
        active: root.visible && (!root.gpuRing || root.shaderFailed)
        sourceComponent: Canvas {
            objectName: "samplingRingRaster"
            Component.onCompleted: requestPaint()
            onPainted: root.painted()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c=getContext("2d")
                c.reset();c.clearRect(0,0,width,height)
                const x=width/2,y=height/2
                c.lineWidth=10
                c.beginPath();c.arc(x,y,35,Math.PI,Math.PI*2);c.strokeStyle=sampleColor;c.stroke()
                c.beginPath();c.arc(x,y,35,0,Math.PI);c.strokeStyle=previousColor;c.stroke()
                c.lineWidth=1;c.strokeStyle="#111111"
                c.beginPath();c.arc(x,y,41,0,Math.PI*2);c.stroke()
                c.strokeStyle="#ffffff";c.beginPath();c.arc(x,y,29,0,Math.PI*2);c.stroke()
            }
        }
    }
    onSampleColorChanged: requestPaint()
    onPreviousColorChanged: requestPaint()
    onVisibleChanged: if(visible)requestPaint()
}
