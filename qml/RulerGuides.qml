import QtQuick
import QtQuick.Shapes

Item {
    id: root
    objectName: "rulerGpuGuides"
    required property point startPoint
    required property point endPoint
    readonly property bool curveRendererAvailable: stroke.curveRendererAvailable
    enabled: false
    visible: curveRendererAvailable
    GpuGuideShape {
        id: stroke
        objectName: "rulerGuideShape"
        anchors.fill: parent
        ShapePath {
            strokeColor: Qt.rgba(0,0,0,.85)
            strokeWidth: 4
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            startX: root.startPoint.x; startY: root.startPoint.y
            PathLine {x:root.endPoint.x;y:root.endPoint.y}
        }
        ShapePath {
            strokeColor: "#f5f7f9"
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            startX: root.startPoint.x; startY: root.startPoint.y
            PathLine {x:root.endPoint.x;y:root.endPoint.y}
        }
    }
    component Handle: ShapePath {
        id: handle
        required property point center
        fillColor: "#f5f7f9"
        strokeColor: "#101419"
        strokeWidth: 1
        PathSvg {
            path: "M"+(handle.center.x+4)+" "+handle.center.y
                +"A4 4 0 1 0 "+(handle.center.x-4)+" "+handle.center.y
                +"A4 4 0 1 0 "+(handle.center.x+4)+" "+handle.center.y+"Z"
        }
    }
    GpuGuideShape {
        objectName: "rulerHandleShape"
        anchors.fill: parent
        Handle {center:root.startPoint}
        Handle {center:root.endPoint}
    }
}
