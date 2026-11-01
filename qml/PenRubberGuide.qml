import QtQuick
import QtQuick.Shapes

Item {
    id: root
    objectName: "penGpuRubberBand"
    required property string svg
    readonly property bool curveRendererAvailable: stroke.curveRendererAvailable
    visible: curveRendererAvailable
    layer.enabled: true
    layer.smooth: true
    GpuGuideShape {
        id: stroke
        objectName: "penGpuRubberStroke"
        anchors.fill: parent
        ShapePath {
            strokeWidth: -1
            strokeColor: "transparent"
            fillColor: "#73c5ff"
            fillRule: ShapePath.WindingFill
            PathSvg {path:root.svg}
        }
    }
}
