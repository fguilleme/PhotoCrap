import QtQuick
import QtQuick.Window
import PhotoCrap.Native 1.0

Item {
    id: root
    required property point startPoint
    required property point controlPoint
    required property point endPoint
    required property rect viewBounds
    property point imageOrigin: Qt.point(0,0)
    enabled: false
    signal painted()
    readonly property bool gpuBackend: GraphicsInfo.api===GraphicsInfo.Metal
        || GraphicsInfo.api===GraphicsInfo.Vulkan || GraphicsInfo.api===GraphicsInfo.OpenGL
        || GraphicsInfo.api===GraphicsInfo.Direct3D11 || GraphicsInfo.api===GraphicsInfo.Direct3D12
    // The control polygon bounds the arc length, and hence dash preparation.
    readonly property real controlLength: Math.hypot(controlPoint.x-startPoint.x,controlPoint.y-startPoint.y)
        +Math.hypot(endPoint.x-controlPoint.x,endPoint.y-controlPoint.y)
    // Raster clipping can change Qt's dash subdivision. Keep that path when
    // the view cuts the control hull or its antialias fringe.
    readonly property bool curveFits: Math.min(startPoint.x,controlPoint.x,endPoint.x)>=viewBounds.x+2
        && Math.min(startPoint.y,controlPoint.y,endPoint.y)>=viewBounds.y+2
        && Math.max(startPoint.x,controlPoint.x,endPoint.x)<=viewBounds.x+viewBounds.width-2
        && Math.max(startPoint.y,controlPoint.y,endPoint.y)<=viewBounds.y+viewBounds.height-2
    readonly property bool gpuEligible: gpuBackend && curveFits && controlLength<=8192
        && Screen.devicePixelRatio>0 && Screen.devicePixelRatio<=8
    readonly property bool gpuBand: gpuEligible && nativeBand.item!==null
        && nativeBand.item.curveRendererAvailable
    readonly property bool rasterActive: softwareBand.item!==null
        && softwareBand.item.context!==null && softwareBand.item.context!==undefined
    readonly property string curvePath: gpuBand
        ? PenBandGeometry.outline(startPoint,controlPoint,endPoint,imageOrigin,Screen.devicePixelRatio) : ""
    function requestPaint() {
        if(visible && softwareBand && softwareBand.item)softwareBand.item.requestPaint()
    }
    Loader {
        id: nativeBand
        anchors.fill: parent
        clip: true
        active: root.visible && root.width>0 && root.height>0 && root.controlLength>0 && root.gpuEligible
        sourceComponent: PenRubberGuide {svg:root.curvePath}
    }
    Loader {
        id: softwareBand
        anchors.fill: parent
        active: root.visible && root.width>0 && root.height>0 && root.controlLength>0 && !root.gpuBand
        sourceComponent: Canvas {
            objectName: "penRasterRubberBand"
            antialiasing: true
            Component.onCompleted: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPainted: root.painted()
            onPaint: {
                const c=getContext("2d")
                c.reset();c.clearRect(0,0,width,height)
                c.translate(-root.imageOrigin.x,-root.imageOrigin.y)
                c.beginPath();c.moveTo(root.startPoint.x,root.startPoint.y)
                c.bezierCurveTo(root.controlPoint.x,root.controlPoint.y,
                    root.endPoint.x,root.endPoint.y,root.endPoint.x,root.endPoint.y)
                c.setLineDash([4,3]);c.strokeStyle="#73c5ff";c.lineWidth=1.5;c.stroke()
            }
        }
    }
    onStartPointChanged: requestPaint()
    onControlPointChanged: requestPaint()
    onEndPointChanged: requestPaint()
    onImageOriginChanged: requestPaint()
    onVisibleChanged: if(visible)requestPaint()
}
