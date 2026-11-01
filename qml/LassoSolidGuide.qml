import QtQuick
import QtQuick.Shapes

// A single open stroke preserves joins with the provisional segment. Marker
// paths stay independent of pointer motion and only their nodes move on screen.
Item {
    id: root
    objectName: "lassoGpuGuide"
    required property string fixedPath
    required property point origin
    required property point tail
    required property bool tailVisible
    required property var markers
    required property real zoom
    required property point imageOrigin
    readonly property bool curveRendererAvailable: stroke.curveRendererAvailable
    visible: curveRendererAvailable
    // Composite stroke and markers before inherited opacity, as the Canvas did.
    layer.enabled: true
    layer.smooth: true
    GpuGuideShape {
        id: stroke
        objectName: "lassoSolidStroke"
        x: root.origin.x; y: root.origin.y
        width: root.width; height: root.height
        ShapePath {
            strokeWidth: 1
            strokeColor: "#f5f7f9"
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            joinStyle: ShapePath.MiterJoin
            miterLimit: 10
            PathSvg {
                path: root.fixedPath+(root.tailVisible ? "L"+root.tail.x+" "+root.tail.y : "")
            }
        }
    }
    Repeater {
        model: root.markers
        delegate: GpuGuideShape {
            required property var modelData
            objectName: "lassoSolidMarker"
            x: modelData.x*root.zoom-root.imageOrigin.x-4
            y: modelData.y*root.zoom-root.imageOrigin.y-4
            width: 8; height: 8
            ShapePath {
                strokeWidth: 1
                strokeColor: "#172028"
                fillColor: "#f5f7f9"
                PathSvg {path:"M7 4A3 3 0 1 0 1 4A3 3 0 1 0 7 4Z"}
            }
        }
    }
}
