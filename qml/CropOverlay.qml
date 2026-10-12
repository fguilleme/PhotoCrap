import QtQuick

ScreenOverlayCanvas {
    id: root
    required property var document
    property bool showGrid: true
    property bool perspective: document.tool === "perspectiveCrop"
    enabled: false
    visible: document.cropPoints.length === 4
    antialiasing: true
    function cursorAt(x,y) {
        const points=document.cropPoints
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
    onPaint: {
        const points=document.cropPoints
        const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
        if(points.length!==4) return
        const p=points.map(function(pt){return {x:pt.x*zoom-root.x,y:pt.y*zoom-root.y}})
        // The reverse-wound frame leaves the retained area unobscured.
        c.beginPath();c.rect(0,0,width,height)
        c.moveTo(p[3].x,p[3].y)
        for(let i=2;i>=0;i--) c.lineTo(p[i].x,p[i].y)
        c.closePath();c.fillStyle="rgba(0,0,0,.55)";c.fill()
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
    onZoomChanged: requestPaint()
    onShowGridChanged: requestPaint()
    onPerspectiveChanged: requestPaint()
    onVisibleChanged: requestPaint()
    Connections { target: root.document; function onCropChanged() { root.requestPaint() } }
}
