import QtQuick
import QtQuick.Shapes

// Only the moving white frame changes; the numbered slice layer stays cached.
GpuGuideShape {
    id: root
    objectName: "sliceGpuDraft"
    required property rect draft
    property real zoom: 1
    property point origin: Qt.point(0,0)
    enabled: false
    ShapePath {
        fillColor: "transparent"
        strokeColor: "white"
        strokeWidth: 2
        capStyle: ShapePath.FlatCap
        joinStyle: ShapePath.MiterJoin
        miterLimit: 10
        PathSvg {
            path: {
                if (root.draft.width<=0 || root.draft.height<=0) return ""
                const x=root.draft.x*root.zoom-root.origin.x
                const y=root.draft.y*root.zoom-root.origin.y
                const w=root.draft.width*root.zoom,h=root.draft.height*root.zoom
                return "M"+x+" "+y+"h"+w+"v"+h+"h"+(-w)+"Z"
            }
        }
    }
}
