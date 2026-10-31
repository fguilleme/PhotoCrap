import QtQuick

ScreenOverlayCanvas {
    id: root
    required property point startPoint
    required property point endPoint
    enabled: false
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
    onStartPointChanged: if (visible) requestPaint()
    onEndPointChanged: if (visible) requestPaint()
    onZoomChanged: if (visible) requestPaint()
    onVisibleChanged: if (visible) requestPaint()
    onPaint: {
        const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
        c.translate(-x,-y)
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
