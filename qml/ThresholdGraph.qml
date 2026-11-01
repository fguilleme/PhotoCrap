import QtQuick
import QtQuick.Shapes

Item {
    id: root
    objectName: "thresholdGpuGraph"
    property var histogram: []
    property real level: 128
    readonly property bool curveRendererAvailable: grid.curveRendererAvailable
    visible: curveRendererAvailable
    enabled: false
    layer.enabled: true
    clip: true
    readonly property real leftEdge: 12
    readonly property real rightEdge: width-12
    readonly property real topEdge: 8
    readonly property real bottomEdge: height-30
    readonly property real split: leftEdge+level/255*(rightEdge-leftEdge)
    // Independent of level: a drag keeps both paths and their scene nodes.
    readonly property string histogramPath: {
        if (!histogram || histogram.length!==256) return ""
        let path="M"+leftEdge+" "+bottomEdge
        for (let b=0;b<256;++b)
            path+="L"+(leftEdge+b/255*(rightEdge-leftEdge))+" "
                +(bottomEdge-Number(histogram[b])*(bottomEdge-topEdge)*.92)
        return path+"L"+rightEdge+" "+bottomEdge+"Z"
    }
    readonly property string gridPath: {
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
    Rectangle { anchors.fill: parent; color: "#151a20" }
    GpuGuideShape {
        id: grid
        anchors.fill: parent
        Stroke { svg: root.gridPath; strokeWidth: 1; strokeColor: "#303943" }
    }
    GpuGuideShape {
        objectName: "thresholdHistogramShape"
        anchors.fill: parent
        ShapePath {
            strokeWidth: -1
            strokeColor: "transparent"
            fillColor: Qt.rgba(206/255,216/255,225/255,.48)
            PathSvg { path: root.histogramPath }
        }
    }
    Rectangle {
        x: root.leftEdge; y: root.bottomEdge+5
        width: root.split-root.leftEdge; height: 12
        color: "#050607"; antialiasing: true
    }
    Rectangle {
        x: root.split; y: root.bottomEdge+5
        width: root.rightEdge-root.split; height: 12
        color: "#f2f3f4"; antialiasing: true
    }
    Rectangle {
        objectName: "thresholdGpuMarker"
        x: root.split-1; y: root.topEdge
        width: 2; height: root.bottomEdge+19-root.topEdge
        color: "#70c995"; antialiasing: true
    }
    GpuGuideShape {
        x: root.split; y: root.bottomEdge+20
        width: 14; height: 8
        ShapePath {
            strokeWidth: -1
            strokeColor: "transparent"
            fillColor: "#70c995"
            PathSvg { path: "M0 0L-7 8L7 8Z" }
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
