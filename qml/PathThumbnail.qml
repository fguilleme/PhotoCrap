import QtQuick

Canvas {
    id: root
    required property var pathData
    property real documentWidth: 1
    property real documentHeight: 1
    implicitWidth: 48
    implicitHeight: 40
    antialiasing: true
    Component.onCompleted: requestPaint()
    onAvailableChanged: if (available) requestPaint()
    onVisibleChanged: if (visible) requestPaint()
    onPathDataChanged: requestPaint()
    onDocumentWidthChanged: requestPaint()
    onDocumentHeightChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const c = getContext("2d")
        c.reset()
        c.fillStyle = "#cbd0d5"; c.fillRect(0,0,width,height)
        const scale = Math.min((width-6)/Math.max(1,documentWidth), (height-6)/Math.max(1,documentHeight))
        c.translate((width-documentWidth*scale)/2,(height-documentHeight*scale)/2)
        c.scale(scale,scale)
        c.fillStyle = "#f3f3f3";c.fillRect(0,0,documentWidth,documentHeight)
        const points = pathData ? pathData.points : []
        if (!points.length) return
        c.beginPath();c.moveTo(points[0].x,points[0].y)
        function segment(a,b) { c.bezierCurveTo(a.outX,a.outY,b.inX,b.inY,b.x,b.y) }
        for (let i=1;i<points.length;++i) segment(points[i-1],points[i])
        if (pathData.closed) { segment(points[points.length-1],points[0]); c.closePath() }
        c.fillStyle = "#6a7078";c.fill()
        c.strokeStyle = "#273340";c.lineWidth=1/scale;c.stroke()
    }
}
