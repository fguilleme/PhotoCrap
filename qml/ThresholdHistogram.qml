import QtQuick

// Keep input handling in the editor; this item only draws the histogram.
Canvas {
    id: root
    property var histogram: []
    property real level: 128
    property real leftEdge: 12
    property real rightEdge: width - 12
    property real topEdge: 8
    property real bottomEdge: height - 30

    function px(value) {
        return leftEdge + value / 255 * (rightEdge - leftEdge)
    }
    function valueAt(pixel) {
        return Math.round(Math.max(0, Math.min(255,
            (pixel - leftEdge) / (rightEdge - leftEdge) * 255)))
    }

    readonly property bool gpuBackend: GraphicsInfo.api === GraphicsInfo.Metal
        || GraphicsInfo.api === GraphicsInfo.Vulkan || GraphicsInfo.api === GraphicsInfo.OpenGL
        || GraphicsInfo.api === GraphicsInfo.Direct3D11 || GraphicsInfo.api === GraphicsInfo.Direct3D12
    // Fractional Canvas textures have a different projection; retain that sampling.
    readonly property bool integralSize: width===Math.round(width) && height===Math.round(height)
    readonly property bool gpuChart: nativeChart.item !== null && nativeChart.item.curveRendererAvailable
    readonly property bool rasterActive: context !== null && context !== undefined
    Loader {
        id: nativeChart
        anchors.fill: parent
        active: root.visible && root.gpuBackend && root.integralSize && width>0 && height>0
        onLoaded: if (!item.curveRendererAvailable) root.requestPaint()
        sourceComponent: ThresholdGraph { histogram: root.histogram; level: root.level }
    }
    onHistogramChanged: if (!gpuChart) requestPaint()
    onLevelChanged: if (!gpuChart) requestPaint()
    onVisibleChanged: if (visible && !gpuChart) requestPaint()
    onPaint: {
        if (GraphicsInfo.api===GraphicsInfo.Unknown ||
            (gpuBackend && integralSize && (!nativeChart.item || nativeChart.item.curveRendererAvailable))) return
        const c = getContext("2d")
        c.reset(); c.clearRect(0,0,Math.ceil(width),Math.ceil(height))
        c.fillStyle = "#151a20"
        c.fillRect(0, 0, width, height)
        c.strokeStyle = "#303943"
        for (let i = 0; i <= 4; ++i) {
            const x = leftEdge + (rightEdge - leftEdge) * i / 4
            c.beginPath(); c.moveTo(x, topEdge); c.lineTo(x, bottomEdge); c.stroke()
        }
        if (root.histogram && root.histogram.length === 256) {
            c.beginPath(); c.moveTo(leftEdge, bottomEdge)
            for (let bin = 0; bin < 256; ++bin)
                c.lineTo(px(bin), bottomEdge - Number(root.histogram[bin]) *
                         (bottomEdge - topEdge) * .92)
            c.lineTo(rightEdge, bottomEdge); c.closePath()
            c.fillStyle = "rgba(206,216,225,.48)"; c.fill()
        }
        const split = px(root.level)
        c.fillStyle = "#050607"; c.fillRect(leftEdge, bottomEdge + 5, split - leftEdge, 12)
        c.fillStyle = "#f2f3f4"; c.fillRect(split, bottomEdge + 5, rightEdge - split, 12)
        c.strokeStyle = "#70c995"; c.lineWidth = 2
        c.beginPath(); c.moveTo(split, topEdge); c.lineTo(split, bottomEdge + 19); c.stroke()
        c.beginPath(); c.moveTo(split, bottomEdge + 20)
        c.lineTo(split - 7, bottomEdge + 28); c.lineTo(split + 7, bottomEdge + 28)
        c.closePath(); c.fillStyle = "#70c995"; c.fill()
        c.lineWidth = 1; c.strokeStyle = "#66727d"
        c.strokeRect(leftEdge, topEdge, rightEdge - leftEdge, bottomEdge - topEdge)
    }
}
