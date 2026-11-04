import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: root
    required property var document
    signal editCommitted()
    signal pathsRequested()
    readonly property bool modalOpen: fillDialog.visible || strokeDialog.visible || paintMenu.visible || widthPopup.visible || settings.visible
    readonly property bool shapeMode: document.penMode === "shape"
    readonly property var shapeStyle: document.penShapeStyle
    readonly property var activePath: document.activePenPath >= 0 ? document.penPaths[document.activePenPath] : null
    readonly property bool hasPoints: activePath !== null && activePath.points.length > 0
    readonly property bool editableShape: activePath !== null && Boolean(activePath.vectorMask)
    readonly property rect shapeBounds: document.penShapeBounds
    readonly property var numberLocale: Qt.locale()
    property bool proportionsLinked: false
    property string paintTarget: "fill"
    implicitHeight: 44
    color: "#252c35"

    function setPaint(color) {
        document.setPenShapeStyle(paintTarget + "Color", color)
    }
    function resizeShape(width, height) {
        document.resizePenShape(width, height)
        editCommitted()
    }
    // TextField owns its text editing: a SpinBox whose contentItem is a wrapper
    // Item cannot commit its nested TextInput through Qt's SpinBox machinery.
    function applyWidth() {
        const value = Number.fromLocaleString(root.numberLocale, widthField.text.trim())
        if (isFinite(value) && value >= .5 && value <= 500)
            document.setPenShapeStyle("strokeWidth", value)
        widthField.text = Number(shapeStyle.strokeWidth).toLocaleString(root.numberLocale, "f", 2)
    }
    component Swatch: Button {
        id: swatchControl
        required property string target
        required property color paintColor
        required property bool paintEnabled
        implicitWidth: 40; implicitHeight: 28
        ToolTip.text: target === "fill" ? qsTr("Fond : couleur ou aucun") : qsTr("Contour : couleur ou aucun")
        ToolTip.visible: hovered
        background: Rectangle {
            color: "#1b2128"; border.color: "#697582"; radius: 3
            Rectangle {
                anchors.fill: parent; anchors.margins: 4
                color: swatchControl.paintEnabled ? swatchControl.paintColor : "#edf0f4"
                border.color: "#a6adb7"
                Rectangle {
                    anchors.centerIn: parent
                    visible: !swatchControl.paintEnabled
                    width: parent.width; height: 2; rotation: -32; color: "#df5d68"
                }
            }
        }
        onClicked: { root.paintTarget = target; paintMenu.popup(this, 0, height) }
    }
    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: options.implicitWidth + 24
        contentHeight: 40
        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
        RowLayout {
            id: options
            x: 12; height: 40; spacing: 8
            ComboBox {
                objectName: "penModeCombo"
                implicitWidth: 106
                model: [qsTr("Forme"), qsTr("Tracé")]
                currentIndex: root.shapeMode ? 0 : 1
                onActivated: { root.document.penMode = currentIndex === 0 ? "shape" : "path"; root.editCommitted() }
                ToolTip.text: qsTr("Mode de dessin des prochains points")
                ToolTip.visible: hovered
            }
            Label { visible: !root.shapeMode; text: qsTr("Créer :"); color: "#cbd4dc" }
            Button {
                objectName: "penSelectionButton"
                visible: !root.shapeMode
                text: qsTr("Sélection…"); implicitHeight: 28; enabled: root.hasPoints
                onClicked: { root.document.penPathToSelection(); root.editCommitted() }
            }
            Button {
                visible: !root.shapeMode; text: qsTr("Masque (NYI)"); implicitHeight: 28; enabled: false
                ToolTip.text: qsTr("Créer un masque vectoriel depuis le tracé — non implémenté")
                ToolTip.visible: hovered
            }
            Button {
                objectName: "penMakeShapeButton"
                visible: !root.shapeMode; text: qsTr("Forme"); implicitHeight: 28
                enabled: root.hasPoints && root.activePath.points.length >= 2 && !root.editableShape
                ToolTip.text: qsTr("Créer une forme modifiable depuis le tracé sélectionné")
                ToolTip.visible: hovered
                onClicked: { root.document.penPathToShape(); root.editCommitted() }
            }
            Label { visible: root.shapeMode; text: qsTr("Fond :"); color: "#cbd4dc" }
            Swatch {
                objectName: "penShapeFillColor"
                visible: root.shapeMode; target: "fill"
                paintColor: root.shapeStyle.fillColor; paintEnabled: root.shapeStyle.fillEnabled
            }
            Label { visible: root.shapeMode; text: qsTr("Contour :"); color: "#cbd4dc" }
            Swatch {
                objectName: "penShapeStrokeColor"
                visible: root.shapeMode; target: "stroke"
                paintColor: root.shapeStyle.strokeColor; paintEnabled: root.shapeStyle.strokeEnabled
            }
            RowLayout {
                visible: root.shapeMode; spacing: 0
                TextField {
                    id: widthField
                    objectName: "penShapeStrokeWidth"
                    implicitWidth: 76; implicitHeight: 28
                    text: Number(root.shapeStyle.strokeWidth).toLocaleString(root.numberLocale, "f", 2)
                    selectByMouse: true
                    validator: DoubleValidator { bottom: .5; top: 500; decimals: 2; locale: root.numberLocale.name }
                    color: "#e8edf2"
                    background: Rectangle { color: "#1b2128"; border.color: widthField.activeFocus ? "#78a589" : "#4b5661"; radius: 3 }
                    onEditingFinished: root.applyWidth()
                    onAccepted: { root.applyWidth(); root.editCommitted() }
                    ToolTip.text: qsTr("Épaisseur du contour en pixels du document")
                    ToolTip.visible: hovered
                }
                Binding {
                    target: widthField; property: "text"
                    value: Number(root.shapeStyle.strokeWidth).toLocaleString(root.numberLocale, "f", 2)
                    when: !widthField.activeFocus
                }
                Label { text: " px"; color: "#aeb8c2" }
                ToolButton {
                    text: "⌄"; implicitWidth: 24; implicitHeight: 28
                    onClicked: widthPopup.open()
                    ToolTip.text: qsTr("Régler l’épaisseur du contour"); ToolTip.visible: hovered
                    Popup {
                        id: widthPopup
                        y: parent.height; x: -80; width: 220; height: 54
                        background: Rectangle { color: "#303741"; border.color: "#56616c"; radius: 4 }
                        Slider {
                            anchors.fill: parent
                            from: .5; to: 100; stepSize: .5
                            value: Math.min(100, root.shapeStyle.strokeWidth)
                            onMoved: root.document.setPenShapeStyle("strokeWidth", value)
                        }
                    }
                }
            }
            ComboBox {
                objectName: "penShapeStrokeStyle"
                visible: root.shapeMode; implicitWidth: 100; implicitHeight: 28
                model: [qsTr("Continu"), qsTr("Tirets"), qsTr("Pointillés")]
                currentIndex: root.shapeStyle.strokeStyle
                onActivated: root.document.setPenShapeStyle("strokeStyle", currentIndex)
                ToolTip.text: qsTr("Style du contour"); ToolTip.visible: hovered
            }
            Label { visible: root.shapeMode; text: qsTr("L :"); color: "#cbd4dc" }
            TextField {
                id: shapeWidth
                objectName: "penShapeWidth"
                visible: root.shapeMode; enabled: root.editableShape && root.shapeBounds.width > 0
                implicitWidth: 74; implicitHeight: 28
                text: Number(root.shapeBounds.width).toLocaleString(root.numberLocale, "f", 2)
                selectByMouse: true
                color: "#e8edf2"
                background: Rectangle { color: "#1b2128"; border.color: shapeWidth.activeFocus ? "#78a589" : "#4b5661"; radius: 3 }
                validator: DoubleValidator { bottom: .01; top: 1000000; decimals: 2; locale: root.numberLocale.name }
                onAccepted: {
                    const value = Number.fromLocaleString(root.numberLocale, text)
                    if (isFinite(value) && value > 0)
                        root.resizeShape(value, root.proportionsLinked ? root.shapeBounds.height*value/root.shapeBounds.width : root.shapeBounds.height)
                }
                ToolTip.text: qsTr("Largeur de la forme en pixels"); ToolTip.visible: hovered
            }
            ToolButton {
                visible: root.shapeMode; text: "↔"; checkable: true; checked: root.proportionsLinked
                implicitWidth: 28; implicitHeight: 28
                onToggled: root.proportionsLinked = checked
                ToolTip.text: qsTr("Conserver les proportions"); ToolTip.visible: hovered
            }
            Label { visible: root.shapeMode; text: qsTr("H :"); color: "#cbd4dc" }
            TextField {
                id: shapeHeight
                objectName: "penShapeHeight"
                visible: root.shapeMode; enabled: root.editableShape && root.shapeBounds.height > 0
                implicitWidth: 74; implicitHeight: 28
                text: Number(root.shapeBounds.height).toLocaleString(root.numberLocale, "f", 2)
                selectByMouse: true
                color: "#e8edf2"
                background: Rectangle { color: "#1b2128"; border.color: shapeHeight.activeFocus ? "#78a589" : "#4b5661"; radius: 3 }
                validator: DoubleValidator { bottom: .01; top: 1000000; decimals: 2; locale: root.numberLocale.name }
                onAccepted: {
                    const value = Number.fromLocaleString(root.numberLocale, text)
                    if (isFinite(value) && value > 0)
                        root.resizeShape(root.proportionsLinked ? root.shapeBounds.width*value/root.shapeBounds.height : root.shapeBounds.width, value)
                }
                ToolTip.text: qsTr("Hauteur de la forme en pixels"); ToolTip.visible: hovered
            }
            ToolButton { text: "⚙"; implicitWidth: 30; implicitHeight: 28; onClicked: settings.popup(this, 0, height); ToolTip.text: qsTr("Options de la Plume et du tracé"); ToolTip.visible: hovered }
            CheckBox { visible: root.document.tool === "pen"; text: qsTr("Ajout/Suppression auto"); checked: root.document.penAutoAddDelete; onToggled: root.document.penAutoAddDelete = checked }
            Button { objectName: "showPathsPanelButton"; text: qsTr("Tracés…"); implicitHeight: 28; onClicked: root.pathsRequested() }
        }
    }
    ColorDialog {
        id: fillDialog
        title: qsTr("Couleur du fond")
        onAccepted: root.document.setPenShapeStyle("fillColor", selectedColor.toString())
    }
    ColorDialog {
        id: strokeDialog
        title: qsTr("Couleur du contour")
        onAccepted: root.document.setPenShapeStyle("strokeColor", selectedColor.toString())
    }
    DarkMenu {
        id: paintMenu
        objectName: "penPaintMenu"
        DarkMenuItem {
            objectName: "penPaintNone"
            text: qsTr("Aucun"); checkable: true
            checked: !root.shapeStyle[root.paintTarget + "Enabled"]
            onTriggered: root.document.setPenShapeStyle(root.paintTarget + "Enabled", false)
        }
        DarkMenuItem {
            objectName: "penPaintSolid"
            text: qsTr("Couleur unie…")
            onTriggered: {
                if (root.paintTarget === "fill") { fillDialog.selectedColor = root.shapeStyle.fillColor; fillDialog.open() }
                else { strokeDialog.selectedColor = root.shapeStyle.strokeColor; strokeDialog.open() }
            }
        }
        DarkMenuItem {
            objectName: "penPaintForeground"
            text: qsTr("Couleur de premier plan")
            onTriggered: root.setPaint(root.document.color.toString())
        }
        MenuSeparator {}
        Repeater {
            model: ["#000000", "#ffffff", "#ff9900", "#ff209f", "#e34f32", "#428fe8"]
            delegate: DarkMenuItem {
                required property string modelData
                text: modelData
                icon.source: ""
                contentItem: Row {
                    spacing: 10
                    Rectangle { width: 24; height: 18; color: modelData; border.color: "#a6adb7" }
                    Text { text: modelData; color: "#e0e6ed" }
                }
                onTriggered: root.setPaint(modelData)
            }
        }
    }
    DarkMenu {
        id: settings
        DarkMenuItem { text: qsTr("Aperçu élastique"); checkable: true; checked: root.document.penRubberBand; onTriggered: root.document.penRubberBand = checked }
        MenuSeparator {}
        DarkMenuItem { text: qsTr("Terminer le tracé"); enabled: root.document.penDrawing; onTriggered: { root.document.finishPenPath(); root.editCommitted() } }
        DarkMenuItem { text: qsTr("Fermer le tracé"); enabled: root.hasPoints; onTriggered: { root.document.closePenPath(); root.editCommitted() } }
        DarkMenuItem { text: qsTr("Masquer le tracé d’édition"); enabled: root.hasPoints; onTriggered: { root.document.setActivePenPath(-1); root.editCommitted() } }
        MenuSeparator {}
        ContextMenuItem { text: qsTr("Opérations de tracé (NYI)"); enabled: false }
        ContextMenuItem { text: qsTr("Alignement et disposition des tracés (NYI)"); enabled: false }
        ContextMenuItem { text: qsTr("Aligner les bords (NYI)"); enabled: false }
        ContextMenuItem { text: qsTr("Simulation de pression (NYI)"); enabled: false }
    }
}
