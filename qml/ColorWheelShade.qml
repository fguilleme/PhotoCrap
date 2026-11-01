import QtQuick

// The fixed spectrum is separate; RHI changes only the disk's opacity.
Item {
    id: root
    property real value: 1
    readonly property real center: width/2
    readonly property real radius: Math.min(width,height)/2-9
    readonly property real shadeOpacity: Math.max(0,Math.min(1,1-value))
    readonly property bool gpuBackend: GraphicsInfo.api===GraphicsInfo.Metal
        || GraphicsInfo.api===GraphicsInfo.Vulkan || GraphicsInfo.api===GraphicsInfo.OpenGL
        || GraphicsInfo.api===GraphicsInfo.Direct3D11 || GraphicsInfo.api===GraphicsInfo.Direct3D12
    readonly property bool integralSize: width===Math.round(width) && height===Math.round(height)
    readonly property bool shaderFailed: nativeShade.item!==null && nativeShade.item.status===ShaderEffect.Error
    readonly property bool gpuShade: gpuBackend && integralSize && radius>0 && !shaderFailed
    readonly property bool rasterActive: softwareShade.item!==null && softwareShade.item.context!==null
        && softwareShade.item.context!==undefined
    signal painted()
    function requestPaint() { if(softwareShade && softwareShade.item)softwareShade.item.requestPaint() }
    Loader {
        id: nativeShade
        anchors.fill: parent
        active: root.visible && root.gpuBackend && root.integralSize && root.radius>0
        visible: !root.shaderFailed
        sourceComponent: ShaderEffect {
            objectName: "colorWheelShadeShader"
            property size shadeSize: Qt.size(width,height)
            opacity: root.shadeOpacity
            vertexShader: "qrc:/PhotoCrap/src/render/shaders/colorwheelshade.vert.qsb"
            fragmentShader: "qrc:/PhotoCrap/src/render/shaders/colorwheelshade.frag.qsb"
            blending: true
        }
    }
    Loader {
        id: softwareShade
        anchors.fill: parent
        active: root.visible && !root.gpuShade
        sourceComponent: Canvas {
            objectName: "colorWheelShadeRaster"
            Component.onCompleted: requestPaint()
            onPainted: root.painted()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c=getContext("2d")
                c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height))
                const center=width/2,radius=Math.min(width,height)/2-9
                if(radius<=0)return
                c.beginPath();c.arc(center,center,radius,0,Math.PI*2)
                c.fillStyle="rgba(0,0,0,"+root.shadeOpacity+")";c.fill()
            }
        }
    }
    onValueChanged: if(visible)requestPaint()
    onVisibleChanged: if(visible)requestPaint()
}
