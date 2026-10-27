import QtQuick

ScreenOverlayCanvas {
    id: root
    required property var document
    enabled: false
    antialiasing: true
    onVisibleChanged: if(visible)requestPaint()
    onPaint: {
        const c=getContext("2d")
        c.reset();c.clearRect(0,0,width,height)
        const paths=document.penPaths,index=document.activePenPath
        if(index<0 || index>=paths.length)return
        const path=paths[index],p=path.points
        if(!p.length)return
        c.translate(-x,-y)
        function segment(a,b) { c.bezierCurveTo(a.outX*zoom,a.outY*zoom,b.inX*zoom,b.inY*zoom,b.x*zoom,b.y*zoom) }
        c.beginPath();c.moveTo(p[0].x*zoom,p[0].y*zoom)
        for(let i=1;i<p.length;++i)segment(p[i-1],p[i])
        if(path.closed)segment(p[p.length-1],p[0])
        c.strokeStyle="#1a2b3b";c.lineWidth=3;c.stroke()
        c.strokeStyle="#73c5ff";c.lineWidth=1.5;c.stroke()
        if(document.penDrawing && document.penRubberBand && !path.closed) {
            const last=p[p.length-1],to=document.penHover
            c.beginPath();c.moveTo(last.x*zoom,last.y*zoom)
            c.bezierCurveTo(last.outX*zoom,last.outY*zoom,to.x*zoom,to.y*zoom,to.x*zoom,to.y*zoom)
            c.setLineDash([4,3]);c.strokeStyle="#73c5ff";c.lineWidth=1.5;c.stroke();c.setLineDash([])
        }
        for(let i=0;i<p.length;++i) {
            const a=p[i],px=a.x*zoom,py=a.y*zoom
            if(a.selected)for(const h of [{x:a.inX*zoom,y:a.inY*zoom},{x:a.outX*zoom,y:a.outY*zoom}]) {
                if(Math.hypot(h.x-px,h.y-py)<.1)continue
                c.beginPath();c.moveTo(px,py);c.lineTo(h.x,h.y);c.strokeStyle="#73c5ff";c.lineWidth=1;c.stroke()
                c.beginPath();c.arc(h.x,h.y,3,0,2*Math.PI);c.fillStyle="#eef8ff";c.fill();c.strokeStyle="#1b4968";c.stroke()
            }
            c.fillStyle=a.selected?"#73c5ff":"#ffffff";c.fillRect(px-3,py-3,6,6)
            c.strokeStyle="#183e59";c.lineWidth=1;c.strokeRect(px-3,py-3,6,6)
        }
    }
    Connections {
        target: root.document
        function onPenPathsChanged(){root.requestPaint()}
        function onPenHoverChanged(){if(root.document.penRubberBand)root.requestPaint()}
        function onPenOptionsChanged(){root.requestPaint()}
    }
    onZoomChanged: requestPaint()
}
