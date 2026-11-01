import QtQuick

Canvas {
    id: root
    property var histogram: []
    property var points: []
    property var samples: []
    property int activePoint: -1
    property real leftEdge: 18
    property real rightEdge: width-10
    property real topEdge: 10
    property real bottomEdge: height-20
    function px(value) { return leftEdge+value/255*(rightEdge-leftEdge) }
    function py(value) { return bottomEdge-value/255*(bottomEdge-topEdge) }
    function valueX(pixel) { return Math.max(0,Math.min(255,(pixel-leftEdge)/(rightEdge-leftEdge)*255)) }
    function valueY(pixel) { return Math.max(0,Math.min(255,(bottomEdge-pixel)/(bottomEdge-topEdge)*255)) }
    readonly property var resolvedSamples: {
        if(samples && samples.length===256)return samples
        const identity=[]
        for(let i=0;i<256;++i)identity.push(i)
        return identity
    }
    function sample(index) { return resolvedSamples[index] }
    readonly property bool gpuBackend: GraphicsInfo.api===GraphicsInfo.Metal
        || GraphicsInfo.api===GraphicsInfo.Vulkan || GraphicsInfo.api===GraphicsInfo.OpenGL
        || GraphicsInfo.api===GraphicsInfo.Direct3D11 || GraphicsInfo.api===GraphicsInfo.Direct3D12
    readonly property bool integralSize: width===Math.round(width) && height===Math.round(height)
    readonly property bool gpuChart: nativeChart.item!==null && nativeChart.item.curveRendererAvailable
    readonly property bool rasterActive: context!==null && context!==undefined
    Loader {
        id: nativeChart
        anchors.fill: parent
        active: root.visible && root.gpuBackend && root.integralSize && width>0 && height>0
        onLoaded: if(!item.curveRendererAvailable)root.requestPaint()
        sourceComponent: CurvesGraph {
            histogram: root.histogram; points: root.points
            samples: root.resolvedSamples; activePoint: root.activePoint
        }
    }
    function invalidateRaster() { if(visible && !gpuChart)requestPaint() }
    onGpuChartChanged: if(gpuChart && rasterActive)requestPaint()
    onHistogramChanged: invalidateRaster()
    onPointsChanged: invalidateRaster()
    onResolvedSamplesChanged: invalidateRaster()
    onActivePointChanged: invalidateRaster()
    onVisibleChanged: if(visible)invalidateRaster()
    onPaint: {
        if(!visible || GraphicsInfo.api===GraphicsInfo.Unknown)return
        if(gpuBackend && integralSize && (!nativeChart.item || nativeChart.item.curveRendererAvailable)) {
            if(rasterActive) {
                const stale=getContext("2d")
                stale.reset();stale.clearRect(0,0,Math.ceil(width),Math.ceil(height))
            }
            return
        }
        const c=getContext("2d"); c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height)); c.fillStyle="#151a20"; c.fillRect(0,0,width,height)
        c.strokeStyle="#303943"; c.lineWidth=1
        for(let i=0;i<=4;i++) {
            const x=leftEdge+(rightEdge-leftEdge)*i/4, y=topEdge+(bottomEdge-topEdge)*i/4
            c.beginPath();c.moveTo(x,topEdge);c.lineTo(x,bottomEdge);c.stroke()
            c.beginPath();c.moveTo(leftEdge,y);c.lineTo(rightEdge,y);c.stroke()
        }
        if(root.histogram && root.histogram.length===256) {
            c.beginPath();c.moveTo(leftEdge,bottomEdge)
            for(let b=0;b<256;b++) c.lineTo(px(b),bottomEdge-Number(root.histogram[b])*(bottomEdge-topEdge)*.82)
            c.lineTo(rightEdge,bottomEdge);c.closePath();c.fillStyle="rgba(185,197,208,.25)";c.fill()
        }
        c.strokeStyle="#65717c";c.beginPath();c.moveTo(leftEdge,bottomEdge);c.lineTo(rightEdge,topEdge);c.stroke()
        c.strokeStyle="#e8edf1";c.lineWidth=2;c.beginPath();c.moveTo(px(0),py(sample(0)))
        for(let p=1;p<256;p++)c.lineTo(px(p),py(sample(p)))
        c.stroke()
        for(let j=0;j<root.points.length;j++) {
            const point=root.points[j], x=px(point.x), y=py(point.y)
            c.beginPath();c.arc(x,y,j===root.activePoint?6:5,0,Math.PI*2)
            c.fillStyle=j===root.activePoint?"#ffffff":"#222a32";c.fill()
            c.strokeStyle=j===root.activePoint?"#1d80e5":"#dce4ea";c.lineWidth=2;c.stroke()
        }
        c.strokeStyle="#66727d";c.lineWidth=1;c.strokeRect(leftEdge,topEdge,rightEdge-leftEdge,bottomEdge-topEdge)
    }
}
