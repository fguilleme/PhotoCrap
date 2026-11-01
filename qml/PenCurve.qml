import QtQuick
import QtQuick.Shapes

// Retained quadratic segments prepared from the native cubics; the parent
// loader clips the strokes to their visible envelope.
GpuGuideShape {
    id: root
    objectName: "penGpuCurve"
    required property string svg
    enabled: false
    component Stroke: ShapePath {
        fillColor: "transparent"
        capStyle: ShapePath.FlatCap
        joinStyle: ShapePath.MiterJoin
        miterLimit: 10
        PathSvg { path: root.svg }
    }
    Stroke { strokeColor: "#1a2b3b"; strokeWidth: 3 }
    Stroke { strokeColor: "#73c5ff"; strokeWidth: 1.5 }
}
