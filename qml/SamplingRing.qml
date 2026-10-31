import QtQuick

// Motion only changes the scene-graph transform; repaint only when colours change.
Canvas {
    id: root
    required property color sampleColor
    required property color previousColor
    width: 84
    height: 84
    enabled: false
    onSampleColorChanged: requestPaint()
    onPreviousColorChanged: requestPaint()
    onVisibleChanged: if (visible) requestPaint()
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
