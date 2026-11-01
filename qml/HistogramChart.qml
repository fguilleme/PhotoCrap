import QtQuick

Canvas {
    id: root
    required property var histogramData
    enabled: false
    readonly property bool gpuBackend: GraphicsInfo.api === GraphicsInfo.Metal
        || GraphicsInfo.api === GraphicsInfo.Vulkan || GraphicsInfo.api === GraphicsInfo.OpenGL
        || GraphicsInfo.api === GraphicsInfo.Direct3D11 || GraphicsInfo.api === GraphicsInfo.Direct3D12
    readonly property bool gpuChart: nativeChart.item !== null
        && nativeChart.item.curveRendererAvailable
    readonly property bool rasterActive: context !== null && context !== undefined
    Loader {
        id: nativeChart
        anchors.fill: parent
        active: root.visible && root.gpuBackend && width>0 && height>0
        onLoaded: if (!item.curveRendererAvailable) root.requestPaint()
        sourceComponent: HistogramGraph { histogramData: root.histogramData }
    }
    onPaint: {
        if (GraphicsInfo.api === GraphicsInfo.Unknown ||
            (gpuBackend && (!nativeChart.item || nativeChart.item.curveRendererAvailable))) return
        const context = getContext("2d")
        context.reset()
        context.fillStyle = "#14191f"
        context.fillRect(0, 0, width, height)
        const data = root.histogramData || ({})
        function channel(values, color) {
            if (!values || values.length !== 256) return
            context.beginPath()
            context.moveTo(0, height)
            for (let i = 0; i < 256; ++i)
                context.lineTo(i * width / 255, height - values[i] * (height - 4))
            context.lineTo(width, height)
            context.closePath()
            context.fillStyle = color
            context.fill()
        }
        channel(data.red, "rgba(245,63,69,.48)")
        channel(data.green, "rgba(55,220,95,.43)")
        channel(data.blue, "rgba(64,112,255,.48)")
        context.strokeStyle = "rgba(235,239,242,.7)"
        context.lineWidth = 1
        const luminance = data.luminance
        if (luminance && luminance.length === 256) {
            context.beginPath()
            for (let i = 0; i < 256; ++i) {
                const x = i * width / 255
                const y = height - luminance[i] * (height - 4)
                if (i === 0) context.moveTo(x, y); else context.lineTo(x, y)
            }
            context.stroke()
        }
    }
    onHistogramDataChanged: if (!gpuChart) requestPaint()
    onVisibleChanged: if (visible && !gpuChart) requestPaint()
    onGpuBackendChanged: if (!gpuBackend) requestPaint()
}
