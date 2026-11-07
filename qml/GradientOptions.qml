import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    required property var document
    signal editCommitted()
    spacing: 6

    ComboBox {
        objectName: "gradientApplyMode"
        model: root.document.editingMask ? [qsTr("Masque de fusion")] : [qsTr("Dégradé classique"), qsTr("Dégradé dynamique")]
        currentIndex: root.document.editingMask ? 0 : root.document.gradientNewLayer ? 1 : 0
        enabled: !root.document.editingMask
        implicitWidth: 164
        onActivated: {
            root.document.gradientNewLayer = currentIndex === 1
            root.editCommitted()
        }
    }
    ComboBox {
        id: presets
        objectName: "gradientPreset"
        model: [qsTr("Premier plan vers arrière-plan"), qsTr("Premier plan vers transparent"),
            qsTr("Noir, blanc"), qsTr("Crépuscule"), qsTr("Spectre")]
        currentIndex: root.document.gradientPreset
        implicitWidth: 128
        popup.width: Math.max(width, 265)
        Accessible.name: qsTr("Préréglages du dégradé")
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Préréglage du dégradé")
        onActivated: {
            root.document.gradientPreset = currentIndex
            root.editCommitted()
        }
        contentItem: Canvas {
            id: preview
            readonly property var stops: root.document.gradientPreview
            onStopsChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const w = width - 7
                const h = height - 10
                if (w <= 0 || h <= 0 || !stops.length) return
                ctx.save()
                ctx.beginPath()
                ctx.rect(5, 5, w, h)
                ctx.clip()
                for (let y = 0; y < h; y += 5)
                    for (let x = 0; x < w; x += 5) {
                        ctx.fillStyle = ((x / 5 + y / 5) % 2) ? "#929ba4" : "#e7eaed"
                        ctx.fillRect(x + 5, y + 5, 5, 5)
                    }
                const ramp = ctx.createLinearGradient(5, 0, w + 5, 0)
                for (const stop of stops)
                    ramp.addColorStop(stop.position, stop.color)
                ctx.fillStyle = ramp
                ctx.fillRect(5, 5, w, h)
                ctx.restore()
            }
        }
    }
    Repeater {
        model: [qsTr("Linéaire"), qsTr("Radial"), qsTr("Angulaire"), qsTr("Réfléchi"), qsTr("Losange")]
        delegate: ToolButton {
            id: button
            required property int index
            required property string modelData
            objectName: "gradientShape_" + index
            implicitWidth: 32
            implicitHeight: 30
            checked: root.document.gradientShape === index
            ToolTip.visible: hovered
            ToolTip.text: modelData
            Accessible.name: modelData
            background: Rectangle {
                color: button.checked ? "#48535f" : button.hovered ? "#353e49" : "transparent"
                radius: 4
            }
            contentItem: Canvas {
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const x = (width - 18) / 2
                    const y = (height - 18) / 2
                    ctx.translate(x, y)
                    ctx.strokeStyle = "#dbe3eb"
                    let ramp
                    if (button.index === 1)
                        ramp = ctx.createRadialGradient(9, 9, 0, 9, 9, 9)
                    else
                        ramp = ctx.createLinearGradient(0, 0, button.index === 3 ? 0 : 18,
                            button.index === 3 ? 18 : 0)
                    ramp.addColorStop(0, button.index === 1 ? "#f4f6f8" : "#20252c")
                    if (button.index === 3) ramp.addColorStop(.5, "#f4f6f8")
                    ramp.addColorStop(1, button.index === 1 || button.index === 3 ? "#20252c" : "#f4f6f8")
                    ctx.fillStyle = button.index < 2 || button.index === 3 ? ramp : "#38434f"
                    ctx.fillRect(0, 0, 18, 18)
                    ctx.strokeRect(.5, .5, 17, 17)
                    if (button.index === 2 || button.index === 4) {
                        ctx.beginPath()
                        if (button.index === 2) {
                            ctx.moveTo(9, 9)
                            ctx.lineTo(16, 9)
                            ctx.arc(9, 9, 7, 0, -Math.PI * 1.35, true)
                        } else {
                            ctx.moveTo(9, 1)
                            ctx.lineTo(17, 9)
                            ctx.lineTo(9, 17)
                            ctx.lineTo(1, 9)
                            ctx.closePath()
                        }
                        ctx.stroke()
                    }
                }
            }
            onClicked: {
                root.document.gradientShape = index
                root.editCommitted()
            }
        }
    }
    CheckBox {
        objectName: "gradientReverse"
        text: qsTr("Inverser")
        checked: root.document.gradientReverse
        onToggled: { root.document.gradientReverse = checked; root.editCommitted() }
    }
    CheckBox {
        objectName: "gradientDither"
        text: qsTr("Tramage")
        checked: root.document.gradientDither
        onToggled: { root.document.gradientDither = checked; root.editCommitted() }
    }
    Label { text: qsTr("Méthode"); color: "#c8d0d9" }
    ComboBox {
        objectName: "gradientMethod"
        model: [qsTr("Classique"), qsTr("Linéaire"), qsTr("Perceptive")]
        currentIndex: root.document.gradientMethod
        implicitWidth: 126
        onActivated: {
            root.document.gradientMethod = currentIndex
            root.editCommitted()
        }
    }
    ScrubLabel { control: gradientOpacity; visible: !root.document.gradientNewLayer; text: qsTr("Opacité"); color: "#c8d0d9" }
    DarkSpinBox { id: gradientOpacity;
        objectName: "gradientOpacity"
        visible: !root.document.gradientNewLayer
        from: 1; to: 100
        implicitWidth: 100
        suffix: " %"
        value: Math.round(root.document.brushOpacity * 100)
        onValueModified: root.document.brushOpacity = value / 100
    }
}
