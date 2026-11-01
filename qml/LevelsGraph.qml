import QtQuick
import QtQuick.Shapes

Item {
    id: root
    objectName: output ? "levelsOutputGpuGraph" : "levelsInputGpuGraph"
    property bool output: false
    property var histogram: []
    property real black: 0
    property real white: 255
    property real gamma: 1
    readonly property bool curveRendererAvailable: grid.curveRendererAvailable
    visible: curveRendererAvailable
    enabled: false
    layer.enabled: true
    layer.smooth: true
    clip: true
    readonly property real leftEdge: 14
    readonly property real rightEdge: width-14
    readonly property real topEdge: 8
    readonly property real bottomEdge: height-28
    function px(value) { return leftEdge+value/255*(rightEdge-leftEdge) }
    function gammaT(value) { return value>=1?.5-(value-1)/4:.5+(1-value)/1.8 }
    // Values and size only: moving the markers leaves both paths unchanged.
    readonly property string histogramPath: {
        if(output || !histogram || histogram.length!==256)return ""
        let path="M"+leftEdge+" "+bottomEdge
        for(let b=0;b<256;++b)
            path+="L"+px(b)+" "+(bottomEdge-Number(histogram[b])*(bottomEdge-topEdge)*.92)
        return path+"L"+rightEdge+" "+bottomEdge+"Z"
    }
    readonly property string gridPath: {
        if(output)return ""
        let path=""
        for(let i=0;i<=4;++i) {
            const x=leftEdge+(rightEdge-leftEdge)*i/4
            path+="M"+x+" "+topEdge+"L"+x+" "+bottomEdge
        }
        return path
    }
    component Stroke: ShapePath {
        id: stroke
        property string svg
        fillColor: "transparent"
        capStyle: ShapePath.FlatCap
        joinStyle: ShapePath.MiterJoin
        miterLimit: 10
        PathSvg { path: stroke.svg }
    }
    Rectangle { anchors.fill: parent; color: "#151a20"; visible: !root.output }
    GpuGuideShape {
        id: grid
        anchors.fill: parent
        visible: !root.output && curveRendererAvailable
        Stroke { svg: root.gridPath; strokeWidth: 1; strokeColor: "#303943" }
    }
    GpuGuideShape {
        objectName: "levelsHistogramShape"
        anchors.fill: parent
        visible: !root.output && curveRendererAvailable
        ShapePath {
            strokeWidth: -1
            strokeColor: "transparent"
            fillColor: Qt.rgba(206/255,216/255,225/255,.42)
            PathSvg { path: root.histogramPath }
        }
    }
    GpuGuideShape {
        anchors.fill: parent
        visible: !root.output && curveRendererAvailable
        Stroke {
            svg: "M"+root.leftEdge+" "+root.topEdge+"L"+root.rightEdge+" "+root.topEdge
                +"L"+root.rightEdge+" "+root.bottomEdge+"L"+root.leftEdge+" "+root.bottomEdge+"Z"
            strokeWidth: 1; strokeColor: "#66727d"
        }
    }
    Rectangle {
        objectName: "levelsOutputGradient"
        visible: root.output
        x: root.leftEdge; y: 5
        width: root.rightEdge-root.leftEdge; height: 14
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#000000" }
            GradientStop { position: 1; color: "#ffffff" }
        }
    }
    Repeater {
        model: root.output ? 2 : 3
        delegate: GpuGuideShape {
            required property int index
            objectName: "levelsGpuMarker"+index
            readonly property real value: index===0 ? root.black
                : root.output || index===2 ? root.white
                : root.black+(root.white-root.black)*root.gammaT(root.gamma)
            x: root.px(value); y: root.output ? 21 : root.bottomEdge+2
            width: 14; height: root.output ? 14 : 13
            ShapePath {
                strokeWidth: 1
                strokeColor: "#dbe2e8"
                fillColor: index===0 ? "#07090b" : root.output || index===2 ? "#f4f5f6" : "#858b91"
                capStyle: ShapePath.FlatCap
                joinStyle: ShapePath.MiterJoin
                miterLimit: 10
                PathSvg { path: "M0 0L-7 "+(root.output?14:13)+"L7 "+(root.output?14:13)+"Z" }
            }
        }
    }
}
