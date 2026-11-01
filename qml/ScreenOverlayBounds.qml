import QtQuick

// Visible image-space bounds, shared by retained GPU overlays.
QtObject {
    required property var artboard
    required property var viewportItem
    property real zoom: 1
    readonly property rect bounds: {
        if(!artboard || !viewportItem)return Qt.rect(0,0,0,0)
        const scrollX=viewportItem.contentX,scrollY=viewportItem.contentY
        const scale=zoom,rotation=artboard.rotation,originX=artboard.x,originY=artboard.y
        const sourceWidth=artboard.width,sourceHeight=artboard.height
        const w=viewportItem.width,h=viewportItem.height
        const p=[viewportItem.mapToItem(artboard,0,0),viewportItem.mapToItem(artboard,w,0),
                 viewportItem.mapToItem(artboard,w,h),viewportItem.mapToItem(artboard,0,h)]
        const l=Math.max(0,Math.floor(Math.min.apply(null,p.map(p=>p.x))))
        const t=Math.max(0,Math.floor(Math.min.apply(null,p.map(p=>p.y))))
        const r=Math.min(artboard.width,Math.ceil(Math.max.apply(null,p.map(p=>p.x))))
        const b=Math.min(artboard.height,Math.ceil(Math.max.apply(null,p.map(p=>p.y))))
        return Qt.rect(l,t,Math.max(0,r-l),Math.max(0,b-t))
    }
}
