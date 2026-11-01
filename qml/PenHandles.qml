import QtQuick

// A bounded handle sheet. Geometry is independent of the provisional endpoint.
Item {
    id: root
    required property var points
    property real zoom: 1
    property point imageOrigin: Qt.point(0,0)
    enabled: false
    signal painted()
    readonly property bool gpuBackend: GraphicsInfo.api===GraphicsInfo.Metal
        || GraphicsInfo.api===GraphicsInfo.Vulkan || GraphicsInfo.api===GraphicsInfo.OpenGL
        || GraphicsInfo.api===GraphicsInfo.Direct3D11 || GraphicsInfo.api===GraphicsInfo.Direct3D12
    readonly property bool gpuHandles: nativeHandles.item!==null
        && nativeHandles.item.curveRendererAvailable
    readonly property bool rasterActive: softwareHandles.item!==null
        && softwareHandles.item.context!==null && softwareHandles.item.context!==undefined
    readonly property var primitives: {
        if(!gpuHandles)return []
        const result=[]
        function visibleBox(l,t,r,b,pad) {
            return r+pad>=0 && l-pad<=root.width && b+pad>=0 && t-pad<=root.height
        }
        function add(svg,stroke,fill) { result.push({svg:svg,stroke:stroke,fill:fill}) }
        for(const a of points) {
            const px=a.x*zoom-imageOrigin.x,py=a.y*zoom-imageOrigin.y
            if(a.selected) {
                for(const h of [{x:a.inX*zoom-imageOrigin.x,y:a.inY*zoom-imageOrigin.y},
                                 {x:a.outX*zoom-imageOrigin.x,y:a.outY*zoom-imageOrigin.y}]) {
                    if(Math.hypot(h.x-px,h.y-py)<.1)continue
                    if(visibleBox(Math.min(px,h.x),Math.min(py,h.y),Math.max(px,h.x),Math.max(py,h.y),1))
                        add("M"+px+" "+py+"L"+h.x+" "+h.y,"#73c5ff","transparent")
                    if(visibleBox(h.x,h.y,h.x,h.y,4))
                        add("M"+(h.x+3)+" "+h.y+"A3 3 0 1 0 "+(h.x-3)+" "+h.y
                            +"A3 3 0 1 0 "+(h.x+3)+" "+h.y+"Z","#1b4968","#eef8ff")
                }
            }
            if(visibleBox(px,py,px,py,4))
                add("M"+(px-3)+" "+(py-3)+"H"+(px+3)+"V"+(py+3)+"H"+(px-3)+"Z",
                    "#183e59",a.selected ? "#73c5ff" : "#ffffff")
        }
        return result
    }
    function requestPaint() {
        if(visible && softwareHandles && softwareHandles.item)softwareHandles.item.requestPaint()
    }
    Loader {
        id: nativeHandles
        anchors.fill: parent
        clip: true
        active: root.visible && root.gpuBackend && root.width>0 && root.height>0
        sourceComponent: PenHandleGuides { primitives: root.primitives }
    }
    Loader {
        id: softwareHandles
        anchors.fill: parent
        active: root.visible && root.width>0 && root.height>0 && !root.gpuHandles
        sourceComponent: Canvas {
            objectName: "penRasterHandles"
            antialiasing: true
            Component.onCompleted: requestPaint()
            onPainted: root.painted()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c=getContext("2d")
                c.reset();c.clearRect(0,0,width,height)
                c.translate(-root.imageOrigin.x,-root.imageOrigin.y)
                for(const a of root.points) {
                    const px=a.x*root.zoom,py=a.y*root.zoom
                    if(a.selected) {
                        for(const h of [{x:a.inX*root.zoom,y:a.inY*root.zoom},
                                         {x:a.outX*root.zoom,y:a.outY*root.zoom}]) {
                            if(Math.hypot(h.x-px,h.y-py)<.1)continue
                            c.beginPath();c.moveTo(px,py);c.lineTo(h.x,h.y)
                            c.strokeStyle="#73c5ff";c.lineWidth=1;c.stroke()
                            c.beginPath();c.arc(h.x,h.y,3,0,2*Math.PI)
                            c.fillStyle="#eef8ff";c.fill();c.strokeStyle="#1b4968";c.stroke()
                        }
                    }
                    c.fillStyle=a.selected ? "#73c5ff" : "#ffffff";c.fillRect(px-3,py-3,6,6)
                    c.strokeStyle="#183e59";c.lineWidth=1;c.strokeRect(px-3,py-3,6,6)
                }
            }
        }
    }
    onPointsChanged: requestPaint()
    onZoomChanged: requestPaint()
    onImageOriginChanged: requestPaint()
    onVisibleChanged: if(visible)requestPaint()
}
