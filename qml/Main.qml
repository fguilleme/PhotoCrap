import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQml.Models
import PhotoCrap.Native 1.0

ApplicationWindow {
    id: window
    width: 1480
    height: 940
    minimumWidth: 1000
    minimumHeight: 680
    visible: true
    title: (editor.modified ? "● " : "") + editor.name + " — PhotoCrap"
    color: "#191d23"
    font.family: Qt.platform.os === "osx" ? "SF Pro Text" : "sans-serif"
    font.pixelSize: 12
    palette.window: "#22272f"
    palette.windowText: "#dbe1e8"
    palette.base: "#191e25"
    palette.alternateBase: "#292f38"
    palette.text: "#dbe1e8"
    palette.button: "#303741"
    palette.buttonText: "#dbe1e8"
    palette.highlight: "#597f6b"
    palette.highlightedText: "#ffffff"
    palette.mid: "#343d48"
    palette.dark: "#171c22"
    property real zoom: 0.6
    property string status: "Prêt à créer"
    property string pendingAction: ""
    property bool allowClose: false
    property bool resumeAfterSave: false
    property bool importing: false
    property bool changingSecondary: false
    property bool editingText: activeFocusItem !== null && typeof activeFocusItem.cursorPosition === "number"
    property bool modalOpen: newDialog.opened || resizeDialog.opened || adjustmentDialog.opened || renameDialog.opened || preferencesDialog.opened || unsavedDialog.opened || errorDialog.opened || aboutDialog.opened || colorDialog.visible || openDialog.visible || saveDialog.visible || exportDialog.visible
    property string adjustment: "brightness"
    property string adjustmentTitle: "Luminosité"
    property var tools: [
        {name:"move", icon:"↖", label:"Déplacement", shortcut:"toolMove"},
        {name:"select", icon:"⬚", label:"Sélection rectangulaire", shortcut:"toolSelect"},
        {name:"crop", icon:"⌗", label:"Recadrage", shortcut:"toolCrop"},
        {name:"brush", icon:"╱", label:"Pinceau", shortcut:"toolBrush"},
        {name:"eraser", icon:"▱", label:"Gomme", shortcut:"toolEraser"},
        {name:"fill", icon:"◩", label:"Remplissage contigu", shortcut:"toolFill"},
        {name:"gradient", icon:"▨", label:"Dégradé", shortcut:"toolGradient"},
        {name:"picker", icon:"⌖", label:"Pipette", shortcut:"toolPicker"},
        {name:"text", icon:"T", label:"Texte rasterisé", shortcut:"toolText"},
        {name:"rectangle", icon:"□", label:"Rectangle", shortcut:"toolRectangle"},
        {name:"ellipse", icon:"○", label:"Ellipse", shortcut:"toolEllipse"},
        {name:"line", icon:"╲", label:"Ligne", shortcut:"toolLine"},
        {name:"hand", icon:"✥", label:"Navigation", shortcut:"toolHand"}
    ]
    property var activeInfo: {
        const list = editor.layers
        for (let i=0; i<list.length; ++i) if (list[i].layerIndex === editor.activeLayer) return list[i]
        return {name:"", opacity:1, blend:0, locked:false}
    }
    property string toolName: {
        for (let i=0; i<tools.length; ++i) if (tools[i].name === editor.tool) return tools[i].label
        return "Pinceau"
    }
    function fitCanvas() {
        zoom = Math.max(0.02, Math.min(1, (viewport.width-110)/editor.width, (viewport.height-110)/editor.height))
        viewport.contentX = 0
        viewport.contentY = 0
    }
    function setZoom(value) { zoom = Math.max(0.02, Math.min(4, value)) }
    function perform(action) {
        if (action === "new") newDialog.open()
        else if (action === "open") { importing = false; openDialog.open() }
        else if (action === "demo") { editor.createDemo(); fitCanvas() }
        else if (action === "close") { allowClose = true; close() }
    }
    function guard(action) {
        editor.endStroke()
        pendingAction = action
        if (editor.modified) unsavedDialog.open()
        else perform(action)
    }
    function save(asNew) {
        if (!asNew && editor.projectUrl.toString().length) {
            if (editor.saveProject(editor.projectUrl) && resumeAfterSave) {
                resumeAfterSave = false
                perform(pendingAction)
            }
        } else saveDialog.open()
    }
    function adjust(kind, label, initial) {
        adjustment = kind; adjustmentTitle = label; adjustmentSlider.value = initial
        adjustmentDialog.open()
    }
    onClosing: function(event) {
        if (!allowClose && editor.modified) { event.accepted = false; guard("close") }
    }
    Component.onCompleted: Qt.callLater(fitCanvas)
    Connections {
        target: editor
        function onErrorOccurred(message) { errorLabel.text = message; errorDialog.open() }
        function onStatusMessage(message) { window.status = message; statusTimer.restart() }
        function onDocumentChanged() { Qt.callLater(window.fitCanvas) }
    }
    Connections {
        target: shortcuts
        function onErrorOccurred(message) { errorLabel.text = message; errorDialog.open() }
    }
    Timer { id: statusTimer; interval: 5000; onTriggered: window.status = "Prêt à créer" }

    Action { id: newAction; text: "Nouveau…"; shortcut: (shortcuts.revision, shortcuts.sequence("new")); enabled: !window.modalOpen; onTriggered: guard("new") }
    Action { id: openAction; text: "Ouvrir…"; shortcut: (shortcuts.revision, shortcuts.sequence("open")); enabled: !window.modalOpen; onTriggered: guard("open") }
    Action { id: saveAction; text: "Enregistrer le projet"; shortcut: (shortcuts.revision, shortcuts.sequence("save")); enabled: !window.modalOpen; onTriggered: { resumeAfterSave=false; save(false) } }
    Action { id: saveAsAction; text: "Enregistrer sous…"; shortcut: (shortcuts.revision, shortcuts.sequence("saveAs")); enabled: !window.modalOpen; onTriggered: { resumeAfterSave=false; save(true) } }
    Action { id: exportAction; text: "Exporter l’image…"; shortcut: (shortcuts.revision, shortcuts.sequence("export")); enabled: !window.modalOpen; onTriggered: exportDialog.open() }
    Action { id: importAction; text: "Importer comme calque…"; shortcut: (shortcuts.revision, shortcuts.sequence("import")); enabled: !window.modalOpen; onTriggered: { importing=true; openDialog.open() } }
    Action { id: undoAction; text: "Annuler"; shortcut: (shortcuts.revision, shortcuts.sequence("undo")); enabled: editor.canUndo && !window.modalOpen; onTriggered: editor.undo() }
    Action { id: redoAction; text: "Rétablir"; shortcut: (shortcuts.revision, shortcuts.sequence("redo")); enabled: editor.canRedo && !window.modalOpen; onTriggered: editor.redo() }
    Action { id: copyAction; text: "Copier l’image visible"; shortcut: (shortcuts.revision, shortcuts.sequence("copy")); enabled: !window.editingText && !window.modalOpen; onTriggered: editor.copy() }
    Action { id: pasteAction; text: "Coller comme calque"; shortcut: (shortcuts.revision, shortcuts.sequence("paste")); enabled: !window.editingText && !window.modalOpen; onTriggered: editor.paste() }
    Action { id: selectAction; text: "Tout sélectionner"; shortcut: (shortcuts.revision, shortcuts.sequence("selectAll")); enabled: !window.editingText && !window.modalOpen; onTriggered: editor.selectAll() }
    Action { id: deselectAction; text: "Désélectionner"; shortcut: (shortcuts.revision, shortcuts.sequence("deselect")); enabled: !window.editingText && !window.modalOpen; onTriggered: editor.deselect() }
    Action { id: deleteAction; text: "Effacer les pixels"; shortcut: (shortcuts.revision, shortcuts.sequence("clear")); enabled: !window.editingText && !window.modalOpen; onTriggered: editor.clearSelected() }
    Action { id: addAction; text: "Nouveau calque"; shortcut: (shortcuts.revision, shortcuts.sequence("newLayer")); enabled: !window.modalOpen; onTriggered: editor.addLayer() }
    Action { id: duplicateAction; text: "Dupliquer le calque"; shortcut: (shortcuts.revision, shortcuts.sequence("duplicateLayer")); enabled: !window.modalOpen; onTriggered: editor.duplicateLayer() }
    Action { id: preferencesAction; text: "Réglages…"; shortcut: (shortcuts.revision, shortcuts.sequence("preferences")); enabled: !window.modalOpen; onTriggered: preferencesDialog.open() }
    Instantiator {
        model: window.tools
        delegate: Shortcut {
            required property var modelData
            sequence: (shortcuts.revision, shortcuts.sequence(modelData.shortcut))
            enabled: !window.editingText && !window.modalOpen
            onActivated: { editor.tool = modelData.name; workspace.forceActiveFocus() }
        }
    }
    Shortcut { enabled: !window.modalOpen; sequence: "Escape"; onActivated: { editor.cancelStroke(); editor.deselect() } }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("zoomFit")); onActivated: fitCanvas() }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("zoomActual")); onActivated: setZoom(1) }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("brushSmaller")); onActivated: editor.brushSize = editor.brushSize-5 }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("brushLarger")); onActivated: editor.brushSize = editor.brushSize+5 }

    menuBar: MenuBar {
        background: Rectangle { color: "#22272e" }
        Menu {
            title: "Fichier"
            MenuItem { action: newAction }
            MenuItem { action: openAction }
            MenuItem { action: importAction }
            MenuSeparator {}
            MenuItem { action: saveAction }
            MenuItem { action: saveAsAction }
            MenuItem { action: exportAction }
            MenuSeparator {}
            MenuItem { text: "Ouvrir la composition de démonstration"; onTriggered: guard("demo") }
            MenuItem { text: "Quitter"; onTriggered: guard("close") }
        }
        Menu {
            title: "Édition"
            MenuItem { action: undoAction }
            MenuItem { action: redoAction }
            MenuSeparator {}
            MenuItem { action: copyAction }
            MenuItem { action: pasteAction }
            MenuItem { action: deleteAction }
            MenuSeparator {}
            MenuItem { action: preferencesAction }
        }
        Menu {
            title: "Image"
            MenuItem { text: "Dimensions de l’image…"; onTriggered: { resizeWidth.value=editor.width; resizeHeight.value=editor.height; resizeDialog.open() } }
            MenuItem { text: "Recadrer selon la sélection"; enabled: editor.hasSelection; onTriggered: editor.cropToSelection() }
            MenuSeparator {}
            MenuItem { text: "Rotation de 90° à droite"; onTriggered: editor.transformImage("rotateRight") }
            MenuItem { text: "Rotation de 90° à gauche"; onTriggered: editor.transformImage("rotateLeft") }
            MenuItem { text: "Symétrie horizontale"; onTriggered: editor.transformImage("flipH") }
            MenuItem { text: "Symétrie verticale"; onTriggered: editor.transformImage("flipV") }
        }
        Menu {
            title: "Calque"
            MenuItem { action: addAction }
            MenuItem { action: duplicateAction }
            MenuItem { text: "Renommer…"; onTriggered: { layerName.text=activeInfo.name; renameDialog.open() } }
            MenuItem { text: "Fusionner vers le bas"; enabled: editor.activeLayer>0; onTriggered: editor.mergeDown() }
            MenuItem { text: "Supprimer le calque"; enabled: editor.layers.length>1; onTriggered: editor.removeLayer() }
        }
        Menu {
            title: "Sélection"
            MenuItem { action: selectAction }
            MenuItem { action: deselectAction }
        }
        Menu {
            title: "Réglages"
            MenuItem { text: "Luminosité…"; onTriggered: adjust("brightness","Luminosité",0) }
            MenuItem { text: "Contraste…"; onTriggered: adjust("contrast","Contraste",0) }
            MenuItem { text: "Saturation…"; onTriggered: adjust("saturation","Saturation",0) }
            MenuSeparator {}
            MenuItem { text: "Noir et blanc"; onTriggered: editor.applyFilter("grayscale") }
            MenuItem { text: "Négatif"; onTriggered: editor.applyFilter("invert") }
            MenuItem { text: "Sépia"; onTriggered: editor.applyFilter("sepia") }
            MenuItem { text: "Flou…"; onTriggered: adjust("blur","Rayon du flou",5) }
            MenuItem { text: "Renforcer la netteté"; onTriggered: editor.applyFilter("sharpen") }
        }
        Menu {
            title: "Affichage"
            MenuItem { text: "Adapter à la fenêtre"; onTriggered: fitCanvas() }
            MenuItem { text: "Taille réelle · 100 %"; onTriggered: setZoom(1) }
            MenuItem { text: "Zoom avant"; onTriggered: setZoom(zoom*1.25) }
            MenuItem { text: "Zoom arrière"; onTriggered: setZoom(zoom/1.25) }
        }
        Menu {
            title: "Aide"
            MenuItem { text: "Raccourcis clavier…"; onTriggered: preferencesDialog.open() }
            MenuItem { text: "À propos de PhotoCrap"; onTriggered: aboutDialog.open() }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 64; color: "#242a32"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18; spacing: 14
                Rectangle {
                    width: 33; height: 33; radius: 9; color: "#bfefce"
                    Text { anchors.centerIn: parent; text: "P"; color: "#1d342b"; font.bold: true; font.pixelSize: 24 }
                }
                Column {
                    spacing: 3
                    Label { text: "PhotoCrap"; font.pixelSize: 16; font.bold: true; color: "#f0f3f6" }
                    Label { text: "L’ATELIER NUMÉRIQUE"; font.pixelSize: 8; font.letterSpacing: 2; color: "#87929e" }
                }
                Rectangle { Layout.leftMargin: 10; width: 1; height: 28; color: "#3a414a" }
                Label { visible: window.width>=1250; text: window.toolName; color: "#c8d0d9"; Layout.preferredWidth: 152 }
                Label { text: "Taille"; color: "#87929e" }
                SpinBox { from: 1; to: 500; value: Math.round(editor.brushSize); editable: true; implicitWidth: 124; onValueModified: editor.brushSize=value }
                Label { text: "Opacité"; color: "#87929e"; Layout.leftMargin: 8 }
                SpinBox { from: 1; to: 100; value: Math.round(editor.brushOpacity*100); editable: true; implicitWidth: 124; onValueModified: editor.brushOpacity=value/100 }
                CheckBox { text: "Remplir"; checked: editor.filledShapes; visible: editor.tool==="rectangle" || editor.tool==="ellipse"; onToggled: editor.filledShapes=checked }
                Button { text: "Recadrer"; visible: editor.tool==="crop"; enabled: editor.hasSelection; onClicked: editor.cropToSelection() }
                Item { Layout.fillWidth: true }
                IconButton { glyph: "↶"; hint: "Annuler"; enabled: editor.canUndo; onClicked: editor.undo() }
                IconButton { glyph: "↷"; hint: "Rétablir"; enabled: editor.canRedo; onClicked: editor.redo() }
                Button {
                    text: "Exporter ↗"; implicitWidth: 116
                    background: Rectangle { color: parent.down ? "#94caa4" : "#beeccb"; radius: 5 }
                    contentItem: Text { text: parent.text; color: "#203b2c"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: exportDialog.open()
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#10141a" }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.preferredWidth: 58; Layout.fillHeight: true; color: "#222830"
                Flickable {
                    anchors.fill: parent; contentHeight: toolColumn.height+26; clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    Column {
                    id: toolColumn
                    y: 13; anchors.horizontalCenter: parent.horizontalCenter; spacing: 4
                    Repeater {
                        model: window.tools
                        delegate: IconButton {
                            required property var modelData
                            glyph: modelData.icon; hint: modelData.label + " (" + modelData.key + ")"
                            selected: editor.tool===modelData.name
                            onClicked: { editor.tool=modelData.name; workspace.forceActiveFocus() }
                        }
                    }
                    Rectangle { width: 30; height: 1; color: "#3b444e"; anchors.horizontalCenter: parent.horizontalCenter }
                    Item {
                        width: 38; height: 48
                        Rectangle { x: 13; y: 15; width: 24; height: 24; radius: 3; color: editor.secondaryColor; border.color: "#87939e"; MouseArea { anchors.fill: parent; onClicked: { changingSecondary=true; colorDialog.selectedColor=editor.secondaryColor; colorDialog.open() } } }
                        Rectangle { x: 2; y: 4; width: 25; height: 25; radius: 3; color: editor.color; border.color: "#b5c4bf"; MouseArea { anchors.fill: parent; onClicked: { changingSecondary=false; colorDialog.selectedColor=editor.color; colorDialog.open() } } }
                    }
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
                Rectangle {
                    Layout.fillWidth: true; height: 42; color: "#1f242c"
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 12; spacing: 10
                        Rectangle { width: 6; height: 6; radius: 3; color: editor.modified ? "#c7e5b4" : "#687482" }
                        Label { text: editor.name; color: "#dce2e8"; font.pixelSize: 12 }
                        Label { text: "@ " + Math.round(zoom*100) + " %"; color: "#73808e" }
                        Item { Layout.fillWidth: true }
                        Label { text: "RVB / 8"; color: "#73808e"; font.pixelSize: 10 }
                        IconButton { glyph: "+"; hint: "Nouveau document"; implicitWidth: 27; onClicked: guard("new") }
                    }
                }
                Item {
                    id: workspace
                    Layout.fillWidth: true; Layout.fillHeight: true; focus: true
                    Rectangle { anchors.fill: parent; color: "#171c22" }
                    // Rulers follow the document's zoom and scroll position.
                    Canvas {
                        id: horizontalRuler
                        x: 24; width: parent.width-24; height: 24
                        onPaint: {
                            const c=getContext("2d"); c.reset(); c.fillStyle="#20262e"; c.fillRect(0,0,width,height)
                            c.fillStyle="#788491"; c.strokeStyle="#46505b"; c.font="9px sans-serif"
                            const step=zoom<0.3?500:zoom<0.65?200:zoom<1.5?100:50
                            const offset=artboard.x-viewport.contentX
                            for(let i=0;i<=editor.width;i+=step) { let px=offset+i*zoom; c.fillText(i,px+4,12); c.beginPath(); c.moveTo(px,17);c.lineTo(px,24);c.stroke() }
                        }
                    }
                    Canvas {
                        id: verticalRuler
                        y: 24; width: 24; height: parent.height-24
                        onPaint: {
                            const c=getContext("2d"); c.reset(); c.fillStyle="#20262e"; c.fillRect(0,0,width,height)
                            c.fillStyle="#788491"; c.strokeStyle="#46505b"; c.font="9px sans-serif"
                            const step=zoom<0.3?500:zoom<0.65?200:zoom<1.5?100:50
                            const offset=artboard.y-viewport.contentY
                            for(let i=0;i<=editor.height;i+=step) { let py=offset+i*zoom; c.save();c.translate(11,py+4);c.rotate(-Math.PI/2);c.fillText(i,0,0);c.restore();c.beginPath();c.moveTo(17,py);c.lineTo(24,py);c.stroke() }
                        }
                    }
                    Connections { target: window; function onZoomChanged() { horizontalRuler.requestPaint(); verticalRuler.requestPaint() } }
                    Connections { target: editor; function onDocumentChanged() { horizontalRuler.requestPaint(); verticalRuler.requestPaint() } }
                    Flickable {
                        id: viewport
                        anchors.fill: parent; anchors.leftMargin: 24; anchors.topMargin: 24
                        clip: true; boundsBehavior: Flickable.StopAtBounds
                        contentWidth: Math.max(width,artboard.width+110)
                        contentHeight: Math.max(height,artboard.height+110)
                        interactive: editor.tool==="hand"
                        onContentXChanged: horizontalRuler.requestPaint()
                        onContentYChanged: verticalRuler.requestPaint()
                        onWidthChanged: horizontalRuler.requestPaint()
                        onHeightChanged: verticalRuler.requestPaint()
                        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        Item {
                            id: artboard
                            x: (viewport.contentWidth-width)/2; y: (viewport.contentHeight-height)/2
                            width: editor.width*zoom; height: editor.height*zoom
                            onXChanged: horizontalRuler.requestPaint()
                            onYChanged: verticalRuler.requestPaint()
                            Rectangle { x: 5; y: 9; width: parent.width; height: parent.height; color: "#0c1015"; opacity: 0.6 }
                            ImageCanvas { anchors.fill: parent; document: editor }
                            Rectangle {
                                visible: editor.hasSelection
                                x: editor.selection.x*zoom; y: editor.selection.y*zoom
                                width: editor.selection.width*zoom; height: editor.selection.height*zoom
                                color: "#153fe0b0"; border.width: 1; border.color: "#ffffff"
                                Rectangle { anchors.fill: parent; anchors.margins: 1; color: "transparent"; border.width: 1; border.color: "#263a33" }
                            }
                            MouseArea {
                                id: drawingArea
                                objectName: "drawingArea"
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: editor.tool!=="hand"
                                preventStealing: true
                                cursorShape: editor.tool==="move" ? Qt.SizeAllCursor : Qt.CrossCursor
                                onPressed: function(mouse) { workspace.forceActiveFocus(); editor.beginStroke(mouse.x/zoom,mouse.y/zoom) }
                                onPositionChanged: function(mouse) {
                                    window.status = "X " + Math.round(mouse.x/zoom) + "   Y " + Math.round(mouse.y/zoom)
                                    if(pressed) editor.continueStroke(mouse.x/zoom,mouse.y/zoom)
                                }
                                onReleased: editor.endStroke()
                                onCanceled: editor.cancelStroke()
                                onWheel: function(wheel) {
                                    if(wheel.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) {
                                        setZoom(zoom*(wheel.angleDelta.y>0?1.1:1/1.1)); wheel.accepted=true
                                    } else {
                                        viewport.contentY = Math.max(0, Math.min(viewport.contentHeight-viewport.height,viewport.contentY-wheel.angleDelta.y/2))
                                        viewport.contentX = Math.max(0, Math.min(viewport.contentWidth-viewport.width,viewport.contentX-wheel.angleDelta.x/2))
                                        wheel.accepted=true
                                    }
                                }
                            }
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; height: editor.tool==="text" ? 54 : 40; color: "#20262e"
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 15; anchors.rightMargin: 15
                        Label { text: editor.tool==="text" ? "Texte" : editor.tool==="crop" ? "Tracez une zone, puis cliquez sur Recadrer" : ""; color: "#8e9aa7" }
                        TextField { id: textField; objectName: "textField"; visible: editor.tool==="text"; text: editor.text; Layout.fillWidth: true; placeholderText: "Cliquez ensuite sur le canevas"; onTextEdited: editor.text=text }
                        SpinBox { visible: editor.tool==="text"; from: 6; to: 512; value: editor.textSize; editable: true; onValueModified: editor.textSize=value }
                        Item { Layout.fillWidth: true; visible: editor.tool!=="text" }
                        IconButton { text: "−"; implicitHeight: 28; onClicked: setZoom(zoom/1.25) }
                        Label { text: Math.round(zoom*100)+" %"; color: "#bec8d2"; horizontalAlignment: Text.AlignHCenter; Layout.preferredWidth: 50 }
                        IconButton { text: "+"; implicitHeight: 28; onClicked: setZoom(zoom*1.25) }
                        IconButton { text: "Adapter"; implicitWidth: 65; implicitHeight: 28; hint: "Adapter à la fenêtre"; onClicked: fitCanvas() }
                    }
                }
            }
            Rectangle { width: 1; Layout.fillHeight: true; color: "#10141a" }
            Rectangle {
                Layout.preferredWidth: 280; Layout.fillHeight: true; color: "#242a32"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 15; spacing: 8
                    PanelTitle { visible: window.height>=800; text: "NAVIGATEUR" }
                    Rectangle {
                        visible: window.height>=800; Layout.fillWidth: true; height: 90; color: "#181e25"; radius: 5
                        Image { anchors.fill: parent; anchors.margins: 8; source: "image://layers/composite/"+editor.revision; sourceSize.width: 240; sourceSize.height: 105; fillMode: Image.PreserveAspectFit; cache: false }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: editor.width+" × "+editor.height+" px"; color: "#9ba7b5"; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Label { text: "sRGB"; color: "#74828e"; font.pixelSize: 10 }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#363e47" }
                    PanelTitle { text: "COULEUR" }
                    RowLayout {
                        Layout.fillWidth: true
                        Rectangle {
                            width: 35; height: 33; radius: 5; color: editor.color; border.color: "#899b96"
                            MouseArea { anchors.fill: parent; onClicked: { changingSecondary=false; colorDialog.selectedColor=editor.color; colorDialog.open() } }
                        }
                        TextField {
                            id: hexField; Layout.fillWidth: true; text: editor.color.toString().toUpperCase()
                            maximumLength: 9; selectByMouse: true
                            validator: RegularExpressionValidator { regularExpression: /#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?/ }
                            onEditingFinished: if(acceptableInput) editor.color=text
                        }
                        IconButton { glyph: "⇄"; hint: "Échanger les couleurs"; onClicked: { const c=editor.color; editor.color=editor.secondaryColor; editor.secondaryColor=c } }
                    }
                    Row {
                        spacing: 7
                        Repeater {
                            model: ["#f0f0e6","#b1f5cb","#ffdca3","#edafa1","#bdabdc","#87b9d0","#243644","#14191f"]
                            delegate: Rectangle {
                                required property string modelData
                                width: 25; height: 21; radius: 4; color: modelData
                                border.width: editor.color.toString()===modelData?2:0; border.color: "#ffffff"
                                MouseArea { anchors.fill: parent; onClicked: editor.color=parent.modelData }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#363e47"; Layout.topMargin: 3 }
                    RowLayout {
                        Layout.fillWidth: true
                        PanelTitle { text: "CALQUES" }
                        Item { Layout.fillWidth: true }
                        Label { text: editor.layers.length; color: "#71808d"; font.pixelSize: 10 }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ComboBox { Layout.fillWidth: true; model: ["Normal","Produit","Écran","Incrustation","Obscurcir","Éclaircir","Différence"]; currentIndex: window.activeInfo.blend; onActivated: editor.setLayerBlend(editor.activeLayer,currentIndex) }
                        IconButton { glyph: activeInfo.locked?"▣":"▢"; selected: activeInfo.locked; hint: "Verrouiller / déverrouiller"; onClicked: editor.setLayerLocked(editor.activeLayer,!activeInfo.locked) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Opacité"; color: "#8e9baa"; font.pixelSize: 11 }
                        Slider {
                            id: layerOpacity; Layout.fillWidth: true; from: 0; to: 1
                            value: window.activeInfo.opacity
                            onPressedChanged: if(!pressed) editor.setLayerOpacity(editor.activeLayer,value)
                        }
                        Label { text: Math.round(layerOpacity.value*100)+" %"; color: "#b7c2cc"; font.pixelSize: 11; Layout.preferredWidth: 38 }
                    }
                    ListView {
                        id: layerList
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 100
                        clip: true; spacing: 5; model: editor.layers
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property var modelData
                            width: layerList.width; height: 57; radius: 5
                            color: modelData.layerIndex===editor.activeLayer ? "#35443e" : "#292f38"
                            border.color: modelData.layerIndex===editor.activeLayer ? "#567563" : "#343c46"
                            MouseArea {
                                anchors.fill: parent
                                onClicked: editor.activeLayer=modelData.layerIndex
                                onDoubleClicked: { editor.activeLayer=modelData.layerIndex; layerName.text=modelData.name; renameDialog.open() }
                            }
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 6; spacing: 7
                                IconButton { glyph: modelData.visible?"◉":"○"; implicitWidth: 24; hint: "Visibilité du calque"; onClicked: editor.setLayerVisible(modelData.layerIndex,!modelData.visible) }
                                Rectangle {
                                    width: 42; height: 36; color: "#182027"; radius: 3; border.color: "#506159"
                                    Image { anchors.fill: parent; anchors.margins: 2; source: "image://layers/"+modelData.layerIndex+"/"+editor.revision; sourceSize.width: 80; sourceSize.height: 60; fillMode: Image.PreserveAspectFit; cache: false }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 3
                                    Label { text: modelData.name; color: modelData.visible?"#d6e0db":"#7c8791"; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: 11 }
                                    Label { text: modelData.locked?"Verrouillé":"Calque de pixels"; color: "#80928c"; font.pixelSize: 9 }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 4
                        IconButton { glyph: "+"; hint: "Nouveau calque"; onClicked: editor.addLayer() }
                        IconButton { glyph: "⧉"; hint: "Dupliquer le calque"; onClicked: editor.duplicateLayer() }
                        IconButton { glyph: "↑"; hint: "Monter le calque"; enabled: editor.activeLayer<editor.layers.length-1; onClicked: editor.moveLayer(1) }
                        IconButton { glyph: "↓"; hint: "Descendre le calque"; enabled: editor.activeLayer>0; onClicked: editor.moveLayer(-1) }
                        Item { Layout.fillWidth: true }
                        IconButton { glyph: "×"; hint: "Supprimer le calque"; enabled: editor.layers.length>1; onClicked: editor.removeLayer() }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#363e47" }
                    PanelTitle { text: "HISTORIQUE" }
                    ListView {
                        Layout.fillWidth: true; Layout.preferredHeight: 65; clip: true
                        model: editor.history; currentIndex: editor.historyIndex
                        onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Label {
                            required property string modelData
                            required property int index
                            width: ListView.view.width; height: 26
                            text: (index===editor.historyIndex?"●  ":"    ")+modelData
                            color: index===editor.historyIndex?"#beeccb":index>editor.historyIndex?"#65707b":"#9ba8b6"
                            font.pixelSize: 11; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                        }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; height: 27; color: "#20262d"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 15; anchors.rightMargin: 15
                Rectangle { width: 5; height: 5; radius: 3; color: "#aad4b4" }
                Label { text: window.status; color: "#8c99a7"; font.pixelSize: 10 }
                Item { Layout.fillWidth: true }
                Label { text: ""+editor.width+" × "+editor.height+" px   ·   "+editor.layers.length+" calques   ·   PhotoCrap 0.1"; color: "#73818f"; font.pixelSize: 10 }
            }
        }
    }

    FileDialog {
        id: openDialog; title: window.importing ? "Importer comme calque" : "Ouvrir une image ou un projet"
        nameFilters: window.importing ? ["Images (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff)","Tous les fichiers (*)"] : ["Images et projets (*.pcr *.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff)","Tous les fichiers (*)"]
        onAccepted: { if(window.importing) editor.importLayer(selectedFile); else editor.openFile(selectedFile) }
    }
    FileDialog {
        id: saveDialog; title: "Enregistrer le projet avec ses calques"; fileMode: FileDialog.SaveFile
        nameFilters: ["Projet PhotoCrap (*.pcr)"]; defaultSuffix: "pcr"
        onAccepted: {
            if(editor.saveProject(selectedFile) && window.resumeAfterSave) { window.resumeAfterSave=false; window.perform(window.pendingAction) }
        }
        onRejected: window.resumeAfterSave=false
    }
    FileDialog {
        id: exportDialog; title: "Exporter l’image aplatie"; fileMode: FileDialog.SaveFile
        nameFilters: ["PNG (*.png)","JPEG (*.jpg *.jpeg)","BMP (*.bmp)","WebP (*.webp)"]
        defaultSuffix: selectedNameFilter.index===1?"jpg":selectedNameFilter.index===2?"bmp":selectedNameFilter.index===3?"webp":"png"
        onAccepted: editor.exportImage(selectedFile)
    }
    ColorDialog { id: colorDialog; title: "Choisir une couleur"; onAccepted: { if(changingSecondary) editor.secondaryColor=selectedColor; else editor.color=selectedColor } }
    Dialog {
        id: newDialog; objectName: "newDialog"; title: "Nouveau document"; anchors.centerIn: parent; modal: true; width: 410
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent; spacing: 16
            Label { text: "Une page blanche pour votre prochaine idée."; color: "#a5b2c0" }
            ComboBox {
                Layout.fillWidth: true; model: ["Personnalisé","Écran · 1920 × 1080","Carré · 1080 × 1080","Portrait · 1080 × 1350","A4 · 2480 × 3508"]
                onActivated: {
                    const sizes=[[1280,800],[1920,1080],[1080,1080],[1080,1350],[2480,3508]]
                    newWidth.value=sizes[currentIndex][0]; newHeight.value=sizes[currentIndex][1]
                }
            }
            RowLayout { Label { text: "Largeur (px)"; Layout.preferredWidth: 140 } SpinBox { id: newWidth; from: 1; to: 16384; value: 1280; editable: true; Layout.fillWidth: true } }
            RowLayout { Label { text: "Hauteur (px)"; Layout.preferredWidth: 140 } SpinBox { id: newHeight; from: 1; to: 16384; value: 800; editable: true; Layout.fillWidth: true } }
            CheckBox { id: transparent; text: "Arrière-plan transparent" }
            Label { text: "RVB · 8 bits · sRGB"; color: "#8794a2" }
        }
        onAccepted: editor.newDocument(newWidth.value,newHeight.value,transparent.checked)
    }
    Dialog {
        id: resizeDialog; title: "Dimensions de l’image"; anchors.centerIn: parent; modal: true; width: 370
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent; spacing: 16
            RowLayout { Label { text: "Largeur"; Layout.preferredWidth: 100 } SpinBox { id: resizeWidth; from: 1; to: 16384; editable: true } }
            RowLayout { Label { text: "Hauteur"; Layout.preferredWidth: 100 } SpinBox { id: resizeHeight; from: 1; to: 16384; editable: true } }
            Label { text: "Tous les calques sont rééchantillonnés.\nLes proportions sont libres."; color: "#9eabb7" }
        }
        onAccepted: editor.resizeImage(resizeWidth.value,resizeHeight.value)
    }
    Dialog {
        id: renameDialog; title: "Renommer le calque"; anchors.centerIn: parent; modal: true; width: 350
        standardButtons: Dialog.Ok | Dialog.Cancel
        TextField { id: layerName; width: parent.width; maximumLength: 256; selectByMouse: true; onAccepted: renameDialog.accept() }
        onOpened: { layerName.forceActiveFocus(); layerName.selectAll() }
        onAccepted: editor.renameLayer(editor.activeLayer,layerName.text)
    }
    Dialog {
        id: adjustmentDialog; title: window.adjustmentTitle; anchors.centerIn: parent; modal: true; width: 390
        standardButtons: Dialog.Apply | Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent; spacing: 16
            Label { text: "Appliquer au calque actif"; color: "#9cabb7" }
            RowLayout {
                Slider { id: adjustmentSlider; Layout.fillWidth: true; from: window.adjustment==="blur"?1:-100; to: window.adjustment==="blur"?30:100; stepSize: 1 }
                Label { text: Math.round(adjustmentSlider.value); Layout.preferredWidth: 35 }
            }
            Label { text: "La sélection limite la zone traitée.\nL’opération peut être annulée."; color: "#8996a4" }
        }
        onApplied: { editor.applyFilter(window.adjustment,adjustmentSlider.value); close() }
    }
    Dialog {
        id: preferencesDialog
        objectName: "preferencesDialog"
        title: "Réglages"
        anchors.centerIn: parent
        modal: true
        width: Math.min(820, window.width - 80)
        height: Math.min(680, window.height - 80)
        closePolicy: Popup.CloseOnEscape

        contentItem: RowLayout {
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 190
                Layout.fillHeight: true
                color: "#1d232a"
                border.color: "#343d47"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Label {
                        text: "RÉGLAGES"
                        color: "#71808e"
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 1.6
                        bottomPadding: 6
                    }
                    ItemDelegate {
                        Layout.fillWidth: true
                        highlighted: true
                        text: "⌨   Raccourcis clavier"
                        Accessible.name: "Raccourcis clavier"
                    }
                    Item { Layout.fillHeight: true }
                    Label {
                        Layout.fillWidth: true
                        text: "Les réglages sont enregistrés automatiquement."
                        color: "#71808e"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 22
                Layout.rightMargin: 14
                Layout.topMargin: 14
                spacing: 10

                Label {
                    text: "Raccourcis clavier"
                    color: "#eef2f5"
                    font.pixelSize: 20
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    text: "Cliquez dans un champ puis saisissez une combinaison comme Ctrl+Shift+E. Sur macOS, Ctrl correspond à la touche Commande. Videz un champ pour désactiver la commande."
                    color: "#95a2af"
                    wrapMode: Text.WordWrap
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#39424c" }

                ListView {
                    id: shortcutList
                    objectName: "shortcutList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 3
                    model: shortcuts.entries
                    section.property: "category"
                    section.criteria: ViewSection.FullString
                    ScrollBar.vertical: ScrollBar {}

                    section.delegate: Label {
                        required property string section
                        width: shortcutList.width
                        height: 34
                        text: section.toUpperCase()
                        color: "#7f958a"
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 1.4
                        verticalAlignment: Text.AlignBottom
                        bottomPadding: 6
                    }

                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: shortcutList.width
                        height: 42
                        radius: 4
                        color: shortcutField.activeFocus ? "#303b38" : index % 2 ? "#252b33" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 9
                            anchors.rightMargin: 5
                            spacing: 10

                            Label {
                                Layout.fillWidth: true
                                text: modelData.label
                                color: "#d3dbe2"
                                elide: Text.ElideRight
                            }
                            TextField {
                                id: shortcutField
                                objectName: "shortcut_" + modelData.id
                                Layout.preferredWidth: 175
                                text: modelData.sequence
                                placeholderText: "Aucun"
                                selectByMouse: true
                                Accessible.name: "Raccourci pour " + modelData.label
                                onEditingFinished: {
                                    if (text !== modelData.sequence && !shortcuts.setSequence(modelData.id, text))
                                        text = modelData.sequence
                                }
                            }
                            ToolButton {
                                text: "×"
                                enabled: modelData.sequence.length > 0
                                ToolTip.visible: hovered
                                ToolTip.text: "Désactiver ce raccourci"
                                onClicked: shortcuts.setSequence(modelData.id, "")
                            }
                        }
                    }
                }
            }
        }

        footer: DialogButtonBox {
            Button {
                text: "Valeurs par défaut"
                DialogButtonBox.buttonRole: DialogButtonBox.ResetRole
                onClicked: shortcuts.resetDefaults()
            }
            Button {
                text: "Fermer"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: preferencesDialog.close()
            }
        }
    }
    Dialog {
        id: unsavedDialog; objectName: "unsavedDialog"; title: "Enregistrer vos modifications ?"; anchors.centerIn: parent; modal: true; width: 450
        closePolicy: Popup.NoAutoClose
        Label { width: parent.width; text: "Le document « "+editor.name+" » contient des modifications non enregistrées."; wrapMode: Text.WordWrap }
        footer: DialogButtonBox {
            Button { text: "Annuler"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole; onClicked: unsavedDialog.close() }
            Button { text: "Abandonner"; DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole; onClicked: { unsavedDialog.close(); window.perform(window.pendingAction) } }
            Button { text: "Enregistrer"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: { unsavedDialog.close(); window.resumeAfterSave=true; window.save(false) } }
        }
    }
    Dialog {
        id: errorDialog; title: "Impossible de terminer l’opération"; anchors.centerIn: parent; modal: true; width: 460
        standardButtons: Dialog.Ok
        Label { id: errorLabel; width: parent.width; wrapMode: Text.WordWrap }
    }
    Dialog {
        id: aboutDialog; title: "PhotoCrap 0.1"; anchors.centerIn: parent; modal: true; width: 480; standardButtons: Dialog.Ok
        Label {
            width: parent.width; wrapMode: Text.WordWrap; lineHeight: 1.4
            text: "Un atelier de création d’images en C++ et Qt Quick.\n\nCalques, dessin, sélection, transformations et retouche, sur macOS, Linux et Windows.\n\nCette première version utilise des calques raster en sRGB 8 bits. Elle ne lit pas les PSD et n’inclut pas les objets dynamiques, les masques de calque, le RAW ou les fonctions IA.\n\nLogiciel indépendant, sans affiliation avec Adobe."
        }
    }
}
