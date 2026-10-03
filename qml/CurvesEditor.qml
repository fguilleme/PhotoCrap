import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "curvesEditorRoot"
    property var histogram: []
    property var sourcePoints: [{"x": 0, "y": 0}, {"x": 255, "y": 255}]
    property var points: []
    property int activePoint: -1
    property bool dragging: false
    signal editStarted()
    signal previewed(var points)
    signal committed(var points)
    spacing: 6

    function copyPoints(value) {
        const result=[]
        if(value) for(let i=0;i<value.length;i++)
            result.push({x:Math.max(0,Math.min(255,Number(value[i].x))),
                         y:Math.max(0,Math.min(255,Number(value[i].y)))})
        if(result.length<2) return [{x:0,y:0},{x:255,y:255}]
        result.sort((a,b)=>a.x-b.x)
        return result
    }
    function sync() { if(!dragging) { points=copyPoints(sourcePoints); curveCanvas.requestPaint() } }
    function emittedPoints() { return copyPoints(points) }
    function curveValue(input) {
        if(points.length<2)return input
        const delta=[],slopes=[]
        for(let i=0;i+1<points.length;i++)
            delta.push((points[i+1].y-points[i].y)/Math.max(.001,points[i+1].x-points[i].x))
        slopes.push(delta[0])
        for(let i=1;i+1<points.length;i++) {
            if(delta[i-1]*delta[i]<=0)slopes.push(0)
            else {
                const left=points[i].x-points[i-1].x,right=points[i+1].x-points[i].x
                slopes.push((left+right)/((left+2*right)/(3*delta[i-1])+(2*left+right)/(3*delta[i])))
            }
        }
        slopes.push(delta[delta.length-1])
        if(input<=points[0].x)return points[0].y
        if(input>=points[points.length-1].x)return points[points.length-1].y
        let segment=0
        while(segment+1<points.length-1 && input>points[segment+1].x)segment++
        const width=points[segment+1].x-points[segment].x
        const t=(input-points[segment].x)/width,t2=t*t,t3=t2*t
        return (2*t3-3*t2+1)*points[segment].y+(t3-2*t2+t)*width*slopes[segment]
                +(-2*t3+3*t2)*points[segment+1].y+(t3-t2)*width*slopes[segment+1]
    }
    onSourcePointsChanged: sync()
    onHistogramChanged: curveCanvas.requestPaint()
    Component.onCompleted: sync()

    Label { text: qsTr("RVB composite"); color: "#c4cdd5" }
    Canvas {
        id: curveCanvas
        objectName: "curvesEditor"
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(width, 320)
        Layout.minimumHeight: 230
        property real leftEdge: 18
        property real rightEdge: width-10
        property real topEdge: 10
        property real bottomEdge: height-20
        function px(value) { return leftEdge+value/255*(rightEdge-leftEdge) }
        function py(value) { return bottomEdge-value/255*(bottomEdge-topEdge) }
        function valueX(pixel) { return Math.max(0,Math.min(255,(pixel-leftEdge)/(rightEdge-leftEdge)*255)) }
        function valueY(pixel) { return Math.max(0,Math.min(255,(bottomEdge-pixel)/(bottomEdge-topEdge)*255)) }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const c=getContext("2d"); c.reset(); c.fillStyle="#151a20"; c.fillRect(0,0,width,height)
            c.strokeStyle="#303943"; c.lineWidth=1
            for(let i=0;i<=4;i++) {
                const x=leftEdge+(rightEdge-leftEdge)*i/4, y=topEdge+(bottomEdge-topEdge)*i/4
                c.beginPath();c.moveTo(x,topEdge);c.lineTo(x,bottomEdge);c.stroke()
                c.beginPath();c.moveTo(leftEdge,y);c.lineTo(rightEdge,y);c.stroke()
            }
            if(root.histogram && root.histogram.length===256) {
                c.beginPath();c.moveTo(leftEdge,bottomEdge)
                for(let b=0;b<256;b++) c.lineTo(px(b),bottomEdge-Number(root.histogram[b])*(bottomEdge-topEdge)*.82)
                c.lineTo(rightEdge,bottomEdge);c.closePath();c.fillStyle="rgba(185,197,208,.25)";c.fill()
            }
            c.strokeStyle="#65717c";c.beginPath();c.moveTo(leftEdge,bottomEdge);c.lineTo(rightEdge,topEdge);c.stroke()
            c.strokeStyle="#e8edf1";c.lineWidth=2;c.beginPath();c.moveTo(px(0),py(root.curveValue(0)))
            for(let p=1;p<256;p++)c.lineTo(px(p),py(root.curveValue(p)))
            c.stroke()
            for(let j=0;j<root.points.length;j++) {
                const point=root.points[j], x=px(point.x), y=py(point.y)
                c.beginPath();c.arc(x,y,j===root.activePoint?6:5,0,Math.PI*2)
                c.fillStyle=j===root.activePoint?"#ffffff":"#222a32";c.fill()
                c.strokeStyle=j===root.activePoint?"#1d80e5":"#dce4ea";c.lineWidth=2;c.stroke()
            }
            c.strokeStyle="#66727d";c.lineWidth=1;c.strokeRect(leftEdge,topEdge,rightEdge-leftEdge,bottomEdge-topEdge)
        }
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: function(mouse) {
                let nearest=-1, distance=14
                for(let i=0;i<root.points.length;i++) {
                    const dx=mouse.x-curveCanvas.px(root.points[i].x),dy=mouse.y-curveCanvas.py(root.points[i].y)
                    const d=Math.sqrt(dx*dx+dy*dy);if(d<distance){distance=d;nearest=i}
                }
                if(mouse.button===Qt.RightButton) {
                    if(nearest>0 && nearest<root.points.length-1) {
                        root.editStarted();root.points.splice(nearest,1);root.points=root.points.slice();root.activePoint=-1
                        root.committed(root.emittedPoints());curveCanvas.requestPaint()
                    }
                    return
                }
                root.editStarted();root.dragging=true
                if(nearest<0) {
                    root.points.push({x:curveCanvas.valueX(mouse.x),y:curveCanvas.valueY(mouse.y)})
                    root.points.sort((a,b)=>a.x-b.x);nearest=root.points.findIndex(p=>Math.abs(p.x-curveCanvas.valueX(mouse.x))<.5)
                }
                root.activePoint=nearest;updatePoint(mouse.x,mouse.y)
            }
            function updatePoint(x,y) {
                if(root.activePoint<0)return
                const previous=root.activePoint>0?root.points[root.activePoint-1].x+1:0
                const next=root.activePoint+1<root.points.length?root.points[root.activePoint+1].x-1:255
                const lockedX=root.activePoint===0?0:root.activePoint===root.points.length-1?255:Math.max(previous,Math.min(next,curveCanvas.valueX(x)))
                root.points[root.activePoint]={x:lockedX,y:curveCanvas.valueY(y)};root.points=root.points.slice()
                root.previewed(root.emittedPoints());curveCanvas.requestPaint()
            }
            onPositionChanged: function(mouse) { if(pressed && (mouse.buttons & Qt.LeftButton)) updatePoint(mouse.x,mouse.y) }
            onReleased: {
                if(root.dragging)root.committed(root.emittedPoints())
                root.dragging=false;curveCanvas.requestPaint()
            }
            onCanceled: root.dragging=false
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Ombres"); color: "#7f8b96" }
        Item { Layout.fillWidth: true }
        Label { text: qsTr("Clic : ajouter · clic droit : supprimer"); color: "#7f8b96"; font.pixelSize: 10 }
        Item { Layout.fillWidth: true }
        Label { text: qsTr("Hautes lumières"); color: "#7f8b96" }
    }
}
