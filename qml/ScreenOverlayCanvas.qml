import QtQuick

// Allocate only the visible part of a zoomed artboard, including after view rotation.
Canvas {
    id: root
    required property var viewportItem
    property real zoom: 1
    readonly property rect screenBounds: {
        if(!parent || !viewportItem) return Qt.rect(0,0,0,0)
        // These dependencies track the Flickable's content transform as well as its size.
        const scrollX=viewportItem.contentX, scrollY=viewportItem.contentY
        const scale=zoom, rotation=parent.rotation
        const w=viewportItem.width,h=viewportItem.height
        const points=[viewportItem.mapToItem(parent,0,0),viewportItem.mapToItem(parent,w,0),
                      viewportItem.mapToItem(parent,w,h),viewportItem.mapToItem(parent,0,h)]
        const left=Math.max(0,Math.floor(Math.min.apply(null,points.map(p=>p.x))))
        const top=Math.max(0,Math.floor(Math.min.apply(null,points.map(p=>p.y))))
        const right=Math.min(parent.width,Math.ceil(Math.max.apply(null,points.map(p=>p.x))))
        const bottom=Math.min(parent.height,Math.ceil(Math.max.apply(null,points.map(p=>p.y))))
        return Qt.rect(left,top,Math.max(0,right-left),Math.max(0,bottom-top))
    }
    x: screenBounds.x; y: screenBounds.y
    width: screenBounds.width; height: screenBounds.height
    onXChanged: requestPaint()
    onYChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
}
