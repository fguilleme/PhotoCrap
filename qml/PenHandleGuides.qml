import QtQuick
import QtQuick.Shapes

Item {
    id: root
    objectName: "penGpuHandles"
    required property var primitives
    readonly property bool curveRendererAvailable: capability.curveRendererAvailable
    visible: curveRendererAvailable
    // Resolve support even when there is no visible marker or tangent.
    GpuGuideShape {id:capability;visible:false}
    // Match the single Canvas composition before inherited opacity is applied.
    layer.enabled: true
    layer.smooth: true
    Repeater {
        model: root.primitives
        delegate: GpuGuideShape {
            required property var modelData
            required property int index
            objectName: "penGpuHandlePrimitive"+index
            width: root.width;height: root.height
            ShapePath {
                strokeWidth: 1
                capStyle: ShapePath.FlatCap
                joinStyle: ShapePath.MiterJoin
                miterLimit: 10
                strokeColor: modelData.stroke
                fillColor: modelData.fill
                PathSvg {path:modelData.svg}
            }
        }
    }
}
