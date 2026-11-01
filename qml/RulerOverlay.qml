import QtQuick

Item {
    id: root
    required property var viewportItem
    required property point startPoint
    required property point endPoint
    property real zoom: 1
    enabled: false
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
    readonly property rect lineBounds: {
        const left=Math.max(screenBounds.x,Math.floor(Math.min(startPoint.x,endPoint.x)*zoom-6))
        const top=Math.max(screenBounds.y,Math.floor(Math.min(startPoint.y,endPoint.y)*zoom-6))
        const right=Math.min(screenBounds.x+screenBounds.width,Math.ceil(Math.max(startPoint.x,endPoint.x)*zoom+6))
        const bottom=Math.min(screenBounds.y+screenBounds.height,Math.ceil(Math.max(startPoint.y,endPoint.y)*zoom+6))
        return Qt.rect(left,top,Math.max(0,right-left),Math.max(0,bottom-top))
    }
    x: lineBounds.x
    y: lineBounds.y
    width: lineBounds.width
    height: lineBounds.height
    function requestPaint(){if(softwareGuides && softwareGuides.item)softwareGuides.item.requestPaint()}
    Loader {
        id: nativeGuides
        anchors.fill: parent
        active: root.visible && root.gpuBackend && width>0 && height>0
        sourceComponent: RulerGuides {
            startPoint: Qt.point(root.startPoint.x*root.zoom-root.x,root.startPoint.y*root.zoom-root.y)
            endPoint: Qt.point(root.endPoint.x*root.zoom-root.x,root.endPoint.y*root.zoom-root.y)
        }
    }
    Loader {
        id: softwareGuides
        anchors.fill: parent
        active: root.visible && (!root.gpuBackend || (nativeGuides.item && !nativeGuides.item.curveRendererAvailable))
        sourceComponent: Canvas {
            objectName: "rulerRasterGuides"
            antialiasing: true
            onPainted: root.painted()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Component.onCompleted: requestPaint()
            onPaint: {
                const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
                c.translate(-root.x,-root.y)
                const x1=startPoint.x*zoom,y1=startPoint.y*zoom
                const x2=endPoint.x*zoom,y2=endPoint.y*zoom
                c.strokeStyle="rgba(0,0,0,.85)";c.lineWidth=4
                c.beginPath();c.moveTo(x1,y1);c.lineTo(x2,y2);c.stroke()
                c.strokeStyle="#f5f7f9";c.lineWidth=2
                c.beginPath();c.moveTo(x1,y1);c.lineTo(x2,y2);c.stroke()
                for(const p of [{x:x1,y:y1},{x:x2,y:y2}]) {
                    c.beginPath();c.arc(p.x,p.y,4,0,Math.PI*2)
                    c.fillStyle="#f5f7f9";c.fill();c.strokeStyle="#101419";c.lineWidth=1;c.stroke()
                }
            }
        }
    }
    onXChanged: if(visible)requestPaint()
    onYChanged: if(visible)requestPaint()
    onWidthChanged: if(visible)requestPaint()
    onHeightChanged: if(visible)requestPaint()
    onStartPointChanged: if(visible)requestPaint()
    onEndPointChanged: if(visible)requestPaint()
    onZoomChanged: if(visible)requestPaint()
    onVisibleChanged: if(visible)requestPaint()
}
