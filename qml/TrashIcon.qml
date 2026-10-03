import QtQuick

Item {
    id: root

    property color iconColor: "#c0c7d0"

    implicitWidth: 20
    implicitHeight: 20

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.iconColor
            ctx.lineWidth = 1.55
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            ctx.beginPath()
            ctx.moveTo(4, 5.5)
            ctx.lineTo(16, 5.5)
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(7.4, 5.2)
            ctx.lineTo(7.9, 3.5)
            ctx.lineTo(12.1, 3.5)
            ctx.lineTo(12.6, 5.2)
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(5.4, 7.5)
            ctx.lineTo(6.2, 16.2)
            ctx.quadraticCurveTo(6.3, 17.2, 7.4, 17.2)
            ctx.lineTo(12.6, 17.2)
            ctx.quadraticCurveTo(13.7, 17.2, 13.8, 16.2)
            ctx.lineTo(14.6, 7.5)
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(8.4, 8.5)
            ctx.lineTo(8.7, 15)
            ctx.moveTo(11.6, 8.5)
            ctx.lineTo(11.3, 15)
            ctx.stroke()
        }
        Connections {
            target: root
            function onIconColorChanged() { canvas.requestPaint() }
        }
    }
}
