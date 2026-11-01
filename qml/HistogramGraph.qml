import QtQuick
import QtQuick.Shapes
import PhotoCrap.Native 1.0

Item {
    id: root
    objectName: "histogramGpuChart"
    required property var histogramData
    readonly property bool curveRendererAvailable: plot.curveRendererAvailable
    visible: curveRendererAvailable
    enabled: false
    function curve(values, filled) {
        if (!values || values.length!==256 || width<=0 || height<=0) return ""
        let path=filled ? "M0 "+height : ""
        for (let i=0; i<256; ++i) {
            const x=i*width/255, y=height-values[i]*(height-4)
            path+=(i===0 && !filled ? "M" : "L")+x+" "+y
        }
        return filled ? path+"L"+width+" "+height+"Z" : path
    }
    readonly property var channels: histogramData || ({})
    readonly property string redPath: curve(channels.red,true)
    readonly property string greenPath: curve(channels.green,true)
    readonly property string bluePath: curve(channels.blue,true)
    readonly property string luminancePath: HistogramGeometry.luminanceStroke(
        channels.luminance || [],width,height)
    // Canvas rasterizes into a rounded logical pixel window, then maps that
    // window to the fractional item size. Preserve the same projection on GPU.
    Item {
        id: surface
        width: Math.max(1,Math.round(root.width))
        height: Math.max(1,Math.round(root.height))
        transform: Scale {
            xScale: root.width/surface.width
            yScale: root.height/surface.height
        }
        // Compose the translucent channels before applying inherited opacity.
        layer.enabled: true
        layer.smooth: true
        clip: true
        Rectangle { width: root.width; height: root.height; color: "#14191f" }
        GpuGuideShape {
            id: plot
            objectName: "histogramCurveShape"
            anchors.fill: parent
            ShapePath {
                strokeWidth: -1
                strokeColor: "transparent"
                fillColor: Qt.rgba(245/255,63/255,69/255,.48)
                PathSvg { path: root.redPath }
            }
            ShapePath {
                strokeWidth: -1
                strokeColor: "transparent"
                fillColor: Qt.rgba(55/255,220/255,95/255,.43)
                PathSvg { path: root.greenPath }
            }
            ShapePath {
                strokeWidth: -1
                strokeColor: "transparent"
                fillColor: Qt.rgba(64/255,112/255,1,.48)
                PathSvg { path: root.bluePath }
            }
            ShapePath {
                strokeWidth: -1
                strokeColor: "transparent"
                fillColor: Qt.rgba(235/255,239/255,242/255,.7)
                fillRule: ShapePath.WindingFill
                PathSvg { path: root.luminancePath }
            }
        }
    }
}
