import QtQuick
import QtQuick.Shapes
import PhotoCrap.Native 1.0

Item {
    id: root
    objectName: "curvesGpuGraph"
    property var histogram: []
    property var points: []
    property var samples: []
    property int activePoint: -1
    readonly property bool curveRendererAvailable: grid.curveRendererAvailable
    visible: curveRendererAvailable
    enabled: false
    layer.enabled: true
    layer.smooth: true
    layer.samples: 4
    clip: true
    readonly property real leftEdge: 18
    readonly property real rightEdge: width-10
    readonly property real topEdge: 10
    readonly property real bottomEdge: height-20
    function px(value) { return leftEdge+value/255*(rightEdge-leftEdge) }
    function py(value) { return bottomEdge-value/255*(bottomEdge-topEdge) }
    readonly property string gridPath: {
        let path=""
        for(let i=0;i<=4;++i) {
            const x=leftEdge+(rightEdge-leftEdge)*i/4,y=topEdge+(bottomEdge-topEdge)*i/4
            path+="M"+x+" "+topEdge+"L"+x+" "+bottomEdge
                +"M"+leftEdge+" "+y+"L"+rightEdge+" "+y
        }
        return path
    }
    readonly property string histogramPath: {
        if(!histogram || histogram.length!==256)return ""
        let path="M"+leftEdge+" "+bottomEdge
        for(let b=0;b<256;++b)
            path+="L"+px(b)+" "+(bottomEdge-Number(histogram[b])*(bottomEdge-topEdge)*.82)
        return path+"L"+rightEdge+" "+bottomEdge+"Z"
    }
    readonly property string curveOutline: CurvesGeometry.stroke(samples,rightEdge-leftEdge,bottomEdge-topEdge)
    component Stroke: ShapePath {
        id: stroke
        property string svg
        fillColor: "transparent"
        capStyle: ShapePath.FlatCap
        joinStyle: ShapePath.MiterJoin
        miterLimit: 10
        PathSvg { path: stroke.svg }
    }
    Rectangle { anchors.fill: parent; color: "#151a20" }
    GpuGuideShape {
        id: grid
        anchors.fill: parent
        Stroke { svg: root.gridPath; strokeWidth: 1; strokeColor: "#303943" }
    }
    GpuGuideShape {
        objectName: "curvesHistogramShape"
        anchors.fill: parent
        ShapePath {
            strokeWidth: -1; strokeColor: "transparent"
            fillColor: Qt.rgba(185/255,197/255,208/255,.25)
            PathSvg { path: root.histogramPath }
        }
    }
    GpuGuideShape {
        anchors.fill: parent
        Stroke {
            svg: "M"+root.leftEdge+" "+root.bottomEdge+"L"+root.rightEdge+" "+root.topEdge
            strokeWidth: 1; strokeColor: "#65717c"
        }
    }
    GpuGuideShape {
        objectName: "curvesTransferShape"
        x: root.leftEdge; y: root.topEdge
        width: root.rightEdge-root.leftEdge; height: root.bottomEdge-root.topEdge
        ShapePath {
            strokeWidth: -1; strokeColor: "transparent"
            fillColor: "#e8edf1"
            fillRule: ShapePath.WindingFill
            PathSvg { path: root.curveOutline }
        }
    }
    Repeater {
        model: root.points.length
        delegate: GpuGuideShape {
            id: pointMarker
            required property int index
            objectName: "curvesGpuPoint"+index
            readonly property var point: root.points[index] || ({x:0,y:0})
            readonly property real radius: index===root.activePoint ? 6 : 5
            x: root.px(point.x); y: root.py(point.y)
            width: 2*radius; height: 2*radius
            ShapePath {
                strokeWidth: 2
                strokeColor: index===root.activePoint ? "#1d80e5" : "#dce4ea"
                fillColor: index===root.activePoint ? "#ffffff" : "#222a32"
                PathAngleArc { centerX: 0; centerY: 0; radiusX: pointMarker.radius; radiusY: pointMarker.radius; startAngle: 0; sweepAngle: 360 }
            }
        }
    }
    GpuGuideShape {
        anchors.fill: parent
        Stroke {
            svg: "M"+root.leftEdge+" "+root.topEdge+"L"+root.rightEdge+" "+root.topEdge
                +"L"+root.rightEdge+" "+root.bottomEdge+"L"+root.leftEdge+" "+root.bottomEdge+"Z"
            strokeWidth: 1; strokeColor: "#66727d"
        }
    }
}
