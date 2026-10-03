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
    property string status: qsTr("Prêt à créer")
    property string pendingAction: ""
    property url pendingRecentFile: ""
    property bool allowClose: false
    property bool resumeAfterSave: false
    property bool importing: false
    property bool changingSecondary: false
    property bool spacePressed: false
    property bool layerContextMenuVisible: layerContextMenu.visible
    property real parameterSliderWidth: 0
    property int preferencesPage: 0
    property string interfaceLanguage: languages.language
    property string newActionText: newAction.text
    property int recentFileCount: recentFiles.entries.length
    property bool layerPreviewPending: false
    property string pendingLayerPreviewKey: ""
    property var pendingLayerPreviewValue
    property bool editingText: activeFocusItem !== null && typeof activeFocusItem.cursorPosition === "number"
    property bool modalOpen: newDialog.opened || resizeDialog.opened || adjustmentDialog.opened || renameDialog.opened || preferencesDialog.opened || unsavedDialog.opened || errorDialog.opened || aboutDialog.opened || colorDialog.visible || openDialog.visible || saveDialog.visible || exportDialog.visible
    property string adjustment: "brightness"
    property string adjustmentTitle: qsTr("Luminosité")
    property var blendModeNames: [qsTr("Normal"),qsTr("Fondu"),qsTr("Obscurcir"),qsTr("Produit"),qsTr("Densité couleur +"),qsTr("Densité linéaire +"),qsTr("Couleur plus foncée"),qsTr("Éclaircir"),qsTr("Écran"),qsTr("Densité couleur −"),qsTr("Densité linéaire − (Ajout)"),qsTr("Couleur plus claire"),qsTr("Incrustation"),qsTr("Lumière tamisée"),qsTr("Lumière crue"),qsTr("Lumière vive"),qsTr("Lumière linéaire"),qsTr("Lumière ponctuelle"),qsTr("Mélange maximal"),qsTr("Différence"),qsTr("Exclusion"),qsTr("Soustraction"),qsTr("Division"),qsTr("Teinte"),qsTr("Saturation"),qsTr("Couleur"),qsTr("Luminosité","blend mode")]
    property var adjustmentDefinitions: ({
        brightnessContrast: [{key:"brightness",label:qsTr("Luminosité"),from:-100,to:100,step:1},{key:"contrast",label:qsTr("Contraste"),from:-100,to:100,step:1}],
        levels: [{key:"inputBlack",label:qsTr("Point noir d’entrée"),from:0,to:254,step:1},{key:"inputWhite",label:qsTr("Point blanc d’entrée"),from:1,to:255,step:1},{key:"gamma",label:qsTr("Gamma"),from:0.1,to:3,step:0.05},{key:"outputBlack",label:qsTr("Noir de sortie"),from:0,to:255,step:1},{key:"outputWhite",label:qsTr("Blanc de sortie"),from:0,to:255,step:1}],
        curves: [{key:"shadows",label:qsTr("Ombres"),from:-100,to:100,step:1},{key:"midtones",label:qsTr("Tons moyens"),from:-100,to:100,step:1},{key:"highlights",label:qsTr("Hautes lumières"),from:-100,to:100,step:1}],
        exposure: [{key:"exposure",label:qsTr("Exposition"),from:-5,to:5,step:0.1},{key:"offset",label:qsTr("Décalage"),from:-0.5,to:0.5,step:0.01},{key:"gamma",label:qsTr("Gamma"),from:0.1,to:3,step:0.05}],
        vibrance: [{key:"vibrance",label:qsTr("Vibrance"),from:-100,to:100,step:1},{key:"saturation",label:qsTr("Saturation"),from:-100,to:100,step:1}],
        hueSaturation: [{key:"hue",label:qsTr("Teinte"),from:-180,to:180,step:1},{key:"saturation",label:qsTr("Saturation"),from:-100,to:100,step:1},{key:"lightness",label:qsTr("Luminosité"),from:-100,to:100,step:1}],
        colorBalance: [{key:"cyanRed",label:qsTr("Cyan ↔ Rouge"),from:-100,to:100,step:1},{key:"magentaGreen",label:qsTr("Magenta ↔ Vert"),from:-100,to:100,step:1},{key:"yellowBlue",label:qsTr("Jaune ↔ Bleu"),from:-100,to:100,step:1}],
        blackWhite: [{key:"reds",label:qsTr("Rouges"),from:0,to:100,step:1},{key:"greens",label:qsTr("Verts"),from:0,to:100,step:1},{key:"blues",label:qsTr("Bleus"),from:0,to:100,step:1}],
        photoFilter: [{key:"hue",label:qsTr("Teinte du filtre"),from:0,to:360,step:1},{key:"density",label:qsTr("Densité"),from:0,to:100,step:1}],
        channelMixer: [{key:"redRed",label:qsTr("Rouge · R"),from:-200,to:200,step:1},{key:"redGreen",label:qsTr("Rouge · V"),from:-200,to:200,step:1},{key:"redBlue",label:qsTr("Rouge · B"),from:-200,to:200,step:1},{key:"greenRed",label:qsTr("Vert · R"),from:-200,to:200,step:1},{key:"greenGreen",label:qsTr("Vert · V"),from:-200,to:200,step:1},{key:"greenBlue",label:qsTr("Vert · B"),from:-200,to:200,step:1},{key:"blueRed",label:qsTr("Bleu · R"),from:-200,to:200,step:1},{key:"blueGreen",label:qsTr("Bleu · V"),from:-200,to:200,step:1},{key:"blueBlue",label:qsTr("Bleu · B"),from:-200,to:200,step:1}],
        colorLookup: [{key:"preset",label:qsTr("Style (0 chaud, 1 cinéma, 2 froid, 3 mat)"),from:0,to:3,step:1}],
        invert: [],
        posterize: [{key:"levels",label:qsTr("Niveaux"),from:2,to:32,step:1}],
        threshold: [{key:"level",label:qsTr("Seuil"),from:0,to:255,step:1}],
        gradientMap: [],
        selectiveColor: [{key:"cyanRed",label:qsTr("Cyan ↔ Rouge"),from:-100,to:100,step:1},{key:"magentaGreen",label:qsTr("Magenta ↔ Vert"),from:-100,to:100,step:1},{key:"yellowBlue",label:qsTr("Jaune ↔ Bleu"),from:-100,to:100,step:1},{key:"black",label:qsTr("Noir"),from:-100,to:100,step:1}],
        gaussianBlur: [{key:"radius",label:qsTr("Rayon"),from:0.1,to:100,step:0.5}],
        motionBlur: [{key:"distance",label:qsTr("Distance"),from:1,to:200,step:1},{key:"angle",label:qsTr("Angle"),from:-180,to:180,step:1}],
        boxBlur: [{key:"radius",label:qsTr("Rayon"),from:1,to:100,step:1}],
        unsharpMask: [{key:"amount",label:qsTr("Gain (%)"),from:0,to:300,step:1},{key:"radius",label:qsTr("Rayon"),from:0.5,to:20,step:0.5},{key:"threshold",label:qsTr("Seuil"),from:0,to:255,step:1}],
        highPass: [{key:"radius",label:qsTr("Rayon"),from:0.5,to:100,step:0.5}],
        mosaic: [{key:"cellSize",label:qsTr("Taille des cellules"),from:2,to:200,step:1}],
        noise: [{key:"amount",label:qsTr("Quantité (%)"),from:0,to:100,step:1}]
    })
    property var tools: [
        {name:"move", icon:"↖", label:qsTr("Déplacement"), shortcut:"toolMove"},
        {name:"select", icon:"⬚", label:qsTr("Sélection rectangulaire"), shortcut:"toolSelect"},
        {name:"crop", icon:"⌗", label:qsTr("Recadrage"), shortcut:"toolCrop"},
        {name:"brush", icon:"╱", label:qsTr("Pinceau"), shortcut:"toolBrush"},
        {name:"eraser", icon:"▱", label:qsTr("Gomme"), shortcut:"toolEraser"},
        {name:"fill", icon:"◩", label:qsTr("Remplissage contigu"), shortcut:"toolFill"},
        {name:"gradient", icon:"▨", label:qsTr("Dégradé"), shortcut:"toolGradient"},
        {name:"picker", icon:"⌖", label:qsTr("Pipette"), shortcut:"toolPicker"},
        {name:"text", icon:"T", label:qsTr("Texte rasterisé"), shortcut:"toolText"},
        {name:"rectangle", icon:"□", label:qsTr("Rectangle"), shortcut:"toolRectangle"},
        {name:"ellipse", icon:"○", label:qsTr("Ellipse"), shortcut:"toolEllipse"},
        {name:"line", icon:"╲", label:qsTr("Ligne"), shortcut:"toolLine"},
        {name:"hand", icon:"✥", label:qsTr("Navigation"), shortcut:"toolHand"}
    ]
    property var activeInfo: {
        const list = editor.layers
        for (let i=0; i<list.length; ++i) if (list[i].layerIndex === editor.activeLayer) return list[i]
        return {name:"", opacity:1, blend:0, locked:false, kind:"pixel", parameters:({}), hasMask:false, maskEnabled:true, clippingMask:false, groupId:"", groupCollapsed:false, selected:false}
    }
    property string toolName: {
        for (let i=0; i<tools.length; ++i) if (tools[i].name === editor.tool) return tools[i].label
        return qsTr("Pinceau")
    }
    function fitCanvas() {
        zoom = Math.max(0.02, Math.min(1, (viewport.width-110)/editor.width, (viewport.height-110)/editor.height))
        centerCanvas()
    }
    function setZoom(value) { zoom = Math.max(0.02, Math.min(4, value)) }
    function centerCanvas() {
        viewport.contentX = Math.max(0, (viewport.contentWidth-viewport.width)/2)
        viewport.contentY = Math.max(0, (viewport.contentHeight-viewport.height)/2)
    }
    function clampScroll(value, extent) { return Math.max(0, Math.min(value, Math.max(0, extent))) }
    function panCanvas(dx, dy) {
        viewport.contentX = clampScroll(viewport.contentX+dx, viewport.contentWidth-viewport.width)
        viewport.contentY = clampScroll(viewport.contentY+dy, viewport.contentHeight-viewport.height)
    }
    function zoomAt(value, pointerX, pointerY) {
        const oldZoom=zoom
        const documentX=(viewport.contentX+pointerX-artboard.x)/oldZoom
        const documentY=(viewport.contentY+pointerY-artboard.y)/oldZoom
        setZoom(value)
        viewport.contentX=clampScroll(artboard.x+documentX*zoom-pointerX,viewport.contentWidth-viewport.width)
        viewport.contentY=clampScroll(artboard.y+documentY*zoom-pointerY,viewport.contentHeight-viewport.height)
    }
    function perform(action) {
        if (action === "new") newDialog.open()
        else if (action === "open") { importing = false; openDialog.open() }
        else if (action === "demo") { editor.createDemo(); fitCanvas() }
        else if (action === "recent") openRecentFile(pendingRecentFile)
        else if (action === "close") { allowClose = true; close() }
    }
    function guard(action) {
        editor.endStroke()
        pendingAction = action
        if (editor.modified) unsavedDialog.open()
        else perform(action)
    }
    function guardRecent(url) {
        pendingRecentFile=url
        guard("recent")
    }
    function openRecentFile(url) {
        if(editor.openFile(url)) recentFiles.add(url)
        else recentFiles.remove(url)
    }
    function compactRecentPath(path) {
        const limit=36
        if(path.length<=limit) return path
        return path.substring(0,16)+"…"+path.substring(path.length-19)
    }
    function save(asNew) {
        if (!asNew && editor.projectUrl.toString().length) {
            if (editor.saveProject(editor.projectUrl)) {
                recentFiles.add(editor.projectUrl)
                if(resumeAfterSave) {
                    resumeAfterSave = false
                    perform(pendingAction)
                }
            }
        } else saveDialog.open()
    }
    function adjust(kind, label, initial) {
        adjustment = kind; adjustmentTitle = label; adjustmentSlider.value = initial
        adjustmentDialog.open()
    }
    function addAdjustmentLayer(type) {
        editor.addAdjustmentLayer(type)
        layerPropertiesDialog.open()
    }
    function queueLayerPreview(key, value) {
        if(!layerPreview.checked)return
        pendingLayerPreviewKey = key
        pendingLayerPreviewValue = value
        layerPreviewPending = true
        if(!layerPreviewTimer.running)layerPreviewTimer.start()
    }
    function flushLayerPreview() {
        if(!layerPreviewPending)return
        const key = pendingLayerPreviewKey
        const value = pendingLayerPreviewValue
        layerPreviewPending = false
        if(layerPreview.checked)editor.previewLayerParameter(key,value)
    }
    function cancelLayerPreviewQueue() {
        layerPreviewTimer.stop()
        layerPreviewPending = false
    }
    function previewLayerValue(key, value, finalValue) {
        if(finalValue) {
            cancelLayerPreviewQueue()
            editor.previewLayerParameter(key,value)
            editor.finishLayerPreview(qsTr("Modifier %1").arg(activeInfo.name))
        } else if(layerPreview.checked) queueLayerPreview(key,value)
    }
    function openLayerContextMenu() { layerContextMenu.popup() }
    function closeLayerContextMenu() { layerContextMenu.close() }
    function setInterfaceLanguage(code) { languages.language=code }
    onClosing: function(event) {
        if (!allowClose && editor.modified) { event.accepted = false; guard("close") }
    }
    Component.onCompleted: Qt.callLater(fitCanvas)
    Connections {
        target: editor
        function onErrorOccurred(message) { errorLabel.text = message; errorDialog.open() }
        function onStatusMessage(message) { window.status = message; statusTimer.restart() }
        function onCanvasSizeChanged() { Qt.callLater(window.fitCanvas) }
    }
    Connections {
        target: shortcuts
        function onErrorOccurred(message) { errorLabel.text = message; errorDialog.open() }
    }
    Timer { id: statusTimer; interval: 5000; onTriggered: window.status = qsTr("Prêt à créer") }
    Timer { id: layerPreviewTimer; interval: 16; onTriggered: window.flushLayerPreview() }

    Action { id: newAction; text: qsTr("Nouveau…"); shortcut: (shortcuts.revision, shortcuts.sequence("new")); enabled: !window.modalOpen; onTriggered: guard("new") }
    Action { id: openAction; text: qsTr("Ouvrir…"); shortcut: (shortcuts.revision, shortcuts.sequence("open")); enabled: !window.modalOpen; onTriggered: guard("open") }
    Action { id: saveAction; text: qsTr("Enregistrer le projet"); shortcut: (shortcuts.revision, shortcuts.sequence("save")); enabled: !window.modalOpen; onTriggered: { resumeAfterSave=false; save(false) } }
    Action { id: saveAsAction; text: qsTr("Enregistrer sous…"); shortcut: (shortcuts.revision, shortcuts.sequence("saveAs")); enabled: !window.modalOpen; onTriggered: { resumeAfterSave=false; save(true) } }
    Action { id: exportAction; text: qsTr("Exporter l’image…"); shortcut: (shortcuts.revision, shortcuts.sequence("export")); enabled: !window.modalOpen; onTriggered: exportDialog.open() }
    Action { id: importAction; text: qsTr("Importer comme calque…"); shortcut: (shortcuts.revision, shortcuts.sequence("import")); enabled: !window.modalOpen; onTriggered: { importing=true; openDialog.open() } }
    Action { id: undoAction; text: qsTr("Annuler"); shortcut: (shortcuts.revision, shortcuts.sequence("undo")); enabled: editor.canUndo && !window.modalOpen; onTriggered: editor.undo() }
    Action { id: redoAction; text: qsTr("Rétablir"); shortcut: (shortcuts.revision, shortcuts.sequence("redo")); enabled: editor.canRedo && !window.modalOpen; onTriggered: editor.redo() }
    Action { id: copyAction; text: qsTr("Copier l’image visible"); shortcut: (shortcuts.revision, shortcuts.sequence("copy")); enabled: !window.editingText && !window.modalOpen; onTriggered: editor.copy() }
    Action { id: pasteAction; text: qsTr("Coller comme calque"); shortcut: (shortcuts.revision, shortcuts.sequence("paste")); enabled: !window.editingText && !window.modalOpen; onTriggered: editor.paste() }
    Action { id: selectAction; text: qsTr("Tout sélectionner"); shortcut: (shortcuts.revision, shortcuts.sequence("selectAll")); enabled: !window.editingText && !window.modalOpen; onTriggered: editor.selectAll() }
    Action { id: deselectAction; text: qsTr("Désélectionner"); shortcut: (shortcuts.revision, shortcuts.sequence("deselect")); enabled: !window.editingText && !window.modalOpen; onTriggered: editor.deselect() }
    Action { id: deleteAction; text: qsTr("Effacer les pixels"); shortcut: (shortcuts.revision, shortcuts.sequence("clear")); enabled: !window.editingText && !window.modalOpen; onTriggered: editor.clearSelected() }
    Action { id: addAction; text: qsTr("Nouveau calque"); shortcut: (shortcuts.revision, shortcuts.sequence("newLayer")); enabled: !window.modalOpen; onTriggered: editor.addLayer() }
    Action { id: duplicateAction; text: qsTr("Dupliquer le calque"); shortcut: (shortcuts.revision, shortcuts.sequence("duplicateLayer")); enabled: !window.modalOpen; onTriggered: editor.duplicateLayer() }
    Action { id: preferencesAction; text: qsTr("Réglages…"); shortcut: (shortcuts.revision, shortcuts.sequence("preferences")); enabled: !window.modalOpen; onTriggered: preferencesDialog.open() }
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
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("brushSofter")); onActivated: editor.brushHardness = editor.brushHardness-0.1 }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("brushHarder")); onActivated: editor.brushHardness = editor.brushHardness+0.1 }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("defaultColors")); onActivated: editor.resetColors() }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("swapColors")); onActivated: editor.swapColors() }

    menuBar: MenuBar {
        background: Rectangle { color: "#22272e" }
        delegate: MenuBarItem {
            id: menuBarItem
            contentItem: Text {
                text: menuBarItem.text
                color: menuBarItem.highlighted || menuBarItem.down ? "#ffffff" : "#dbe1e8"
                font: menuBarItem.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: menuBarItem.highlighted || menuBarItem.down
                       ? "#2866c8"
                       : menuBarItem.hovered ? "#343d48" : "transparent"
            }
        }
        DarkMenu {
            title: qsTr("Fichier")
            DarkMenuItem { action: newAction }
            DarkMenuItem { action: openAction }
            DarkMenu {
                id: recentFilesMenu
                objectName: "recentFilesMenu"
                title: qsTr("Ouvrir les fichiers récents")
                implicitWidth: 300
                width: implicitWidth
                enabled: recentFiles.entries.length>0
                onAboutToShow: recentFiles.refresh()
                Instantiator {
                    model: recentFiles.entries
                    delegate: DarkMenuItem {
                        id: recentFileEntry
                        required property var modelData
                        objectName: "recentFileItem"
                        implicitWidth: 300
                        width: recentFilesMenu.width
                        text: window.compactRecentPath(modelData.path)
                        contentItem: Label {
                            objectName: "recentFilePathLabel"
                            text: recentFileEntry.text
                            color: recentFileEntry.highlighted ? "#ffffff" : "#dbe1e8"
                            font: recentFileEntry.font
                            verticalAlignment: Text.AlignVCenter
                            maximumLineCount: 1
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: modelData.path
                        ToolTip.delay: 500
                        onTriggered: window.guardRecent(modelData.url)
                    }
                    onObjectAdded: function(index, object) { recentFilesMenu.insertItem(index,object) }
                    onObjectRemoved: function(index, object) { recentFilesMenu.removeItem(object) }
                }
                MenuSeparator { visible: recentFiles.entries.length>0 }
                DarkMenuItem { text: qsTr("Effacer la liste des fichiers récents"); enabled: recentFiles.entries.length>0; onTriggered: recentFiles.clear() }
            }
            DarkMenuItem { action: importAction }
            MenuSeparator {}
            DarkMenuItem { action: saveAction }
            DarkMenuItem { action: saveAsAction }
            DarkMenuItem { action: exportAction }
            MenuSeparator {}
            DarkMenuItem { text: qsTr("Ouvrir la composition de démonstration"); onTriggered: guard("demo") }
            DarkMenuItem { text: qsTr("Quitter"); onTriggered: guard("close") }
        }
        DarkMenu {
            title: qsTr("Édition")
            DarkMenuItem { action: undoAction }
            DarkMenuItem { action: redoAction }
            MenuSeparator {}
            DarkMenuItem { action: copyAction }
            DarkMenuItem { action: pasteAction }
            DarkMenuItem { action: deleteAction }
            MenuSeparator {}
            DarkMenuItem { action: preferencesAction }
        }
        DarkMenu {
            title: qsTr("Image")
            DarkMenuItem { text: qsTr("Dimensions de l’image…"); onTriggered: { resizeWidth.value=editor.width; resizeHeight.value=editor.height; resizeDialog.open() } }
            DarkMenuItem { text: qsTr("Recadrer selon la sélection"); enabled: editor.hasSelection; onTriggered: editor.cropToSelection() }
            MenuSeparator {}
            DarkMenuItem { text: qsTr("Rotation de 90° à droite"); onTriggered: editor.transformImage("rotateRight") }
            DarkMenuItem { text: qsTr("Rotation de 90° à gauche"); onTriggered: editor.transformImage("rotateLeft") }
            DarkMenuItem { text: qsTr("Symétrie horizontale"); onTriggered: editor.transformImage("flipH") }
            DarkMenuItem { text: qsTr("Symétrie verticale"); onTriggered: editor.transformImage("flipV") }
        }
        DarkMenu {
            title: qsTr("Calque")
            DarkMenuItem { action: addAction }
            DarkMenuItem { action: duplicateAction }
            DarkMenu {
                title: qsTr("Nouveau calque de réglage")
                DarkMenuItem { text: qsTr("Luminosité / Contraste"); onTriggered: addAdjustmentLayer("brightnessContrast") }
                DarkMenuItem { text: qsTr("Niveaux"); onTriggered: addAdjustmentLayer("levels") }
                DarkMenuItem { text: qsTr("Courbes"); onTriggered: addAdjustmentLayer("curves") }
                DarkMenuItem { text: qsTr("Exposition"); onTriggered: addAdjustmentLayer("exposure") }
                DarkMenuItem { text: qsTr("Vibrance"); onTriggered: addAdjustmentLayer("vibrance") }
                DarkMenuItem { text: qsTr("Teinte / Saturation"); onTriggered: addAdjustmentLayer("hueSaturation") }
                DarkMenuItem { text: qsTr("Balance des couleurs"); onTriggered: addAdjustmentLayer("colorBalance") }
                DarkMenuItem { text: qsTr("Noir et blanc"); onTriggered: addAdjustmentLayer("blackWhite") }
                DarkMenuItem { text: qsTr("Filtre photo"); onTriggered: addAdjustmentLayer("photoFilter") }
                DarkMenuItem { text: qsTr("Mélangeur de couches"); onTriggered: addAdjustmentLayer("channelMixer") }
                DarkMenuItem { text: qsTr("Recherche de couleurs"); onTriggered: addAdjustmentLayer("colorLookup") }
                DarkMenuItem { text: qsTr("Négatif"); onTriggered: addAdjustmentLayer("invert") }
                DarkMenuItem { text: qsTr("Isohélie"); onTriggered: addAdjustmentLayer("posterize") }
                DarkMenuItem { text: qsTr("Seuil"); onTriggered: addAdjustmentLayer("threshold") }
                DarkMenuItem { text: qsTr("Courbe de transfert de dégradé"); onTriggered: addAdjustmentLayer("gradientMap") }
                DarkMenuItem { text: qsTr("Correction sélective"); onTriggered: addAdjustmentLayer("selectiveColor") }
                MenuSeparator {}
                DarkMenu {
                    title: qsTr("Filtres")
                    DarkMenuItem { text: qsTr("Flou gaussien…"); onTriggered: addAdjustmentLayer("gaussianBlur") }
                    DarkMenuItem { text: qsTr("Flou directionnel…"); onTriggered: addAdjustmentLayer("motionBlur") }
                    DarkMenuItem { text: qsTr("Flou de boîte…"); onTriggered: addAdjustmentLayer("boxBlur") }
                    MenuSeparator {}
                    DarkMenuItem { text: qsTr("Netteté optimisée…"); onTriggered: addAdjustmentLayer("unsharpMask") }
                    DarkMenuItem { text: qsTr("Passe-haut…"); onTriggered: addAdjustmentLayer("highPass") }
                    MenuSeparator {}
                    DarkMenuItem { text: qsTr("Mosaïque…"); onTriggered: addAdjustmentLayer("mosaic") }
                    DarkMenuItem { text: qsTr("Ajout de bruit…"); onTriggered: addAdjustmentLayer("noise") }
                }
            }
            DarkMenuItem { text: qsTr("Séparation de fréquences…"); onTriggered: { editor.addFrequencySeparation(8); layerPropertiesDialog.open() } }
            MenuSeparator {}
            DarkMenu {
                title: qsTr("Masque de fusion")
                DarkMenuItem { text: qsTr("Tout faire apparaître"); enabled: !activeInfo.hasMask; onTriggered: editor.addLayerMask(true) }
                DarkMenuItem { text: qsTr("Tout masquer"); enabled: !activeInfo.hasMask; onTriggered: editor.addLayerMask(false) }
                DarkMenuItem { text: activeInfo.maskEnabled ? qsTr("Désactiver le masque") : qsTr("Activer le masque"); enabled: activeInfo.hasMask; onTriggered: editor.setMaskEnabled(!activeInfo.maskEnabled) }
                DarkMenuItem { text: qsTr("Inverser le masque"); enabled: activeInfo.hasMask; onTriggered: editor.invertLayerMask() }
                DarkMenuItem { text: qsTr("Supprimer le masque"); enabled: activeInfo.hasMask; onTriggered: editor.removeLayerMask() }
            }
            DarkMenuItem { text: qsTr("Renommer…"); onTriggered: { layerName.text=activeInfo.name; renameDialog.open() } }
            DarkMenuItem { text: qsTr("Propriétés…"); enabled: activeInfo.kind!=="pixel"; onTriggered: layerPropertiesDialog.open() }
            DarkMenuItem { text: qsTr("Fusionner vers le bas"); enabled: editor.activeLayer>0; onTriggered: editor.mergeDown() }
            DarkMenuItem { text: qsTr("Supprimer le calque"); enabled: editor.layers.length>1; onTriggered: editor.removeLayer() }
        }
        DarkMenu {
            title: qsTr("Sélection")
            DarkMenuItem { action: selectAction }
            DarkMenuItem { action: deselectAction }
        }
        DarkMenu {
            title: qsTr("Réglages","image adjustments")
            DarkMenuItem { text: qsTr("Luminosité…"); onTriggered: adjust("brightness",qsTr("Luminosité"),0) }
            DarkMenuItem { text: qsTr("Contraste…"); onTriggered: adjust("contrast",qsTr("Contraste"),0) }
            DarkMenuItem { text: qsTr("Saturation…"); onTriggered: adjust("saturation",qsTr("Saturation"),0) }
            MenuSeparator {}
            DarkMenuItem { text: qsTr("Noir et blanc"); onTriggered: editor.applyFilter("grayscale") }
            DarkMenuItem { text: qsTr("Négatif"); onTriggered: editor.applyFilter("invert") }
            DarkMenuItem { text: qsTr("Sépia"); onTriggered: editor.applyFilter("sepia") }
            DarkMenuItem { text: qsTr("Renforcer la netteté"); onTriggered: editor.applyFilter("sharpen") }
            MenuSeparator {}
            DarkMenu {
                title: qsTr("Filtres")
                DarkMenuItem { text: qsTr("Flou gaussien…"); onTriggered: addAdjustmentLayer("gaussianBlur") }
                DarkMenuItem { text: qsTr("Flou directionnel…"); onTriggered: addAdjustmentLayer("motionBlur") }
                DarkMenuItem { text: qsTr("Flou de boîte…"); onTriggered: addAdjustmentLayer("boxBlur") }
                MenuSeparator {}
                DarkMenuItem { text: qsTr("Netteté optimisée…"); onTriggered: addAdjustmentLayer("unsharpMask") }
                DarkMenuItem { text: qsTr("Passe-haut…"); onTriggered: addAdjustmentLayer("highPass") }
                MenuSeparator {}
                DarkMenuItem { text: qsTr("Mosaïque…"); onTriggered: addAdjustmentLayer("mosaic") }
                DarkMenuItem { text: qsTr("Ajout de bruit…"); onTriggered: addAdjustmentLayer("noise") }
            }
        }
        DarkMenu {
            title: qsTr("Affichage")
            DarkMenuItem { text: qsTr("Paramètres de pinceau…"); onTriggered: brushSettingsDialog.open() }
            MenuSeparator {}
            DarkMenuItem { text: qsTr("Adapter à la fenêtre"); onTriggered: fitCanvas() }
            DarkMenuItem { text: qsTr("Taille réelle · 100 %"); onTriggered: setZoom(1) }
            DarkMenuItem { text: qsTr("Zoom avant"); onTriggered: setZoom(zoom*1.25) }
            DarkMenuItem { text: qsTr("Zoom arrière"); onTriggered: setZoom(zoom/1.25) }
        }
        DarkMenu {
            title: qsTr("Aide")
            DarkMenuItem { text: qsTr("Raccourcis clavier…"); onTriggered: preferencesDialog.open() }
            DarkMenuItem { text: qsTr("À propos de PhotoCrap"); onTriggered: aboutDialog.open() }
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
                    Text { anchors.centerIn: parent; text: qsTr("P"); color: "#1d342b"; font.bold: true; font.pixelSize: 24 }
                }
                Column {
                    spacing: 3
                    Label { text: qsTr("PhotoCrap"); font.pixelSize: 16; font.bold: true; color: "#f0f3f6" }
                    Label { text: qsTr("L’ATELIER NUMÉRIQUE"); font.pixelSize: 8; font.letterSpacing: 2; color: "#87929e" }
                }
                Rectangle { Layout.leftMargin: 10; width: 1; height: 28; color: "#3a414a" }
                Label { visible: window.width>=1250; text: window.toolName; color: "#c8d0d9"; Layout.preferredWidth: 152 }
                Label { text: qsTr("Taille"); color: "#87929e" }
                DarkSpinBox { from: 1; to: 500; value: Math.round(editor.brushSize); suffix: " px"; implicitWidth: 110; onValueModified: editor.brushSize=value }
                Label { visible: editor.tool==="brush" || editor.tool==="eraser"; text: qsTr("Dureté"); color: "#87929e"; Layout.leftMargin: 4 }
                DarkSpinBox { objectName: "brushHardnessTop"; visible: editor.tool==="brush" || editor.tool==="eraser"; from: 0; to: 100; value: Math.round(editor.brushHardness*100); suffix: " %"; implicitWidth: 100; onValueModified: editor.brushHardness=value/100 }
                Label { text: qsTr("Opacité"); color: "#87929e"; Layout.leftMargin: 8 }
                DarkSpinBox { from: 1; to: 100; value: Math.round(editor.brushOpacity*100); suffix: " %"; implicitWidth: 100; onValueModified: editor.brushOpacity=value/100 }
                Label { visible: editor.tool==="brush" || editor.tool==="eraser"; text: qsTr("Débit"); color: "#87929e"; Layout.leftMargin: 4 }
                DarkSpinBox { visible: editor.tool==="brush" || editor.tool==="eraser"; from: 1; to: 100; value: Math.round(editor.brushFlow*100); suffix: " %"; implicitWidth: 95; onValueModified: editor.brushFlow=value/100 }
                Button { visible: editor.tool==="brush" || editor.tool==="eraser"; text: qsTr("Pinceau…"); onClicked: brushSettingsDialog.open() }
                CheckBox { text: qsTr("Remplir"); checked: editor.filledShapes; visible: editor.tool==="rectangle" || editor.tool==="ellipse"; onToggled: editor.filledShapes=checked }
                Button { text: qsTr("Recadrer"); visible: editor.tool==="crop"; enabled: editor.hasSelection; onClicked: editor.cropToSelection() }
                Item { Layout.fillWidth: true }
                IconButton { glyph: "↶"; hint: qsTr("Annuler"); enabled: editor.canUndo; onClicked: editor.undo() }
                IconButton { glyph: "↷"; hint: qsTr("Rétablir"); enabled: editor.canRedo; onClicked: editor.redo() }
                Button {
                    text: qsTr("Exporter ↗"); implicitWidth: 116
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
                        Label { text: qsTr("@ ") + Math.round(zoom*100) + " %"; color: "#73808e" }
                        Item { Layout.fillWidth: true }
                        Label { text: qsTr("RVB / 8"); color: "#73808e"; font.pixelSize: 10 }
                        IconButton { glyph: "+"; hint: qsTr("Nouveau document"); implicitWidth: 27; onClicked: guard("new") }
                    }
                }
                Item {
                    id: workspace
                    Layout.fillWidth: true; Layout.fillHeight: true; focus: true
                    Keys.priority: Keys.BeforeItem
                    Keys.onPressed: function(event) {
                        if(event.key===Qt.Key_Space && !event.isAutoRepeat && !window.editingText && !window.modalOpen) {
                            window.spacePressed=true;event.accepted=true
                        }
                    }
                    Keys.onReleased: function(event) {
                        if(event.key===Qt.Key_Space && !event.isAutoRepeat) {
                            window.spacePressed=false;event.accepted=true
                        }
                    }
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
                        objectName: "viewport"
                        anchors.fill: parent; anchors.leftMargin: 24; anchors.topMargin: 24
                        clip: true; boundsBehavior: Flickable.StopAtBounds
                        contentWidth: artboard.width+width
                        contentHeight: artboard.height+height
                        interactive: false
                        onContentXChanged: horizontalRuler.requestPaint()
                        onContentYChanged: verticalRuler.requestPaint()
                        onWidthChanged: horizontalRuler.requestPaint()
                        onHeightChanged: verticalRuler.requestPaint()
                        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        Item {
                            id: artboard
                            objectName: "artboard"
                            x: (viewport.contentWidth-width)/2; y: (viewport.contentHeight-height)/2
                            width: editor.width*zoom; height: editor.height*zoom
                            onXChanged: horizontalRuler.requestPaint()
                            onYChanged: verticalRuler.requestPaint()
                            Rectangle { x: 5; y: 9; width: parent.width; height: parent.height; color: "#0c1015"; opacity: 0.6 }
                            ImageCanvas {
                                anchors.fill: parent
                                document: editor
                                Component.onCompleted: editor.setPreviewSize(Math.round(width), Math.round(height))
                                onWidthChanged: editor.setPreviewSize(Math.round(width), Math.round(height))
                                onHeightChanged: editor.setPreviewSize(Math.round(width), Math.round(height))
                            }
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
                                cursorShape: editor.tool==="move" ? Qt.SizeAllCursor : (editor.tool==="brush" || editor.tool==="eraser") ? Qt.BlankCursor : Qt.CrossCursor
                                onPressed: function(mouse) { workspace.forceActiveFocus(); editor.beginStroke(mouse.x/zoom,mouse.y/zoom) }
                                onPositionChanged: function(mouse) {
                                    window.status = "X " + Math.round(mouse.x/zoom) + "   Y " + Math.round(mouse.y/zoom)
                                    brushCursor.pointerX=mouse.x;brushCursor.pointerY=mouse.y;brushCursor.requestPaint()
                                    if(pressed) editor.continueStroke(mouse.x/zoom,mouse.y/zoom)
                                }
                                onReleased: editor.endStroke()
                                onCanceled: editor.cancelStroke()
                            }
                            Canvas {
                                id: brushCursor
                                property real pointerX: 0
                                property real pointerY: 0
                                visible: drawingArea.containsMouse && (editor.tool==="brush" || editor.tool==="eraser")
                                enabled: false; z: 5
                                width: Math.max(8,editor.brushSize*zoom+8); height: width
                                x: pointerX-width/2; y: pointerY-height/2
                                onPaint: {
                                    const c=getContext("2d");c.reset();c.clearRect(0,0,width,height);c.translate(width/2,height/2)
                                    c.rotate(editor.brushAngle*Math.PI/180);c.scale(1,editor.brushRoundness)
                                    const radius=editor.brushSize*zoom/2;c.beginPath();c.arc(0,0,radius,0,Math.PI*2)
                                    c.lineWidth=2/editor.brushRoundness;c.strokeStyle="rgba(0,0,0,.75)";c.stroke()
                                    c.lineWidth=1/editor.brushRoundness;c.strokeStyle="rgba(255,255,255,.95)";c.stroke()
                                }
                                Connections { target: editor; function onSettingsChanged() { brushCursor.requestPaint() } }
                                Connections { target: window; function onZoomChanged() { brushCursor.requestPaint() } }
                            }
                        }
                    }
                    MouseArea {
                        id: wheelArea
                        anchors.fill: viewport
                        z: 20
                        acceptedButtons: Qt.NoButton
                        onWheel: function(wheel) {
                            const pixelY=wheel.pixelDelta.y
                            const pixelX=wheel.pixelDelta.x
                            const vertical=pixelY!==0 ? pixelY : wheel.angleDelta.y/2
                            const horizontal=pixelX!==0 ? pixelX : wheel.angleDelta.x/2
                            const primaryModifier=wheel.modifiers&(Qt.ControlModifier|Qt.MetaModifier)
                            if(primaryModifier) {
                                const steps=wheel.angleDelta.y!==0 ? wheel.angleDelta.y/120 : vertical/40
                                zoomAt(zoom*Math.pow(1.15,steps),wheel.x,wheel.y)
                            } else if(wheel.modifiers&Qt.ShiftModifier) {
                                panCanvas(-(vertical!==0?vertical:horizontal),0)
                            } else {
                                panCanvas(-horizontal,-vertical)
                            }
                            wheel.accepted=true
                        }
                    }
                    MouseArea {
                        id: panArea
                        objectName: "panArea"
                        anchors.fill: viewport
                        z: 21
                        enabled: window.spacePressed || editor.tool==="hand"
                        hoverEnabled: enabled
                        preventStealing: true
                        cursorShape: enabled
                                     ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor)
                                     : (drawingArea.containsMouse ? drawingArea.cursorShape
                                                                  : Qt.ArrowCursor)
                        property real pressX: 0
                        property real pressY: 0
                        property real initialContentX: 0
                        property real initialContentY: 0
                        onPressed: function(mouse) {
                            workspace.forceActiveFocus()
                            pressX=mouse.x;pressY=mouse.y
                            initialContentX=viewport.contentX;initialContentY=viewport.contentY
                        }
                        onPositionChanged: function(mouse) {
                            if(!pressed) return
                            viewport.contentX=clampScroll(initialContentX-(mouse.x-pressX),viewport.contentWidth-viewport.width)
                            viewport.contentY=clampScroll(initialContentY-(mouse.y-pressY),viewport.contentHeight-viewport.height)
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; height: editor.tool==="text" ? 54 : 40; color: "#20262e"
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 15; anchors.rightMargin: 15
                        Label { text: editor.tool==="text" ? qsTr("Texte") : editor.tool==="crop" ? qsTr("Tracez une zone, puis cliquez sur Recadrer") : ""; color: "#8e9aa7" }
                        TextField { id: textField; objectName: "textField"; visible: editor.tool==="text"; text: editor.text; Layout.fillWidth: true; placeholderText: qsTr("Cliquez ensuite sur le canevas"); onTextEdited: editor.text=text }
                        DarkSpinBox { visible: editor.tool==="text"; from: 6; to: 512; value: editor.textSize; suffix: " px"; onValueModified: editor.textSize=value }
                        Item { Layout.fillWidth: true; visible: editor.tool!=="text" }
                        IconButton { text: qsTr("−"); implicitHeight: 28; onClicked: setZoom(zoom/1.25) }
                        Label { text: Math.round(zoom*100)+" %"; color: "#bec8d2"; horizontalAlignment: Text.AlignHCenter; Layout.preferredWidth: 50 }
                        IconButton { text: qsTr("+"); implicitHeight: 28; onClicked: setZoom(zoom*1.25) }
                        IconButton { text: qsTr("Adapter"); implicitWidth: 65; implicitHeight: 28; hint: qsTr("Adapter à la fenêtre"); onClicked: fitCanvas() }
                    }
                }
            }
            Rectangle { width: 1; Layout.fillHeight: true; color: "#10141a" }
            Rectangle {
                Layout.preferredWidth: 280; Layout.fillHeight: true; color: "#242a32"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 15; spacing: 8
                    PanelTitle { visible: window.height>=800; text: qsTr("NAVIGATEUR") }
                    Rectangle {
                        visible: window.height>=800; Layout.fillWidth: true; height: 90; color: "#181e25"; radius: 5
                        Image { anchors.fill: parent; anchors.margins: 8; source: "image://layers/composite/"+editor.revision; sourceSize.width: 240; sourceSize.height: 105; fillMode: Image.PreserveAspectFit; cache: false }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: editor.width+" × "+editor.height+" px"; color: "#9ba7b5"; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Label { text: qsTr("sRGB"); color: "#74828e"; font.pixelSize: 10 }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#363e47" }
                    PanelTitle { text: qsTr("COULEUR") }
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
                        IconButton { glyph: "⇄"; hint: qsTr("Échanger les couleurs"); onClicked: editor.swapColors() }
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
                        PanelTitle { text: qsTr("CALQUES") }
                        Item { Layout.fillWidth: true }
                        Label { text: editor.layers.length; color: "#71808d"; font.pixelSize: 10 }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ComboBox {
                            id: layerBlendMode
                            objectName: "layerBlendMode"
                            property bool previewActive: false
                            Layout.fillWidth: true
                            enabled: activeInfo.kind!=="group"
                            model: window.blendModeNames
                            currentIndex: window.activeInfo.blend
                            popup.onOpened: {
                                previewActive = true
                                editor.beginLayerPreview()
                            }
                            onHighlightedIndexChanged: {
                                if(previewActive && popup.visible && highlightedIndex>=0)
                                    editor.previewLayerBlend(editor.activeLayer,highlightedIndex)
                            }
                            onActivated: {
                                editor.previewLayerBlend(editor.activeLayer,currentIndex)
                                editor.finishLayerPreview(qsTr("Mode de fusion"))
                                previewActive = false
                            }
                            popup.onClosed: {
                                if(previewActive) editor.cancelLayerPreview()
                                previewActive = false
                            }
                        }
                        IconButton { glyph: activeInfo.locked?"▣":"▢"; selected: activeInfo.locked; hint: qsTr("Verrouiller / déverrouiller"); onClicked: editor.setLayerLocked(editor.activeLayer,!activeInfo.locked) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Opacité"); color: "#8e9baa"; font.pixelSize: 11 }
                        DarkSlider {
                            id: layerOpacity; Layout.fillWidth: true; from: 0; to: 1
                            enabled: activeInfo.kind!=="group"
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
                            objectName: "layerDelegate"
                            required property var modelData
                            width: layerList.width; height: 64; radius: 5
                            color: modelData.layerIndex===editor.activeLayer ? "#35443e" : modelData.selected ? "#303b48" : "#292f38"
                            border.width: modelData.selected ? 2 : 1
                            border.color: modelData.layerIndex===editor.activeLayer ? "#78a589" : modelData.selected ? "#66829b" : "#343c46"
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function(mouse) {
                                    if(mouse.button===Qt.RightButton) {
                                        if(!modelData.selected) editor.selectLayer(modelData.layerIndex)
                                    } else {
                                        const toggle=Boolean(mouse.modifiers&(Qt.ControlModifier|Qt.MetaModifier))
                                        const range=Boolean(mouse.modifiers&Qt.ShiftModifier)
                                        editor.selectLayer(modelData.layerIndex,toggle,range)
                                    }
                                    if(mouse.button===Qt.RightButton) window.openLayerContextMenu()
                                }
                                onDoubleClicked: function(mouse) {
                                    if(mouse.button!==Qt.LeftButton) return
                                    editor.selectLayer(modelData.layerIndex)
                                    if(modelData.kind!=="pixel" && modelData.kind!=="group") layerPropertiesDialog.open()
                                    else { layerName.text=modelData.name; renameDialog.open() }
                                }
                            }
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 6
                                anchors.leftMargin: modelData.groupId!=="" && modelData.kind!=="group" ? 24 : 6
                                spacing: 7
                                IconButton {
                                    visible: modelData.kind==="group"
                                    Layout.preferredWidth: visible ? 22 : 0
                                    glyph: modelData.groupCollapsed ? "▸" : "▾"
                                    hint: modelData.groupCollapsed ? qsTr("Déplier le groupe") : qsTr("Replier le groupe")
                                    onClicked: editor.toggleGroupCollapsed(modelData.layerIndex)
                                }
                                Item {
                                    Layout.preferredWidth: 24
                                    Layout.fillHeight: true
                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.visible ? "◉" : "○"
                                        color: visibilityMouse.containsMouse ? "#ffffff" : "#c0c7d0"
                                        font.pixelSize: 19
                                    }
                                    MouseArea {
                                        id: visibilityMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: function(mouse) {
                                            if(mouse.modifiers & Qt.AltModifier)
                                                editor.toggleLayerIsolation(modelData.layerIndex)
                                            else
                                                editor.setLayerVisible(modelData.layerIndex,!modelData.visible)
                                        }
                                    }
                                    ToolTip.visible: visibilityMouse.containsMouse
                                    ToolTip.text: qsTr("Visibilité du calque — Alt-clic pour isoler")
                                    ToolTip.delay: 500
                                }
                                Label {
                                    visible: modelData.clippingMask
                                    Layout.preferredWidth: visible ? 14 : 0
                                    text: "↳"
                                    color: "#a7d7b5"
                                    font.pixelSize: 17
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                Rectangle {
                                    width: 42; height: 36; color: "#182027"; radius: 3; border.color: "#506159"
                                    Image { anchors.fill: parent; anchors.margins: 2; source: "image://layers/"+modelData.layerIndex+"/"+editor.thumbnailRevision; sourceSize.width: 80; sourceSize.height: 60; fillMode: Image.PreserveAspectFit; cache: false }
                                    MouseArea { anchors.fill: parent; onClicked: { editor.activeLayer=modelData.layerIndex; editor.setEditingMask(false) } }
                                }
                                Rectangle {
                                    visible: modelData.hasMask
                                    width: visible?34:0; height: 36; color: "#182027"; radius: 3
                                    border.width: modelData.editingMask?2:1; border.color: modelData.editingMask?"#c5f2cf":"#69757f"
                                    Image { anchors.fill: parent; anchors.margins: 2; source: modelData.hasMask?"image://layers/mask/"+modelData.layerIndex+"/"+editor.thumbnailRevision:""; sourceSize.width: 64; sourceSize.height: 64; fillMode: Image.PreserveAspectFit; cache: false }
                                    Label { anchors.centerIn: parent; visible: !modelData.maskEnabled; text: qsTr("×"); color: "#ef6b6b"; font.pixelSize: 34; font.bold: true }
                                    MouseArea { anchors.fill: parent; onClicked: { editor.activeLayer=modelData.layerIndex; editor.setEditingMask(true); layerPropertiesDialog.open() } }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 3
                                    Label { text: modelData.name; color: modelData.visible?"#d6e0db":"#7c8791"; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: 11 }
                                    Label {
                                        text: (modelData.clippingMask ? qsTr("Écrêté · ") : "") +
                                              (modelData.locked?qsTr("Verrouillé"):modelData.kind==="group"?qsTr("Groupe de calques"):modelData.editingMask?qsTr("Masque actif"):modelData.kind==="adjustment"?qsTr("Calque de réglage"):modelData.kind.indexOf("frequency")===0?qsTr("Séparation dynamique"):qsTr("Calque de pixels"))
                                        color: modelData.clippingMask?"#a7d7b5":modelData.editingMask?"#bce8c8":"#80928c"
                                        font.pixelSize: 9
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 4
                        IconButton { glyph: "+"; hint: qsTr("Nouveau calque ou réglage"); onClicked: createLayerMenu.popup() }
                        IconButton { glyph: "⧉"; hint: qsTr("Dupliquer le calque"); enabled: activeInfo.kind!=="group"; onClicked: editor.duplicateLayer() }
                        IconButton { glyph: "◐"; hint: activeInfo.hasMask?qsTr("Sélectionner le masque"):qsTr("Ajouter un masque de fusion"); enabled: activeInfo.kind!=="group"; onClicked: activeInfo.hasMask?editor.setEditingMask(true):editor.addLayerMask(true) }
                        IconButton { glyph: "ƒ"; hint: qsTr("Propriétés du calque"); enabled: activeInfo.kind!=="group" && (activeInfo.kind!=="pixel" || activeInfo.editingMask); onClicked: layerPropertiesDialog.open() }
                        IconButton { glyph: "↑"; hint: qsTr("Monter le calque"); enabled: activeInfo.kind!=="group" && editor.activeLayer<editor.layers.length-1; onClicked: editor.moveLayer(1) }
                        IconButton { glyph: "↓"; hint: qsTr("Descendre le calque"); enabled: activeInfo.kind!=="group" && editor.activeLayer>0; onClicked: editor.moveLayer(-1) }
                        Item { Layout.fillWidth: true }
                        IconButton { glyph: "×"; hint: qsTr("Supprimer le calque"); enabled: editor.layers.length>1; onClicked: editor.removeLayer() }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#363e47" }
                    PanelTitle { text: qsTr("HISTORIQUE") }
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
                Label { text: qsTr("%1 × %2 px   ·   %3 calques   ·   PhotoCrap 0.1").arg(editor.width).arg(editor.height).arg(editor.layers.length); color: "#73818f"; font.pixelSize: 10 }
            }
        }
    }

    DarkMenu {
        id: createLayerMenu
        DarkMenuItem { text: qsTr("Calque de pixels"); onTriggered: editor.addLayer() }
        DarkMenuItem { text: qsTr("Séparation de fréquences…"); onTriggered: { editor.addFrequencySeparation(8); layerPropertiesDialog.open() } }
        MenuSeparator {}
        DarkMenuItem { text: qsTr("Luminosité / Contraste"); onTriggered: addAdjustmentLayer("brightnessContrast") }
        DarkMenuItem { text: qsTr("Niveaux"); onTriggered: addAdjustmentLayer("levels") }
        DarkMenuItem { text: qsTr("Courbes"); onTriggered: addAdjustmentLayer("curves") }
        DarkMenuItem { text: qsTr("Exposition"); onTriggered: addAdjustmentLayer("exposure") }
        DarkMenuItem { text: qsTr("Vibrance"); onTriggered: addAdjustmentLayer("vibrance") }
        DarkMenuItem { text: qsTr("Teinte / Saturation"); onTriggered: addAdjustmentLayer("hueSaturation") }
        DarkMenuItem { text: qsTr("Balance des couleurs"); onTriggered: addAdjustmentLayer("colorBalance") }
        DarkMenuItem { text: qsTr("Noir et blanc"); onTriggered: addAdjustmentLayer("blackWhite") }
        DarkMenuItem { text: qsTr("Filtre photo"); onTriggered: addAdjustmentLayer("photoFilter") }
        DarkMenuItem { text: qsTr("Mélangeur de couches"); onTriggered: addAdjustmentLayer("channelMixer") }
        DarkMenuItem { text: qsTr("Recherche de couleurs"); onTriggered: addAdjustmentLayer("colorLookup") }
        DarkMenuItem { text: qsTr("Négatif"); onTriggered: addAdjustmentLayer("invert") }
        DarkMenuItem { text: qsTr("Isohélie"); onTriggered: addAdjustmentLayer("posterize") }
        DarkMenuItem { text: qsTr("Seuil"); onTriggered: addAdjustmentLayer("threshold") }
        DarkMenuItem { text: qsTr("Transfert de dégradé"); onTriggered: addAdjustmentLayer("gradientMap") }
        DarkMenuItem { text: qsTr("Correction sélective"); onTriggered: addAdjustmentLayer("selectiveColor") }
        MenuSeparator {}
        DarkMenu {
            title: qsTr("Filtres")
            DarkMenuItem { text: qsTr("Flou gaussien…"); onTriggered: addAdjustmentLayer("gaussianBlur") }
            DarkMenuItem { text: qsTr("Flou directionnel…"); onTriggered: addAdjustmentLayer("motionBlur") }
            DarkMenuItem { text: qsTr("Flou de boîte…"); onTriggered: addAdjustmentLayer("boxBlur") }
            DarkMenuItem { text: qsTr("Netteté optimisée…"); onTriggered: addAdjustmentLayer("unsharpMask") }
            DarkMenuItem { text: qsTr("Passe-haut…"); onTriggered: addAdjustmentLayer("highPass") }
            DarkMenuItem { text: qsTr("Mosaïque…"); onTriggered: addAdjustmentLayer("mosaic") }
            DarkMenuItem { text: qsTr("Ajout de bruit…"); onTriggered: addAdjustmentLayer("noise") }
        }
    }

    DarkMenu {
        id: layerContextMenu
        objectName: "layerContextMenu"
        ContextMenuItem { text: qsTr("Dupliquer le calque"); enabled: activeInfo.kind!=="group"; onTriggered: editor.duplicateLayer() }
        ContextMenuItem { text: qsTr("Renommer…"); onTriggered: { layerName.text=activeInfo.name; renameDialog.open() } }
        ContextMenuItem { text: qsTr("Supprimer le calque"); enabled: editor.layers.length>1 && !activeInfo.locked; onTriggered: editor.removeLayer() }
        MenuSeparator {}
        ContextMenuItem {
            text: activeInfo.clippingMask ? qsTr("Libérer le masque d’écrêtage")
                                          : qsTr("Créer un masque d’écrêtage")
            enabled: activeInfo.kind!=="group" && (activeInfo.clippingMask || editor.activeLayer>0)
            onTriggered: editor.setClippingMask(!activeInfo.clippingMask)
        }
        ContextMenuItem { text: qsTr("Grouper les calques sélectionnés"); enabled: editor.canGroupSelected; onTriggered: editor.groupSelectedLayers() }
        ContextMenuItem { text: qsTr("Sortir du groupe"); enabled: editor.canUngroupActive; onTriggered: editor.ungroupActiveLayers() }
        ContextMenuItem { text: qsTr("Fusionner avec le calque inférieur"); enabled: editor.activeLayer>0 && activeInfo.kind==="pixel" && !activeInfo.locked; onTriggered: editor.mergeDown() }
        MenuSeparator {}
        DarkMenu {
            title: qsTr("Ajouter un masque")
            enabled: activeInfo.kind!=="group" && !activeInfo.hasMask
            ContextMenuItem { text: editor.hasSelection ? qsTr("Faire apparaître la sélection") : qsTr("Tout faire apparaître"); onTriggered: editor.addLayerMask(true) }
            ContextMenuItem { text: editor.hasSelection ? qsTr("Masquer la sélection") : qsTr("Tout masquer"); onTriggered: editor.addLayerMask(false) }
        }
        ContextMenuItem { text: activeInfo.maskEnabled ? qsTr("Désactiver le masque") : qsTr("Activer le masque"); enabled: activeInfo.hasMask; onTriggered: editor.setMaskEnabled(!activeInfo.maskEnabled) }
        ContextMenuItem { text: qsTr("Supprimer le masque"); enabled: activeInfo.hasMask; onTriggered: editor.removeLayerMask() }
        ContextMenuItem { text: qsTr("Dissocier le masque"); enabled: false }
        MenuSeparator {}
        ContextMenuItem { text: activeInfo.visible ? qsTr("Masquer le calque") : qsTr("Afficher le calque"); onTriggered: editor.setLayerVisible(editor.activeLayer,!activeInfo.visible) }
    }

    FileDialog {
        id: openDialog; title: window.importing ? qsTr("Importer comme calque") : qsTr("Ouvrir une image ou un projet")
        nameFilters: window.importing ? [qsTr("Images (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff)"),qsTr("Tous les fichiers (*)")] : [qsTr("Images et projets (*.pcr *.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff)"),qsTr("Tous les fichiers (*)")]
        onAccepted: {
            if(window.importing) editor.importLayer(selectedFile)
            else if(editor.openFile(selectedFile)) recentFiles.add(selectedFile)
        }
    }
    FileDialog {
        id: saveDialog; title: qsTr("Enregistrer le projet avec ses calques"); fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Projet PhotoCrap (*.pcr)")]; defaultSuffix: "pcr"
        onAccepted: {
            if(editor.saveProject(selectedFile)) {
                recentFiles.add(editor.projectUrl)
                if(window.resumeAfterSave) { window.resumeAfterSave=false; window.perform(window.pendingAction) }
            }
        }
        onRejected: window.resumeAfterSave=false
    }
    FileDialog {
        id: exportDialog; title: qsTr("Exporter l’image aplatie"); fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG (*.png)"),qsTr("JPEG (*.jpg *.jpeg)"),qsTr("BMP (*.bmp)"),qsTr("WebP (*.webp)")]
        defaultSuffix: selectedNameFilter.index===1?"jpg":selectedNameFilter.index===2?"bmp":selectedNameFilter.index===3?"webp":"png"
        onAccepted: editor.exportImage(selectedFile)
    }
    ColorDialog { id: colorDialog; title: qsTr("Choisir une couleur"); onAccepted: { if(changingSecondary) editor.secondaryColor=selectedColor; else editor.color=selectedColor } }
    Dialog {
        id: newDialog; objectName: "newDialog"; title: qsTr("Nouveau document"); anchors.centerIn: parent; modal: true; width: 410
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent; spacing: 16
            Label { text: qsTr("Une page blanche pour votre prochaine idée."); color: "#a5b2c0" }
            ComboBox {
                Layout.fillWidth: true; model: [qsTr("Personnalisé"),qsTr("Écran · 1920 × 1080"),qsTr("Carré · 1080 × 1080"),qsTr("Portrait · 1080 × 1350"),qsTr("A4 · 2480 × 3508")]
                onActivated: {
                    const sizes=[[1280,800],[1920,1080],[1080,1080],[1080,1350],[2480,3508]]
                    newWidth.value=sizes[currentIndex][0]; newHeight.value=sizes[currentIndex][1]
                }
            }
            RowLayout { Label { text: qsTr("Largeur (px)"); Layout.preferredWidth: 140 } DarkSpinBox { id: newWidth; from: 1; to: 16384; value: 1280; suffix: " px"; Layout.fillWidth: true } }
            RowLayout { Label { text: qsTr("Hauteur (px)"); Layout.preferredWidth: 140 } DarkSpinBox { id: newHeight; from: 1; to: 16384; value: 800; suffix: " px"; Layout.fillWidth: true } }
            CheckBox { id: transparent; text: qsTr("Arrière-plan transparent") }
            Label { text: qsTr("RVB · 8 bits · sRGB"); color: "#8794a2" }
        }
        onAccepted: editor.newDocument(newWidth.value,newHeight.value,transparent.checked)
    }
    Dialog {
        id: resizeDialog; title: qsTr("Dimensions de l’image"); anchors.centerIn: parent; modal: true; width: 370
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent; spacing: 16
            RowLayout { Label { text: qsTr("Largeur"); Layout.preferredWidth: 100 } DarkSpinBox { id: resizeWidth; from: 1; to: 16384; suffix: " px" } }
            RowLayout { Label { text: qsTr("Hauteur"); Layout.preferredWidth: 100 } DarkSpinBox { id: resizeHeight; from: 1; to: 16384; suffix: " px" } }
            Label { text: qsTr("Tous les calques sont rééchantillonnés.\nLes proportions sont libres."); color: "#9eabb7" }
        }
        onAccepted: editor.resizeImage(resizeWidth.value,resizeHeight.value)
    }
    Dialog {
        id: renameDialog; title: qsTr("Renommer le calque"); anchors.centerIn: parent; modal: true; width: 350
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
            Label { text: qsTr("Appliquer au calque actif"); color: "#9cabb7" }
            RowLayout {
                Layout.fillWidth: true
                Label { text: window.adjustmentTitle; color: "#c4cdd5"; Layout.fillWidth: true }
                Label { text: Math.round(adjustmentSlider.value); color: "#aeb9c3"; Layout.preferredWidth: 42; horizontalAlignment: Text.AlignRight }
            }
            DarkSlider { id: adjustmentSlider; Layout.fillWidth: true; from: window.adjustment==="blur"?1:-100; to: window.adjustment==="blur"?30:100; stepSize: 1 }
            Label { text: qsTr("La sélection limite la zone traitée.\nL’opération peut être annulée."); color: "#8996a4" }
        }
        onApplied: { editor.applyFilter(window.adjustment,adjustmentSlider.value); close() }
    }
    Dialog {
        id: brushSettingsDialog
        objectName: "brushSettingsDialog"
        title: qsTr("Paramètres de pinceau")
        x: Math.max(20,window.width-width-310)
        y: 95
        width: 440
        height: Math.min(650,window.height-130)
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        onOpened: brushPreview.requestPaint()

        contentItem: ColumnLayout {
            spacing: 10
            Label { text: qsTr("Forme de la pointe"); color: "#eef2f5"; font.pixelSize: 17; font.bold: true }
            Canvas {
                id: brushPreview
                Layout.fillWidth: true; Layout.preferredHeight: 105
                onPaint: {
                    const c=getContext("2d");c.reset();c.fillStyle="#151a20";c.fillRect(0,0,width,height)
                    const radius=Math.min(editor.brushSize/2,38);const spacing=Math.max(1,editor.brushSize*editor.brushSpacing)
                    c.globalAlpha=editor.brushOpacity
                    for(let x=28;x<width-20;x+=spacing) {
                        c.save();c.translate(x,height/2);c.rotate(editor.brushAngle*Math.PI/180);c.scale(1,editor.brushRoundness)
                        const g=c.createRadialGradient(0,0,0,0,0,radius)
                        g.addColorStop(0,"rgba(190,236,203,"+editor.brushFlow+")")
                        g.addColorStop(Math.min(.999,editor.brushHardness),"rgba(190,236,203,"+editor.brushFlow+")")
                        g.addColorStop(1,"rgba(190,236,203,0)");c.fillStyle=g;c.beginPath();c.arc(0,0,radius,0,Math.PI*2);c.fill();c.restore()
                    }
                }
                Connections { target: editor; function onSettingsChanged() { brushPreview.requestPaint() } }
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#91a0ad"
                text: qsTr("L’opacité plafonne un passage. Le débit détermine la quantité déposée par chaque empreinte et s’accumule lorsque les empreintes se chevauchent.")
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Mode"); color: "#c4cdd5" }
                ComboBox { Layout.fillWidth: true; model: window.blendModeNames; currentIndex: editor.brushBlend; onActivated: editor.brushBlend=currentIndex }
            }
            ScrollView {
                id: brushSettingsScroll
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: brushSettingsScroll.availableWidth; spacing: 9
                    Repeater {
                        model: [
                            {key:"brushSize",label:qsTr("Taille"),from:1,to:500,scale:1,suffix:" px"},
                            {key:"brushHardness",label:qsTr("Dureté du contour"),from:0,to:100,scale:100,suffix:" %"},
                            {key:"brushOpacity",label:qsTr("Opacité"),from:1,to:100,scale:100,suffix:" %"},
                            {key:"brushFlow",label:qsTr("Débit"),from:1,to:100,scale:100,suffix:" %"},
                            {key:"brushSpacing",label:qsTr("Espacement"),from:1,to:1000,scale:100,suffix:" %"},
                            {key:"brushAngle",label:qsTr("Angle"),from:-180,to:180,scale:1,suffix:"°"},
                            {key:"brushRoundness",label:qsTr("Arrondi"),from:1,to:100,scale:100,suffix:" %"},
                            {key:"brushSmoothing",label:qsTr("Lissage"),from:0,to:100,scale:100,suffix:" %"}
                        ]
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true; spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: modelData.label; color: "#c4cdd5"; Layout.fillWidth: true }
                                Label { text: Math.round(brushSettingSlider.value)+modelData.suffix; color: "#91a0ad"; Layout.preferredWidth: 64; horizontalAlignment: Text.AlignRight }
                            }
                            DarkSlider {
                                id: brushSettingSlider
                                Layout.fillWidth: true; from: modelData.from; to: modelData.to; stepSize: 1
                                value: Number(editor[modelData.key])*modelData.scale
                                onMoved: editor[modelData.key]=value/modelData.scale
                            }
                        }
                    }
                }
            }
        }
        footer: DialogButtonBox { Button { text: qsTr("Fermer"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: brushSettingsDialog.close() } }
    }
    Dialog {
        id: layerPropertiesDialog
        objectName: "layerPropertiesDialog"
        title: qsTr("Propriétés · ") + activeInfo.name
        x: Math.max(20, window.width-width-310)
        y: 105
        width: 430
        height: Math.min(610, window.height-150)
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        onClosed: { window.cancelLayerPreviewQueue(); editor.cancelLayerPreview() }

        contentItem: ColumnLayout {
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Label { text: activeInfo.editingMask?qsTr("Masque de fusion"):activeInfo.kind.indexOf("frequency")===0?qsTr("Séparation de fréquences"):qsTr("Calque de réglage"); color: "#eef2f5"; font.pixelSize: 17; font.bold: true }
                Item { Layout.fillWidth: true }
                CheckBox { id: layerPreview; text: qsTr("Aperçu"); checked: true }
            }
            Label {
                Layout.fillWidth: true
                text: activeInfo.editingMask ? qsTr("Le blanc révèle, le noir masque et les gris donnent une transparence partielle. Densité réduit l’effet du masque ; Contour progressif adoucit ses bords.") : activeInfo.kind.indexOf("frequency")===0 ? qsTr("Deux calques liés séparent la couleur et les volumes (basses fréquences) de la texture (hautes fréquences, Lumière linéaire). Le rayon est recalculé sur les deux calques pendant le déplacement.") : qsTr("Les réglages restent modifiables, possèdent automatiquement un masque et s’appliquent à tous les calques visibles situés en dessous.")
                color: "#91a0ad"; wrapMode: Text.WordWrap
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#3a444d" }
            ScrollView {
                id: layerPropertiesScroll
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: layerPropertiesScroll.availableWidth; spacing: 11
                    Repeater {
                        model: activeInfo.editingMask ? [
                            {key:"_maskDensity",label:qsTr("Densité (%)"),from:0,to:100,step:1},
                            {key:"_maskFeather",label:qsTr("Contour progressif"),from:0,to:100,step:0.5}
                        ] : activeInfo.kind.indexOf("frequency")===0 ? [
                            {key:"radius",label:qsTr("Rayon du flou gaussien"),from:1,to:100,step:1}
                        ] : (activeInfo.adjustmentType==="curves" || activeInfo.adjustmentType==="levels") ? []
                          : (window.adjustmentDefinitions[activeInfo.adjustmentType] || [])
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true; spacing: 3
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: modelData.label; color: "#c4cdd5"; Layout.fillWidth: true }
                                Label { text: Number(parameterSlider.value).toFixed(modelData.step<1?2:0); color: "#91a0ad"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
                            }
                            DarkSlider {
                                id: parameterSlider
                                objectName: "parameterSlider"
                                Layout.fillWidth: true
                                onWidthChanged: window.parameterSliderWidth=width
                                from: modelData.from; to: modelData.to; stepSize: modelData.step
                                value: activeInfo.parameters[modelData.key] === undefined ? 0 : Number(activeInfo.parameters[modelData.key])
                                onPressedChanged: {
                                    if(pressed) editor.beginLayerPreview()
                                    else window.previewLayerValue(modelData.key,value,true)
                                }
                                onMoved: window.queueLayerPreview(modelData.key,value)
                            }
                        }
                    }
                    CurvesEditor {
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType==="curves"
                        Layout.fillWidth: true
                        histogram: (editor.revision, editor.histogram())
                        sourcePoints: activeInfo.parameters.points || [{"x":0,"y":0},{"x":255,"y":255}]
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(points) {
                            if(layerPreview.checked) window.queueLayerPreview("points",points)
                        }
                        onCommitted: function(points) {
                            window.previewLayerValue("points",points,true)
                        }
                    }
                    LevelsEditor {
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType==="levels"
                        Layout.fillWidth: true
                        histogram: (editor.revision, editor.histogram())
                        parameters: activeInfo.parameters
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) {
                            if(layerPreview.checked) window.queueLayerPreview(key,value)
                        }
                        onCommitted: function(key,value) {
                            window.previewLayerValue(key,value,true)
                        }
                    }
                    ColumnLayout {
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType==="gradientMap"
                        Layout.fillWidth: true
                        Label { text: qsTr("Couleurs du dégradé"); color: "#c4cdd5" }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                Layout.fillWidth: true; text: activeInfo.parameters.shadowColor || "#101828"; placeholderText: qsTr("Ombres")
                                onEditingFinished: { editor.beginLayerPreview(); editor.previewLayerParameter("shadowColor",text); editor.finishLayerPreview("Couleur des ombres") }
                            }
                            TextField {
                                Layout.fillWidth: true; text: activeInfo.parameters.highlightColor || "#f5d9a8"; placeholderText: qsTr("Lumières")
                                onEditingFinished: { editor.beginLayerPreview(); editor.previewLayerParameter("highlightColor",text); editor.finishLayerPreview("Couleur des lumières") }
                            }
                        }
                    }
                    Label {
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType==="invert"
                        Layout.fillWidth: true
                        text: qsTr("Ce calque inverse automatiquement les couleurs. Son intensité se règle avec l’opacité du calque.")
                        color: "#91a0ad"; wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        visible: activeInfo.editingMask
                        Layout.fillWidth: true
                        Button { text: qsTr("Inverser"); onClicked: editor.invertLayerMask() }
                        Button { text: activeInfo.maskEnabled?qsTr("Désactiver"):qsTr("Activer"); onClicked: editor.setMaskEnabled(!activeInfo.maskEnabled) }
                        Item { Layout.fillWidth: true }
                        Button { text: qsTr("Supprimer"); onClicked: { editor.removeLayerMask(); layerPropertiesDialog.close() } }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Fermer"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: layerPropertiesDialog.close() }
        }
    }
    Dialog {
        id: preferencesDialog
        objectName: "preferencesDialog"
        title: qsTr("Réglages","application preferences")
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
                        text: qsTr("RÉGLAGES")
                        color: "#71808e"
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 1.6
                        bottomPadding: 6
                    }
                    ItemDelegate {
                        Layout.fillWidth: true
                        highlighted: window.preferencesPage===0
                        text: qsTr("⌨   Raccourcis clavier")
                        Accessible.name: qsTr("Raccourcis clavier")
                        onClicked: window.preferencesPage=0
                    }
                    ItemDelegate {
                        Layout.fillWidth: true
                        highlighted: window.preferencesPage===1
                        text: qsTr("◉   Interface")
                        Accessible.name: qsTr("Interface")
                        onClicked: window.preferencesPage=1
                    }
                    Item { Layout.fillHeight: true }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Les réglages sont enregistrés automatiquement.")
                        color: "#71808e"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 22
                Layout.rightMargin: 14
                Layout.topMargin: 14
                currentIndex: window.preferencesPage

                ColumnLayout {
                    spacing: 10
                    Label {
                        text: qsTr("Raccourcis clavier")
                        color: "#eef2f5"
                        font.pixelSize: 20
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Cliquez dans un champ puis saisissez une combinaison comme Ctrl+Shift+E. Sur macOS, Ctrl correspond à la touche Commande. Videz un champ pour désactiver la commande.")
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
                                placeholderText: qsTr("Aucun")
                                selectByMouse: true
                                Accessible.name: qsTr("Raccourci pour %1").arg(modelData.label)
                                onEditingFinished: {
                                    if (text !== modelData.sequence && !shortcuts.setSequence(modelData.id, text))
                                        text = modelData.sequence
                                }
                            }
                            ToolButton {
                                text: qsTr("×")
                                enabled: modelData.sequence.length > 0
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Désactiver ce raccourci")
                                onClicked: shortcuts.setSequence(modelData.id, "")
                            }
                        }
                    }
                }
                }
                ColumnLayout {
                    spacing: 14
                    Label { text: qsTr("Interface"); color: "#eef2f5"; font.pixelSize: 20; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Choisissez la langue de l’interface. Le changement est immédiat et sera conservé au prochain démarrage.")
                        color: "#95a2af"; wrapMode: Text.WordWrap
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#39424c" }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Langue"); color: "#d3dbe2"; Layout.preferredWidth: 150 }
                        ComboBox {
                            id: languageCombo
                            objectName: "languageCombo"
                            Layout.fillWidth: true
                            model: languages.availableLanguages
                            textRole: "name"
                            valueRole: "code"
                            currentIndex: languages.language==="en"?1:0
                            onActivated: languages.language=currentValue
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("D’autres langues pourront être ajoutées à partir du même catalogue de traduction.")
                        color: "#71808e"; wrapMode: Text.WordWrap
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Valeurs par défaut")
                DialogButtonBox.buttonRole: DialogButtonBox.ResetRole
                onClicked: shortcuts.resetDefaults()
            }
            Button {
                text: qsTr("Fermer")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: preferencesDialog.close()
            }
        }
    }
    Dialog {
        id: unsavedDialog; objectName: "unsavedDialog"; title: qsTr("Enregistrer vos modifications ?"); anchors.centerIn: parent; modal: true; width: 450
        closePolicy: Popup.NoAutoClose
        Label { width: parent.width; text: qsTr("Le document « %1 » contient des modifications non enregistrées.").arg(editor.name); wrapMode: Text.WordWrap }
        footer: DialogButtonBox {
            Button { text: qsTr("Annuler"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole; onClicked: unsavedDialog.close() }
            Button { text: qsTr("Abandonner"); DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole; onClicked: { unsavedDialog.close(); window.perform(window.pendingAction) } }
            Button { text: qsTr("Enregistrer"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: { unsavedDialog.close(); window.resumeAfterSave=true; window.save(false) } }
        }
    }
    Dialog {
        id: errorDialog; title: qsTr("Impossible de terminer l’opération"); anchors.centerIn: parent; modal: true; width: 460
        standardButtons: Dialog.Ok
        Label { id: errorLabel; width: parent.width; wrapMode: Text.WordWrap }
    }
    Dialog {
        id: aboutDialog; title: qsTr("PhotoCrap 0.1"); anchors.centerIn: parent; modal: true; width: 480; standardButtons: Dialog.Ok
        Label {
            width: parent.width; wrapMode: Text.WordWrap; lineHeight: 1.4
            text: qsTr("Un atelier de création d’images en C++ et Qt Quick.\n\nCalques, dessin, sélection, transformations et retouche, sur macOS, Linux et Windows.\n\nCette première version utilise des calques raster en sRGB 8 bits. Elle ne lit pas les PSD et n’inclut pas les objets dynamiques, les masques de calque, le RAW ou les fonctions IA.\n\nLogiciel indépendant, sans affiliation avec Adobe.")
        }
    }
}
