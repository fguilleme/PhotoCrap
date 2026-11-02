import QtQuick
import QtQuick.Shapes

// Preserve per-slice drawing order, including labels and overlapping handles.
Item {
    id: root
    required property var slices
    required property int selectedIndex
    required property bool selectionTool
    required property real zoom
    required property point origin
    required property real pixelScale
    readonly property bool curveRendererAvailable: capability.curveRendererAvailable
    visible: curveRendererAvailable
    enabled: false
    clip: true
    GpuGuideShape { id: capability }

    Repeater {
        model: root.slices
        delegate: Item {
            id: slice
            objectName: "sliceGpuRecord"
            required property var modelData
            readonly property bool selected: modelData.index===root.selectedIndex && !modelData.automatic
            readonly property real sliceWidth: modelData.width*root.zoom
            readonly property real sliceHeight: modelData.height*root.zoom
            readonly property real imageX: modelData.x*root.zoom
            readonly property real imageY: modelData.y*root.zoom
            x: imageX-root.origin.x
            y: imageY-root.origin.y
            width: Math.max(48,sliceWidth+5)
            height: Math.max(24,sliceHeight+5)

            GpuGuideShape {
                objectName: "sliceGpuFrame"
                anchors.fill: parent
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: slice.modelData.automatic ? "#adb3bc" : slice.selected ? "#ffb664" : "#67c5ff"
                    strokeWidth: 1
                    joinStyle: ShapePath.MiterJoin
                    miterLimit: 10
                    PathSvg { path: "M.5 .5h"+(slice.sliceWidth-1)+"v"+(slice.sliceHeight-1)+"h"+(1-slice.sliceWidth)+"Z" }
                }
            }
            Rectangle {
                x: 1; y: 1; width: 26; height: 18
                color: slice.modelData.automatic ? "#515964" : slice.selected ? "#bd6c16" : "#256797"
                antialiasing: true
            }
            Canvas {
                id: label
                objectName: "sliceCachedLabel"
                // Canvas fillText uses unhinted outlines, unlike Text's native
                // glyph renderer. Cache that tiny raster at the same pixel phase.
                // The overlay origin is integral; use absolute image positions
                // so panning cannot perturb the fraction through cancellation.
                readonly property real phaseX: slice.imageX-Math.floor(slice.imageX*root.pixelScale)/root.pixelScale
                readonly property real phaseY: slice.imageY-Math.floor(slice.imageY*root.pixelScale)/root.pixelScale
                readonly property string number: String(slice.modelData.index+1).padStart(2,"0")
                x: -phaseX; y: -phaseY; width: 49; height: 25
                antialiasing: true
                onPhaseXChanged: requestPaint()
                onPhaseYChanged: requestPaint()
                onNumberChanged: requestPaint()
                Component.onCompleted: requestPaint()
                Connections {
                    target: root
                    function onPixelScaleChanged(){label.requestPaint()}
                }
                onPaint: {
                    const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
                    c.font="11px sans-serif";c.fillStyle="white"
                    c.fillText(number,5+phaseX,14+phaseY)
                }
            }
            Repeater {
                model: slice.selected && root.selectionTool ? 8 : 0
                delegate: GpuGuideShape {
                    required property int index
                    readonly property var positions: [Qt.point(0,0),Qt.point(slice.sliceWidth,0),
                        Qt.point(slice.sliceWidth,slice.sliceHeight),Qt.point(0,slice.sliceHeight),
                        Qt.point(slice.sliceWidth/2,0),Qt.point(slice.sliceWidth,slice.sliceHeight/2),
                        Qt.point(slice.sliceWidth/2,slice.sliceHeight),Qt.point(0,slice.sliceHeight/2)]
                    objectName: "sliceGpuHandle"
                    x: positions[index].x-3
                    y: positions[index].y-3
                    width: 6; height: 6
                    ShapePath {
                        fillColor: "#ffb664"
                        strokeColor: "#101419"
                        strokeWidth: 1
                        joinStyle: ShapePath.MiterJoin
                        miterLimit: 10
                        PathSvg { path: "M0 0h6v6h-6Z" }
                    }
                }
            }
        }
    }
}
