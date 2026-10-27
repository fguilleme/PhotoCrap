import QtQuick

Item {
    id: root
    property string toolName: ""
    property string fallback: ""
    property color iconColor: "#c0c7d0"
    property int fallbackPixelSize: 19
    readonly property bool supported: ["pen", "addAnchor", "deleteAnchor", "convertPoint", "magicEraser", "crop", "perspectiveCrop", "slice", "sliceSelect", "picker", "colorSampler", "ruler", "artHistoryBrush", "historyBrush", "remove", "contentAwareMove", "patch", "spotHealing", "healing", "redEye", "mixerBrush", "colorReplacement", "smudge", "cloneStamp", "patternStamp", "lasso", "polygonalLasso", "quickSelection", "magicWand", "blur", "sharpen", "dodge", "burn", "sponge"].indexOf(toolName) >= 0

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

            if (["pen","addAnchor","deleteAnchor","convertPoint"].indexOf(root.toolName)>=0) {
                ctx.beginPath();ctx.moveTo(3,21);ctx.lineTo(6,10);ctx.lineTo(16,3);ctx.lineTo(22,9);ctx.lineTo(14,18);ctx.closePath();ctx.stroke()
                ctx.beginPath();ctx.moveTo(3,21);ctx.lineTo(12,12);ctx.stroke()
                ctx.beginPath();ctx.arc(13,11,1.5,0,Math.PI*2);ctx.stroke()
                if(root.toolName==="addAnchor" || root.toolName==="deleteAnchor") {
                    ctx.beginPath();ctx.moveTo(16,21);ctx.lineTo(23,21)
                    if(root.toolName==="addAnchor"){ctx.moveTo(19.5,17.5);ctx.lineTo(19.5,24)}ctx.stroke()
                }
                if(root.toolName==="convertPoint"){ctx.beginPath();ctx.moveTo(16,19);ctx.lineTo(20,23);ctx.lineTo(23,19);ctx.stroke()}
            } else if (root.toolName === "magicEraser") {
                ctx.beginPath();ctx.moveTo(2,16);ctx.lineTo(10,7);ctx.lineTo(18,15)
                ctx.lineTo(12,22);ctx.lineTo(7,22);ctx.closePath();ctx.stroke()
                ctx.beginPath();ctx.moveTo(5,13);ctx.lineTo(14,21);ctx.stroke()
                ctx.beginPath();ctx.moveTo(19,1);ctx.lineTo(19,9);ctx.moveTo(15,5);ctx.lineTo(23,5);ctx.stroke()
            } else if(root.toolName === "slice" || root.toolName === "sliceSelect") {
                ctx.setLineDash([3,2]);ctx.strokeRect(3,3,18,17);ctx.setLineDash([])
                if(root.toolName === "sliceSelect") {
                    ctx.beginPath();ctx.moveTo(10,7);ctx.lineTo(10,20);ctx.lineTo(14,16);ctx.lineTo(19,16);ctx.closePath();ctx.fill()
                } else {
                    ctx.beginPath();ctx.moveTo(5,18);ctx.lineTo(15,5);ctx.lineTo(19,8);ctx.lineTo(8,20);ctx.closePath();ctx.fill()
                }
            } else if (root.toolName === "crop") {
                ctx.beginPath();ctx.moveTo(6,2);ctx.lineTo(6,18);ctx.lineTo(22,18)
                ctx.moveTo(2,6);ctx.lineTo(18,6);ctx.lineTo(18,22);ctx.stroke()
                ctx.beginPath();ctx.moveTo(9,15);ctx.lineTo(15,9);ctx.stroke()
            } else if(root.toolName === "perspectiveCrop") {
                ctx.beginPath();ctx.moveTo(6,4);ctx.lineTo(20,7);ctx.lineTo(17,21);ctx.lineTo(3,18);ctx.closePath();ctx.stroke()
                ctx.beginPath();ctx.moveTo(5,9);ctx.lineTo(19,12);ctx.moveTo(4,14);ctx.lineTo(18,17)
                ctx.moveTo(11,5);ctx.lineTo(8,19);ctx.moveTo(16,6);ctx.lineTo(13,20);ctx.stroke()
            } else if (root.toolName === "picker" || root.toolName === "colorSampler") {
                ctx.beginPath();ctx.moveTo(3,21);ctx.lineTo(8,16);ctx.lineTo(7,15)
                ctx.lineTo(16,6);ctx.lineTo(20,10);ctx.lineTo(11,19);ctx.lineTo(10,18);ctx.closePath();ctx.stroke()
                ctx.beginPath();ctx.moveTo(14,5);ctx.lineTo(18,1);ctx.lineTo(23,6);ctx.lineTo(19,10);ctx.closePath();ctx.fill()
                if(root.toolName === "colorSampler") {
                    ctx.beginPath();ctx.arc(5,19,4,0,Math.PI*2);ctx.stroke()
                    ctx.beginPath();ctx.moveTo(0,19);ctx.lineTo(10,19);ctx.moveTo(5,14);ctx.lineTo(5,24);ctx.stroke()
                }
            } else if (root.toolName === "ruler") {
                ctx.save();ctx.translate(12,12);ctx.rotate(-Math.PI/4);ctx.strokeRect(-10,-4,20,8)
                for(let x=-6;x<10;x+=4) {ctx.beginPath();ctx.moveTo(x,-4);ctx.lineTo(x,0);ctx.stroke()};ctx.restore()
            } else if (root.toolName === "historyBrush" || root.toolName === "artHistoryBrush") {
                if (root.toolName === "artHistoryBrush") {
                    ctx.beginPath(); ctx.moveTo(2, 19); ctx.bezierCurveTo(8, 12, 11, 25, 16, 20); ctx.stroke()
                }
                ctx.beginPath(); ctx.arc(12, 11, 8, .7, 5.1); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(10, 2); ctx.lineTo(14, 3); ctx.lineTo(11, 6); ctx.stroke()
                ctx.save(); ctx.translate(17, 16); ctx.rotate(-Math.PI / 4)
                ctx.beginPath(); ctx.moveTo(-3, -1); ctx.lineTo(-2, -8); ctx.lineTo(2, -8)
                ctx.lineTo(3, -1); ctx.closePath(); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(-3, 0); ctx.lineTo(3, 0); ctx.lineTo(3, 5)
                ctx.lineTo(-3, 5); ctx.closePath(); ctx.fill(); ctx.restore()
            } else if (root.toolName === "remove") {
                ctx.save(); ctx.translate(10, 15); ctx.rotate(-Math.PI / 4)
                ctx.strokeRect(-4, -7, 8, 14)
                ctx.beginPath(); ctx.moveTo(-4, 2); ctx.lineTo(4, 2); ctx.stroke(); ctx.restore()
                ctx.beginPath(); ctx.moveTo(20, 2); ctx.lineTo(20, 10); ctx.moveTo(16, 6); ctx.lineTo(24, 6)
                ctx.moveTo(22, 14); ctx.lineTo(22, 18); ctx.moveTo(20, 16); ctx.lineTo(24, 16); ctx.stroke()
            } else if (root.toolName === "contentAwareMove") {
                ctx.lineWidth = 1.5
                ctx.beginPath(); ctx.moveTo(3, 6); ctx.lineTo(6, 3); ctx.lineTo(10, 4)
                ctx.lineTo(12, 8); ctx.lineTo(9, 12); ctx.lineTo(4, 11); ctx.closePath(); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(12, 15); ctx.lineTo(15, 12); ctx.lineTo(20, 13)
                ctx.lineTo(22, 18); ctx.lineTo(18, 22); ctx.lineTo(13, 20); ctx.closePath(); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(7, 17); ctx.lineTo(16, 8)
                ctx.moveTo(11, 8); ctx.lineTo(16, 8); ctx.lineTo(16, 13); ctx.stroke()
            } else if (root.toolName === "patch") {
                ctx.strokeRect(6, 6, 12, 12)
                for (const p of [8, 12, 16]) {
                    ctx.beginPath(); ctx.moveTo(p, 3); ctx.lineTo(p, 8)
                    ctx.moveTo(p, 16); ctx.lineTo(p, 21)
                    ctx.moveTo(3, p); ctx.lineTo(8, p)
                    ctx.moveTo(16, p); ctx.lineTo(21, p); ctx.stroke()
                }
            } else if (root.toolName === "healing" || root.toolName === "spotHealing") {
                ctx.save(); ctx.translate(12, 12); ctx.rotate(-Math.PI / 4)
                ctx.beginPath(); ctx.moveTo(-8, -4); ctx.lineTo(8, -4)
                ctx.bezierCurveTo(13, -4, 13, 4, 8, 4); ctx.lineTo(-8, 4)
                ctx.bezierCurveTo(-13, 4, -13, -4, -8, -4); ctx.closePath(); ctx.stroke()
                ctx.strokeRect(-3, -4, 6, 8)
                for (const x of [-7, 7]) for (const y of [-1.5, 1.5]) {
                    ctx.beginPath(); ctx.arc(x, y, .6, 0, Math.PI * 2); ctx.fill()
                }
                ctx.restore()
                if (root.toolName === "spotHealing") {
                    ctx.beginPath(); ctx.moveTo(4, 1); ctx.lineTo(4, 7)
                    ctx.moveTo(1, 4); ctx.lineTo(7, 4); ctx.stroke()
                }
            } else if (root.toolName === "redEye") {
                ctx.beginPath(); ctx.moveTo(2, 14)
                ctx.bezierCurveTo(6, 7, 18, 7, 22, 14)
                ctx.bezierCurveTo(18, 21, 6, 21, 2, 14); ctx.closePath(); ctx.stroke()
                ctx.beginPath(); ctx.arc(12, 14, 3.5, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(4, 2); ctx.lineTo(4, 8)
                ctx.moveTo(1, 5); ctx.lineTo(7, 5); ctx.stroke()
            } else if (root.toolName === "mixerBrush") {
                ctx.beginPath(); ctx.moveTo(6, 15); ctx.lineTo(17, 2); ctx.lineTo(20, 5)
                ctx.lineTo(9, 18); ctx.closePath(); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(6, 15); ctx.bezierCurveTo(2, 15, 6, 20, 2, 22)
                ctx.bezierCurveTo(8, 24, 12, 19, 9, 17); ctx.closePath(); ctx.fill()
                ctx.beginPath(); ctx.moveTo(17, 12); ctx.bezierCurveTo(17, 15, 13, 18, 15, 20)
                ctx.bezierCurveTo(19, 23, 23, 19, 20, 16); ctx.closePath(); ctx.stroke()
            } else if (root.toolName === "colorReplacement") {
                ctx.beginPath(); ctx.moveTo(5, 17); ctx.lineTo(15, 3)
                ctx.lineTo(18, 5); ctx.lineTo(8, 19); ctx.closePath(); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(5, 16); ctx.bezierCurveTo(0, 16, 5, 21, 2, 22)
                ctx.bezierCurveTo(6, 23, 9, 20, 8, 18); ctx.closePath(); ctx.fill()
                ctx.fillRect(13, 14, 6, 6); ctx.strokeRect(17, 18, 5, 5)
            } else if (root.toolName === "cloneStamp" || root.toolName === "patternStamp") {
                ctx.beginPath(); ctx.moveTo(8, 15); ctx.lineTo(9, 10)
                ctx.bezierCurveTo(5, 4, 7, 2, 12, 2)
                ctx.bezierCurveTo(17, 2, 19, 4, 15, 10)
                ctx.lineTo(16, 15); ctx.closePath(); ctx.fill()
                ctx.fillRect(4, 16, 16, 3); ctx.fillRect(3, 21, 18, 1.5)
                if (root.toolName === "patternStamp") {
                    ctx.fillRect(2, 5, 2, 2); ctx.fillRect(4, 7, 2, 2)
                    ctx.fillRect(2, 9, 2, 2)
                }
            } else if (root.toolName === "lasso") {
                ctx.beginPath()
                ctx.moveTo(6, 5)
                ctx.bezierCurveTo(2.8, 7.4, 3.6, 13.5, 8.2, 15.2)
                ctx.bezierCurveTo(12.8, 17, 19.8, 14.9, 20, 10.3)
                ctx.bezierCurveTo(20.2, 6.8, 16.8, 4.6, 12.3, 4.4)
                ctx.bezierCurveTo(9.5, 4.2, 7.3, 4.4, 6, 5)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(8.2, 15.2)
                ctx.bezierCurveTo(10.8, 17.1, 10.4, 20.2, 7.8, 20.5)
                ctx.bezierCurveTo(5.8, 20.7, 5.1, 18.8, 6.7, 18.1)
                ctx.stroke()
            } else if (root.toolName === "polygonalLasso") {
                ctx.beginPath()
                ctx.moveTo(4, 6)
                ctx.lineTo(14, 3.5)
                ctx.lineTo(20, 10)
                ctx.lineTo(15, 16)
                ctx.lineTo(7, 14)
                ctx.closePath()
                ctx.stroke()
                const vertices = [[4,6],[14,3.5],[20,10],[15,16],[7,14]]
                for (let i = 0; i < vertices.length; ++i) {
                    ctx.beginPath()
                    ctx.arc(vertices[i][0], vertices[i][1], 1.35, 0, Math.PI * 2)
                    ctx.fill()
                }
                ctx.beginPath()
                ctx.moveTo(7, 14)
                ctx.lineTo(5.5, 20)
                ctx.stroke()
            } else if (root.toolName === "quickSelection") {
                ctx.setLineDash([2.5, 2])
                ctx.beginPath()
                ctx.arc(9, 10, 6.2, -.4, Math.PI * 1.75)
                ctx.stroke()
                ctx.setLineDash([])
                ctx.beginPath()
                ctx.moveTo(13.5, 14.5)
                ctx.lineTo(20.5, 21.5)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(17.3, 4.5)
                ctx.lineTo(17.3, 10.5)
                ctx.moveTo(14.3, 7.5)
                ctx.lineTo(20.3, 7.5)
                ctx.stroke()
            } else if (root.toolName === "magicWand") {
                ctx.beginPath()
                ctx.moveTo(5, 20)
                ctx.lineTo(17.3, 7.7)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(15, 3)
                ctx.lineTo(15, 6)
                ctx.moveTo(20, 5)
                ctx.lineTo(18.2, 7.2)
                ctx.moveTo(22, 11)
                ctx.lineTo(19, 11)
                ctx.moveTo(19.8, 16)
                ctx.lineTo(17.6, 13.8)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(6.5, 18.5)
                ctx.lineTo(8.5, 20.5)
                ctx.lineTo(5.5, 22)
                ctx.lineTo(4, 20.5)
                ctx.closePath()
                ctx.fill()
            } else if (root.toolName === "smudge") {
                ctx.beginPath(); ctx.moveTo(5, 20); ctx.lineTo(5, 13)
                ctx.quadraticCurveTo(5, 11, 7, 11); ctx.lineTo(9, 13)
                ctx.lineTo(9, 4); ctx.quadraticCurveTo(9, 1, 12, 3)
                ctx.lineTo(13, 10); ctx.lineTo(18, 10)
                ctx.quadraticCurveTo(21, 10, 20, 14)
                ctx.lineTo(17, 21); ctx.closePath(); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(2, 16); ctx.lineTo(2, 21)
                ctx.moveTo(22, 17); ctx.lineTo(20, 22); ctx.stroke()
            } else if (root.toolName === "blur") {
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
