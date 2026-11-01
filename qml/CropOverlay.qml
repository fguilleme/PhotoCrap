import QtQuick
import PhotoCrap.Native 1.0

Item {
    id: root
    required property var viewportItem
    property real zoom: 1
    // Visible artboard bounds; native guides never allocate a viewport-sized raster.
    readonly property rect screenBounds: {
        if(!parent || !viewportItem)return Qt.rect(0,0,0,0)
        const scrollX=viewportItem.contentX,scrollY=viewportItem.contentY
        const scale=zoom,rotation=parent.rotation,originX=parent.x,originY=parent.y
        const sourceWidth=parent.width,sourceHeight=parent.height
        const w=viewportItem.width,h=viewportItem.height
        const p=[viewportItem.mapToItem(parent,0,0),viewportItem.mapToItem(parent,w,0),
                 viewportItem.mapToItem(parent,w,h),viewportItem.mapToItem(parent,0,h)]
        const l=Math.max(0,Math.floor(Math.min.apply(null,p.map(p=>p.x))))
        const t=Math.max(0,Math.floor(Math.min.apply(null,p.map(p=>p.y))))
        const r=Math.min(parent.width,Math.ceil(Math.max.apply(null,p.map(p=>p.x))))
        const b=Math.min(parent.height,Math.ceil(Math.max.apply(null,p.map(p=>p.y))))
        return Qt.rect(l,t,Math.max(0,r-l),Math.max(0,b-t))
    }
    signal painted()
    function requestPaint(){if(softwareGuides && softwareGuides.item)softwareGuides.item.requestPaint()}

    required property var document
    property bool showGrid: true
    property bool perspective: document.tool === "perspectiveCrop"
    readonly property var points: document.cropPoints
    readonly property bool gpuShade: GraphicsInfo.api === GraphicsInfo.Metal
        || GraphicsInfo.api === GraphicsInfo.Vulkan || GraphicsInfo.api === GraphicsInfo.OpenGL
        || GraphicsInfo.api === GraphicsInfo.Direct3D11 || GraphicsInfo.api === GraphicsInfo.Direct3D12
    readonly property rect guideBounds: {
        if (points.length!==4) return Qt.rect(0,0,0,0)
        const left=Math.max(screenBounds.x,Math.floor(Math.min.apply(null,points.map(p=>p.x*zoom))-16))
        const top=Math.max(screenBounds.y,Math.floor(Math.min.apply(null,points.map(p=>p.y*zoom))-16))
        const right=Math.min(screenBounds.x+screenBounds.width,Math.ceil(Math.max.apply(null,points.map(p=>p.x*zoom))+16))
        const bottom=Math.min(screenBounds.y+screenBounds.height,Math.ceil(Math.max.apply(null,points.map(p=>p.y*zoom))+16))
        return Qt.rect(left,top,Math.max(0,right-left),Math.max(0,bottom-top))
    }
    x: gpuShade ? guideBounds.x : screenBounds.x
    y: gpuShade ? guideBounds.y : screenBounds.y
    width: gpuShade ? guideBounds.width : screenBounds.width
    height: gpuShade ? guideBounds.height : screenBounds.height
    enabled: false
    visible: points.length === 4
    antialiasing: true
    function cursorAt(x,y) {
        if(points.length!==4) return Qt.CrossCursor
        for(let i=0;i<4;i++) {
            if(Math.hypot(x-points[i].x*zoom,y-points[i].y*zoom)<=10)
                return perspective ? Qt.CrossCursor : i%2===0 ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
            if(!perspective) {
                const p=points[i],q=points[(i+1)%4]
                if(Math.hypot(x-(p.x+q.x)*zoom/2,y-(p.y+q.y)*zoom/2)<=10)
                    return i%2===0 ? Qt.SizeVerCursor : Qt.SizeHorCursor
            }
        }
        let inside=false
        for(let i=0,j=3;i<4;j=i++) {
            const p=points[i],q=points[j],px=x/zoom,py=y/zoom
            if((p.y>py)!==(q.y>py) && px<(q.x-p.x)*(py-p.y)/(q.y-p.y)+p.x) inside=!inside
        }
        return inside ? Qt.SizeAllCursor : Qt.CrossCursor
    }
    Loader {
        id: nativeGuides
        anchors.fill: parent
        active: root.gpuShade && width>0 && height>0
        sourceComponent: CropGuides {
            points: root.points
            zoom: root.zoom
            origin: Qt.point(root.x,root.y)
            showGrid: root.showGrid
            perspective: root.perspective
        }
    }
    Loader {
        id: softwareGuides
        anchors.fill: parent
        active: !root.gpuShade || (nativeGuides.item && !nativeGuides.item.curveRendererAvailable)
        sourceComponent: Canvas {
            objectName: "cropRasterGuides"
            antialiasing: true
            onPainted: root.painted()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Component.onCompleted: requestPaint()
            onPaint: {
                const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
                if(points.length!==4) return
                const p=points.map(function(pt){return {x:pt.x*zoom-root.x,y:pt.y*zoom-root.y}})
                // The reverse-wound frame leaves the retained area unobscured.
                if(!gpuShade) {
                    c.beginPath();c.rect(0,0,width,height)
                    c.moveTo(p[3].x,p[3].y)
                    for(let i=2;i>=0;i--) c.lineTo(p[i].x,p[i].y)
                    c.closePath();c.fillStyle="rgba(0,0,0,.55)";c.fill()
                }
                c.beginPath();c.moveTo(p[0].x,p[0].y)
                for(let i=1;i<4;i++) c.lineTo(p[i].x,p[i].y)
                c.closePath();c.lineWidth=3;c.strokeStyle="rgba(0,0,0,.9)";c.stroke()
                c.lineWidth=1;c.strokeStyle="#ffffff";c.stroke()
                if(showGrid) {
                    c.beginPath()
                    for(let i=1;i<3;i++) {
                        const t=i/3
                        c.moveTo(p[0].x+(p[1].x-p[0].x)*t,p[0].y+(p[1].y-p[0].y)*t)
                        c.lineTo(p[3].x+(p[2].x-p[3].x)*t,p[3].y+(p[2].y-p[3].y)*t)
                        c.moveTo(p[0].x+(p[3].x-p[0].x)*t,p[0].y+(p[3].y-p[0].y)*t)
                        c.lineTo(p[1].x+(p[2].x-p[1].x)*t,p[1].y+(p[2].y-p[1].y)*t)
                    }
                    c.lineWidth=2;c.strokeStyle="rgba(0,0,0,.65)";c.stroke()
                    c.lineWidth=1;c.strokeStyle="rgba(255,255,255,.7)";c.stroke()
                }
                const handles=p.slice()
                if(!perspective) for(let i=0;i<4;i++) handles.push({x:(p[i].x+p[(i+1)%4].x)/2,y:(p[i].y+p[(i+1)%4].y)/2})
                for(const pt of handles) {
                    c.fillStyle="#ffffff";c.fillRect(pt.x-4,pt.y-4,8,8)
                    c.lineWidth=1;c.strokeStyle="#111111";c.strokeRect(pt.x-4,pt.y-4,8,8)
                }
            }
        }
    }

    CropShade {
        id: shade
        objectName: "cropShade"
        visible: root.gpuShade
        enabled: false
        z: -1
        x: root.screenBounds.x-root.x; y: root.screenBounds.y-root.y
        width: root.screenBounds.width; height: root.screenBounds.height
        points: root.points
        zoom: root.zoom
        origin: Qt.point(root.screenBounds.x,root.screenBounds.y)
    }
    onXChanged: requestPaint()
    onYChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onGpuShadeChanged: if(visible)requestPaint()
    onZoomChanged: requestPaint()
    onShowGridChanged: requestPaint()
    onPerspectiveChanged: requestPaint()
    onVisibleChanged: requestPaint()
    Connections { target: root.document; function onCropChanged() { root.requestPaint() } }
}
