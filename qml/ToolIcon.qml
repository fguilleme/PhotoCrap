import QtQuick

Item {
    id: root
    property string toolName: ""
    property string fallback: ""
    property color iconColor: "#c0c7d0"
    property int fallbackPixelSize: 19
    readonly property bool supported: ["blur", "sharpen", "dodge", "burn", "sponge"].indexOf(toolName) >= 0

    implicitWidth: 24
    implicitHeight: 24

    onToolNameChanged: iconCanvas.requestPaint()
    onIconColorChanged: iconCanvas.requestPaint()
    onWidthChanged: iconCanvas.requestPaint()
    onHeightChanged: iconCanvas.requestPaint()

    Text {
        anchors.centerIn: parent
        visible: !root.supported
        text: root.fallback
        color: root.iconColor
        font.pixelSize: root.fallbackPixelSize
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Canvas {
        id: iconCanvas
        anchors.fill: parent
        visible: root.supported
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.supported || width <= 0 || height <= 0)
                return
            const scale = Math.min(width, height) / 24
            const ox = (width - 24 * scale) / 2
            const oy = (height - 24 * scale) / 2
            ctx.save()
            ctx.translate(ox, oy)
            ctx.scale(scale, scale)
            ctx.strokeStyle = root.iconColor
            ctx.fillStyle = root.iconColor
            ctx.lineWidth = 1.7
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (root.toolName === "blur") {
                ctx.beginPath()
                ctx.moveTo(12, 2.5)
                ctx.bezierCurveTo(10.4, 5.7, 6.2, 9.7, 6.2, 14.1)
                ctx.bezierCurveTo(6.2, 17.9, 8.8, 20.7, 12, 20.7)
                ctx.bezierCurveTo(15.7, 20.7, 18.2, 18, 18.2, 14.2)
                ctx.bezierCurveTo(18.2, 10.1, 13.8, 5.5, 12, 2.5)
                ctx.fill()
            } else if (root.toolName === "sharpen") {
                ctx.beginPath()
                ctx.moveTo(12, 3)
                ctx.lineTo(20, 20)
                ctx.lineTo(4, 20)
                ctx.closePath()
                ctx.fill()
            } else if (root.toolName === "dodge") {
                ctx.beginPath()
                ctx.arc(9.1, 9.1, 5.6, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(13.1, 13.1)
                ctx.lineTo(20.3, 20.3)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(7.4, 7.2, 1.2, 0, Math.PI * 2)
                ctx.fill()
            } else if (root.toolName === "burn") {
                ctx.beginPath()
                ctx.moveTo(4, 14.2)
                ctx.bezierCurveTo(5.4, 11.7, 7.1, 10.7, 9.3, 10.7)
                ctx.lineTo(9.3, 5.2)
                ctx.bezierCurveTo(9.3, 4.1, 10.8, 4.1, 10.8, 5.2)
                ctx.lineTo(10.8, 9.2)
                ctx.lineTo(11.7, 4.2)
                ctx.bezierCurveTo(11.9, 3.2, 13.4, 3.4, 13.2, 4.5)
                ctx.lineTo(12.6, 9.2)
                ctx.lineTo(14.1, 5.1)
                ctx.bezierCurveTo(14.5, 4.1, 15.9, 4.6, 15.5, 5.6)
                ctx.lineTo(14.1, 10.2)
                ctx.bezierCurveTo(17, 9.4, 19.2, 10.8, 18.3, 12.4)
                ctx.bezierCurveTo(17.4, 13.8, 16.4, 15.2, 16, 17.8)
                ctx.lineTo(7.4, 19.8)
                ctx.bezierCurveTo(7.1, 17.4, 5.9, 16.1, 4, 14.2)
                ctx.closePath()
                ctx.stroke()
            } else if (root.toolName === "sponge") {
                ctx.beginPath()
                ctx.moveTo(5, 7)
                ctx.bezierCurveTo(7, 3.9, 10.1, 4.5, 12.2, 3.6)
                ctx.bezierCurveTo(15.1, 2.5, 19.3, 5.4, 18.6, 8.5)
                ctx.bezierCurveTo(21.2, 11.1, 19.2, 15.3, 17.1, 16.3)
                ctx.bezierCurveTo(16.1, 20.3, 10.7, 20.8, 8.6, 18.5)
                ctx.bezierCurveTo(4.8, 18.8, 2.8, 14.5, 4.7, 11.9)
                ctx.bezierCurveTo(3.1, 10.3, 3.6, 8.2, 5, 7)
                ctx.closePath()
                ctx.stroke()
                const pores = [[8, 8, 1.1], [14, 7, 1.4], [11, 12, 1.5], [16, 13, 1], [7.5, 15, .9]]
                for (let index = 0; index < pores.length; ++index) {
                    const pore = pores[index]
                    ctx.beginPath()
                    ctx.arc(pore[0], pore[1], pore[2], 0, Math.PI * 2)
                    ctx.fill()
                }
            }
            ctx.restore()
        }
    }
}
