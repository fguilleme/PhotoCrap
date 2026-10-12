import QtQuick

ScreenOverlayCanvas {
    id: root
    required property var document
    enabled: false
    visible: document.tool==="slice" || document.tool==="sliceSelect"
    antialiasing: true
    function cursorAt(x,y) {
        if(document.tool!=="sliceSelect") return Qt.CrossCursor
        const slice=document.activeSlice>=0 ? document.slices[document.activeSlice] : null
        if(slice) {
            const l=slice.x*zoom,t=slice.y*zoom,r=l+slice.width*zoom,b=t+slice.height*zoom
            const handles=[{x:l,y:t},{x:r,y:t},{x:r,y:b},{x:l,y:b},
                           {x:(l+r)/2,y:t},{x:r,y:(t+b)/2},{x:(l+r)/2,y:b},{x:l,y:(t+b)/2}]
            for(let i=0;i<8;i++) if(Math.hypot(x-handles[i].x,y-handles[i].y)<=10)
                return i<4 ? (i%2===0 ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor)
                           : (i%2===0 ? Qt.SizeVerCursor : Qt.SizeHorCursor)
        }
        for(const s of document.slices)
            if(!s.automatic && x>=s.x*zoom && y>=s.y*zoom && x<=(s.x+s.width)*zoom && y<=(s.y+s.height)*zoom)
                return Qt.SizeAllCursor
        return Qt.ArrowCursor
    }
    onPaint: {
        const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
        for(const r of document.slices) {
            const selected=r.index===document.activeSlice && !r.automatic
            const x=r.x*zoom-root.x,y=r.y*zoom-root.y,w=r.width*zoom,h=r.height*zoom
            c.lineWidth=1;c.strokeStyle=r.automatic ? "#adb3bc" : selected ? "#ffb664" : "#67c5ff"
            c.strokeRect(x+.5,y+.5,w-1,h-1)
            c.fillStyle=r.automatic ? "#515964" : selected ? "#bd6c16" : "#256797"
            c.fillRect(x+1,y+1,26,18);c.fillStyle="#ffffff";c.font="11px sans-serif"
            c.fillText(String(r.index+1).padStart(2,"0"),x+5,y+14)
            if(selected && document.tool==="sliceSelect") {
                for(const p of [{x:x,y:y},{x:x+w,y:y},{x:x+w,y:y+h},{x:x,y:y+h},
                                {x:x+w/2,y:y},{x:x+w,y:y+h/2},{x:x+w/2,y:y+h},{x:x,y:y+h/2}]) {
                    c.fillStyle="#ffb664";c.fillRect(p.x-3,p.y-3,6,6)
                    c.strokeStyle="#101419";c.strokeRect(p.x-3,p.y-3,6,6)
                }
            }
        }
        const draft=document.sliceDraft
        if(draft.width>0 && draft.height>0) {
            c.strokeStyle="#ffffff";c.lineWidth=2
            c.strokeRect(draft.x*zoom-root.x,draft.y*zoom-root.y,draft.width*zoom,draft.height*zoom)
        }
    }
    onZoomChanged:requestPaint()
    onVisibleChanged:requestPaint()
    Connections { target:root.document;function onSlicesChanged(){root.requestPaint()} function onSettingsChanged(){root.requestPaint()} }
}
