import QtQuick

// Presentation only. Mouse capture and preview/commit signals stay in LevelsEditor.
Canvas {
    id: root
    property bool output: false
    property var histogram: []
    property real black: 0
    property real white: 255
    property real gamma: 1
    readonly property real leftEdge: 14
    readonly property real rightEdge: width-14
    readonly property real topEdge: 8
    readonly property real bottomEdge: height-28
    function px(value) { return leftEdge+value/255*(rightEdge-leftEdge) }
    function valueX(pixel) { return Math.max(0,Math.min(255,(pixel-leftEdge)/(rightEdge-leftEdge)*255)) }
    function gammaT(value) { return value>=1?.5-(value-1)/4:.5+(1-value)/1.8 }
    function gammaValue(t) { return t<=.5?1+(.5-t)*4:1-(t-.5)*1.8 }
    function gammaX() { return px(black+(white-black)*gammaT(gamma)) }
    function triangle(c,x,color) { c.beginPath();c.moveTo(x,bottomEdge+2);c.lineTo(x-7,bottomEdge+15);c.lineTo(x+7,bottomEdge+15);c.closePath();c.fillStyle=color;c.fill();c.strokeStyle="#dbe2e8";c.stroke() }
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
        sourceComponent: LevelsGraph {
            output: root.output; histogram: root.histogram
            black: root.black; white: root.white; gamma: root.gamma
        }
    }
    function invalidateRaster() { if(visible && !gpuChart)requestPaint() }
    // Output has no opaque background: discard the old raster marks when
    // returning to GPU, including when rounded backing dimensions stay equal.
    onGpuChartChanged: if(gpuChart && rasterActive)requestPaint()
    onHistogramChanged: invalidateRaster()
    onBlackChanged: invalidateRaster()
    onWhiteChanged: invalidateRaster()
    onGammaChanged: invalidateRaster()
    onOutputChanged: invalidateRaster()
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
        if(output) {
            const c=getContext("2d");c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height));const gradient=c.createLinearGradient(leftEdge,0,rightEdge,0);gradient.addColorStop(0,"#000");gradient.addColorStop(1,"#fff");c.fillStyle=gradient;c.fillRect(leftEdge,5,rightEdge-leftEdge,14)
            function marker(x,color){c.beginPath();c.moveTo(x,21);c.lineTo(x-7,35);c.lineTo(x+7,35);c.closePath();c.fillStyle=color;c.fill();c.strokeStyle="#dbe2e8";c.stroke()}
            marker(px(root.black),"#07090b");marker(px(root.white),"#f4f5f6")
        } else {
            const c=getContext("2d");c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height));c.fillStyle="#151a20";c.fillRect(0,0,width,height)
            c.strokeStyle="#303943";for(let i=0;i<=4;i++){const x=leftEdge+(rightEdge-leftEdge)*i/4;c.beginPath();c.moveTo(x,topEdge);c.lineTo(x,bottomEdge);c.stroke()}
            if(root.histogram && root.histogram.length===256){c.beginPath();c.moveTo(leftEdge,bottomEdge);for(let b=0;b<256;b++)c.lineTo(px(b),bottomEdge-Number(root.histogram[b])*(bottomEdge-topEdge)*.92);c.lineTo(rightEdge,bottomEdge);c.closePath();c.fillStyle="rgba(206,216,225,.42)";c.fill()}
            c.strokeStyle="#66727d";c.strokeRect(leftEdge,topEdge,rightEdge-leftEdge,bottomEdge-topEdge)
            triangle(c,px(root.black),"#07090b");triangle(c,gammaX(),"#858b91");triangle(c,px(root.white),"#f4f5f6")
        }
    }
}
