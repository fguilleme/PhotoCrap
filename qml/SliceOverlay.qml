import QtQuick
import QtQuick.Window

Item {
    id: root
    required property var document
    required property var viewportItem
    property real zoom: 1
    signal painted()
    readonly property rect screenBounds: visibleArea.bounds
    ScreenOverlayBounds {
        id: visibleArea
        artboard: root.parent
        viewportItem: root.viewportItem
        zoom: root.zoom
    }
    x: screenBounds.x; y: screenBounds.y
    width: screenBounds.width; height: screenBounds.height
    enabled: false
    visible: sliceToolActive
    readonly property bool sliceToolActive: document.tool==="slice" || document.tool==="sliceSelect"
    readonly property bool sliceSelection: document.tool==="sliceSelect"
    antialiasing: true
    readonly property bool gpuBackend: GraphicsInfo.api === GraphicsInfo.Metal
        || GraphicsInfo.api === GraphicsInfo.Vulkan || GraphicsInfo.api === GraphicsInfo.OpenGL
        || GraphicsInfo.api === GraphicsInfo.Direct3D11 || GraphicsInfo.api === GraphicsInfo.Direct3D12
    // Fractional-DPR Canvas windows can resample their small label textures.
    readonly property bool gpuSliceEligible: gpuDraft && Screen.devicePixelRatio>0
        && Screen.devicePixelRatio===Math.round(Screen.devicePixelRatio)
        && visibleSlices.length<=128
    // A primitive revision suppresses shared slicesChanged notifications caused
    // only by the draft. The invokable snapshot has no notify dependency itself.
    readonly property double recordsRevision: document.sliceDescriptionRevision
    readonly property var sliceRecords: {
        const revision=recordsRevision
        return sliceToolActive ? document.sliceDescriptionSnapshot() : []
    }
    readonly property int selectedIndex: document.activeSlice
    readonly property rect draft: document.sliceDraft
    readonly property bool gpuSlices: nativeSlices.item !== null
        && nativeSlices.item.curveRendererAvailable
    readonly property bool rasterActive: softwareSlices.item!==null
        && softwareSlices.item.context!==null && softwareSlices.item.context!==undefined
    // Compose labels, handles and the draft before applying panel opacity.
    layer.enabled: gpuSlices || (gpuDraft && opacity<1 && draft.width>0 && draft.height>0)
    layer.smooth: true
    Loader {
        id: nativeSlices
        z: 1
        anchors.fill: parent
        active: root.visible && root.gpuSliceEligible && root.visibleSlices.length>0 && width>0 && height>0
        onLoaded: if(!item.curveRendererAvailable)root.requestPaint()
        sourceComponent: SliceGuides {
            slices: root.visibleSlices
            selectedIndex: root.selectedIndex
            selectionTool: root.sliceSelection
            zoom: root.zoom
            origin: Qt.point(root.x,root.y)
            pixelScale: Screen.devicePixelRatio
        }
    }
    readonly property bool gpuDraft: gpuBackend && nativeDraft.item !== null
        && nativeDraft.item.curveRendererAvailable
    Loader {
        id: nativeDraft
        z: 2
        anchors.fill: parent
        clip: true
        active: root.visible && root.gpuBackend && width>0 && height>0
        sourceComponent: SliceDraft {
            draft: root.draft
            zoom: root.zoom
            origin: Qt.point(root.x,root.y)
        }
    }
    readonly property var manualSlices: sliceRecords.filter(r => !r.automatic)
    readonly property var selectedSlice: selectedIndex>=0 && selectedIndex<sliceRecords.length
        ? sliceRecords[selectedIndex] : null
    // Qt 6.8 can notify var bindings even when they return the same array.
    // Assign only on real changes so the Repeater stays intact on every kit.
    readonly property var visibleCache: ({revision:-1})
    property var visibleSlices: []
    function refreshVisibleSlices() {
        if(!sliceRecords)return
        const view=screenBounds
        const next=sliceRecords.filter(function(r) {
            const x=r.x*zoom, y=r.y*zoom, w=r.width*zoom, h=r.height*zoom
            // Labels and handles can extend outside very small slice rectangles.
            return Math.max(x+w+5,x+48)>=view.x && x-5<=view.x+view.width
                && Math.max(y+h+5,y+24)>=view.y && y-5<=view.y+view.height
        })
        const previous=visibleSlices
        if(visibleCache.revision===recordsRevision && next.length===previous.length
            && next.every((r,i)=>r.index===previous[i].index))return
        visibleCache.revision=recordsRevision
        visibleSlices=next
    }
    function cursorAt(x,y) {
        if(!sliceSelection) return Qt.CrossCursor
        const slice=selectedSlice
        if(slice) {
            const l=slice.x*zoom,t=slice.y*zoom,r=l+slice.width*zoom,b=t+slice.height*zoom
            const handles=[{x:l,y:t},{x:r,y:t},{x:r,y:b},{x:l,y:b},
                           {x:(l+r)/2,y:t},{x:r,y:(t+b)/2},{x:(l+r)/2,y:b},{x:l,y:(t+b)/2}]
            for(let i=0;i<8;i++) if(Math.hypot(x-handles[i].x,y-handles[i].y)<=10)
                return i<4 ? (i%2===0 ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor)
                           : (i%2===0 ? Qt.SizeVerCursor : Qt.SizeHorCursor)
        }
        for(const s of manualSlices)
            if(x>=s.x*zoom && y>=s.y*zoom && x<=(s.x+s.width)*zoom && y<=(s.y+s.height)*zoom)
                return Qt.SizeAllCursor
        return Qt.ArrowCursor
    }
    function requestPaint() {
        if(visible && softwareSlices.item)softwareSlices.item.requestPaint()
    }
    Loader {
        id: softwareSlices
        anchors.fill: parent
        active: root.visible && width>0 && height>0
            && (root.visibleSlices.length>0 || (!root.gpuDraft && root.draft.width>0 && root.draft.height>0))
            && (!root.gpuBackend || (nativeDraft.item && !root.gpuSliceEligible))
        sourceComponent: Canvas {
            objectName: "sliceRasterGuides"
            antialiasing: true
            Component.onCompleted: requestPaint()
            onPainted: root.painted()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
                for(const r of visibleSlices) {
                    const selected=r.index===selectedIndex && !r.automatic
                    const x=r.x*zoom-root.x,y=r.y*zoom-root.y,w=r.width*zoom,h=r.height*zoom
                    c.lineWidth=1;c.strokeStyle=r.automatic ? "#adb3bc" : selected ? "#ffb664" : "#67c5ff"
                    c.strokeRect(x+.5,y+.5,w-1,h-1)
                    c.fillStyle=r.automatic ? "#515964" : selected ? "#bd6c16" : "#256797"
                    c.fillRect(x+1,y+1,26,18);c.fillStyle="#ffffff";c.font="11px sans-serif"
                    c.fillText(String(r.index+1).padStart(2,"0"),x+5,y+14)
                    if(selected && sliceSelection) {
                        for(const p of [{x:x,y:y},{x:x+w,y:y},{x:x+w,y:y+h},{x:x,y:y+h},
                                        {x:x+w/2,y:y},{x:x+w,y:y+h/2},{x:x+w/2,y:y+h},{x:x,y:y+h/2}]) {
                            c.fillStyle="#ffb664";c.fillRect(p.x-3,p.y-3,6,6)
                            c.strokeStyle="#101419";c.strokeRect(p.x-3,p.y-3,6,6)
                        }
                    }
                }
                if(!gpuDraft && draft.width>0 && draft.height>0) {
                    c.strokeStyle="#ffffff";c.lineWidth=2
                    c.strokeRect(draft.x*zoom-root.x,draft.y*zoom-root.y,draft.width*zoom,draft.height*zoom)
                }
            }
        }
    }
    Component.onCompleted: refreshVisibleSlices()
    onScreenBoundsChanged: refreshVisibleSlices()
    onZoomChanged: {refreshVisibleSlices();if(visible && !gpuSlices)requestPaint()}
    onVisibleChanged:if(visible && !gpuSlices)requestPaint()
    onSliceSelectionChanged:if(visible && !gpuSlices)requestPaint()
    onSliceRecordsChanged: {refreshVisibleSlices();if(visible && !gpuSlices)requestPaint()}
    onSelectedIndexChanged: if(visible && !gpuSlices)requestPaint()
    onDraftChanged: if(visible && !gpuDraft && !gpuSlices)requestPaint()
    onGpuDraftChanged: if(visible && !gpuSlices)requestPaint()
    onGpuSlicesChanged: if(visible)requestPaint()
    onXChanged: requestPaint()
    onYChanged: requestPaint()
}
