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
    property real canvasRotation: 0
    property string status: qsTr("Prêt à créer")
    property string pendingAction: ""
    property url pendingRecentFile: ""
    property bool allowClose: false
    property bool resumeAfterSave: false
    property bool importing: false
    property bool changingSecondary: false
    property bool changingShapeFill: false
    property bool changingShapeStroke: false
    property bool changingSolidColor: false
    property bool changingBlackWhiteTint: false
    property bool changingPhotoFilterColor: false
    property bool fillingMaskColor: false
    property bool spacePressed: false
    property bool stampSourcePicking: false
    readonly property bool stampToolActive: editor.tool === "cloneStamp" || editor.tool === "patternStamp"
    readonly property bool selectionToolActive: editor.tool === "select"
                                                 || editor.tool === "ellipticalSelect"
                                                 || editor.tool === "singleRowSelect"
                                                 || editor.tool === "singleColumnSelect"
                                                 || editor.tool === "lasso"
                                                 || editor.tool === "polygonalLasso"
                                                 || editor.tool === "quickSelection"
                                                 || editor.tool === "magicWand"
    readonly property bool smartSelectionToolActive: editor.tool === "quickSelection"
                                                     || editor.tool === "magicWand"
    readonly property int canvasCursorShape: window.stampSourcePicking && (editor.tool === "healing" || editor.tool === "cloneStamp" || editor.tool === "colorReplacement" || editor.tool === "mixerBrush")
                                             ? Qt.CrossCursor
                                             : window.spacePressed || editor.tool === "hand"
                                             ? Qt.OpenHandCursor
                                             : (editor.tool === "patch" || editor.tool === "contentAwareMove") && (editor.patchDragging || drawingArea.patchInsideSelection)
                                               ? Qt.SizeAllCursor
                                             : editor.tool === "move"
                                               ? Qt.SizeAllCursor
                                             : window.paintToolActive
                                                   || window.smartSelectionToolActive
                                                 ? Qt.BlankCursor : Qt.CrossCursor
    readonly property bool closedShapeToolActive: editor.tool === "rectangle" || editor.tool === "ellipse"
                                                  || editor.tool === "triangle" || editor.tool === "polygon"
                                                  || editor.tool === "star"
    readonly property bool shapeToolActive: closedShapeToolActive || editor.tool === "line"
    readonly property bool retouchToolActive: editor.tool === "blur" || editor.tool === "sharpen"
                                              || editor.tool === "dodge" || editor.tool === "burn"
                                              || editor.tool === "sponge" || editor.tool === "smudge"
    readonly property bool paintToolActive: editor.tool === "brush" || editor.tool === "pencil"
                                            || editor.tool === "eraser" || editor.tool === "remove" || (editor.tool === "historyBrush" || editor.tool === "artHistoryBrush") || editor.tool === "spotHealing" || editor.tool === "healing" || editor.tool === "colorReplacement" || editor.tool === "mixerBrush" || retouchToolActive || stampToolActive
    readonly property bool opacityToolActive: editor.tool === "brush" || editor.tool === "pencil"
                                              || editor.tool === "eraser" || editor.tool === "fill"
                                              || editor.tool === "gradient" || editor.tool === "text" || (editor.tool === "historyBrush" || editor.tool === "artHistoryBrush") || editor.tool === "spotHealing" || stampToolActive
    property bool layerContextMenuVisible: layerContextMenu.visible
    property real parameterSliderWidth: 0
    property int preferencesPage: 0
    property string interfaceLanguage: languages.language
    property string newActionText: newAction.text
    property int recentFileCount: recentFiles.entries.length
    property bool layerPreviewPending: false
    property string curvePickerMode: ""
    property string colorPickerMode: ""
    property int inspectorTab: 0
    property bool inspectorCollapsed: false
    property real dockWidth: 300
    readonly property var dockPanelIds: ["histogram", "navigator", "color", "layers", "channels", "history", "information"]
    property var dockPanelOrder: ["histogram", "navigator", "color", "layers", "channels", "history", "information"]
    property var dockPanelColumns: ({histogram: 0, navigator: 0, color: 0, layers: 0, channels: 0, history: 0, information: 0})
    property var dockPanelGroups: ({histogram: "histogram", navigator: "navigator", color: "color", layers: "layers", channels: "channels", history: "history", information: "information"})
    property var dockGroupActive: ({histogram: "histogram", navigator: "navigator", color: "color", layers: "layers", channels: "channels", history: "history", information: "information"})
    property var dockPanelVisibility: ({histogram: true, navigator: true, color: true, layers: true, channels: true, history: true, information: false})
    property bool workspaceRestored: false
    property string informationTool: editor.tool
    property bool informationFloating: true
    property point informationPosition: Qt.point(0, 0)
    property color informationColor: "transparent"
    property bool informationPointerValid: false
    property var colorSamples: []
    property var sampleDocumentSession: -1
    property bool pickerDragging: false
    property color pickerPreviousColor: "#000000"
    property color pickerSampleColor: "#000000"
    property point pickerPosition: Qt.point(0,0)
    readonly property bool eyedropperToolActive: editor.tool === "picker" || editor.tool === "colorSampler"
    property point rulerStart: Qt.point(0,0)
    property point rulerEnd: Qt.point(0,0)
    property bool rulerVisible: false
    property string dockDropPanelId: ""
    property string dockDropZone: ""
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
        invert: [],
        gradientMap: [],
        gaussianBlur: [{key:"radius",label:qsTr("Rayon"),from:0.1,to:100,step:0.5}],
        motionBlur: [{key:"distance",label:qsTr("Distance"),from:1,to:200,step:1},{key:"angle",label:qsTr("Angle"),from:-180,to:180,step:1}],
        boxBlur: [{key:"radius",label:qsTr("Rayon"),from:1,to:100,step:1}],
        unsharpMask: [{key:"amount",label:qsTr("Gain (%)"),from:0,to:300,step:1},{key:"radius",label:qsTr("Rayon"),from:0.5,to:20,step:0.5},{key:"threshold",label:qsTr("Seuil"),from:0,to:255,step:1}],
        highPass: [{key:"radius",label:qsTr("Rayon"),from:0.5,to:100,step:0.5}],
        mosaic: [{key:"cellSize",label:qsTr("Taille des cellules"),from:2,to:200,step:1}],
        noise: [{key:"amount",label:qsTr("Quantité (%)"),from:0,to:100,step:1}]
    })
    property var toolCatalog: [
        {name:"move", icon:"↖", label:qsTr("Déplacement"), implemented:true},
        {name:"artboard", icon:"▱", label:qsTr("Plan de travail"), implemented:false},
        {name:"select", icon:"⬚", label:qsTr("Sélection rectangulaire"), implemented:true},
        {name:"ellipticalSelect", icon:"◯", label:qsTr("Sélection elliptique"), implemented:true},
        {name:"singleRowSelect", icon:"┄", label:qsTr("Rangée unique"), implemented:true},
        {name:"singleColumnSelect", icon:"┊", label:qsTr("Colonne unique"), implemented:true},
        {name:"selectionBrush", icon:"◌", label:qsTr("Pinceau de sélection"), implemented:false},
        {name:"lasso", icon:"♧", label:qsTr("Lasso"), implemented:true},
        {name:"polygonalLasso", icon:"⌁", label:qsTr("Lasso polygonal"), implemented:true},
        {name:"magneticLasso", icon:"⌁", label:qsTr("Lasso magnétique"), implemented:false},
        {name:"objectSelection", icon:"▧", label:qsTr("Sélection d’objet"), implemented:false},
        {name:"quickSelection", icon:"◌", label:qsTr("Sélection rapide"), implemented:true},
        {name:"magicWand", icon:"✦", label:qsTr("Baguette magique"), implemented:true},
        {name:"crop", icon:"⌗", label:qsTr("Recadrage"), implemented:true},
        {name:"perspectiveCrop", icon:"⌗", label:qsTr("Recadrage en perspective"), implemented:false},
        {name:"slice", icon:"◢", label:qsTr("Tranche"), implemented:false},
        {name:"sliceSelect", icon:"◁", label:qsTr("Sélection de tranche"), implemented:false},
        {name:"frame", icon:"⊠", label:qsTr("Cadre"), implemented:false},
        {name:"picker", icon:"", label:qsTr("Pipette"), implemented:true},
        {name:"colorSampler", icon:"", label:qsTr("Échantillonnage de couleur"), implemented:true},
        {name:"ruler", icon:"", label:qsTr("Règle"), implemented:true},
        {name:"note", icon:"▤", label:qsTr("Annotation"), implemented:false},
        {name:"count", icon:"¹²³", label:qsTr("Comptage"), implemented:false},
        {name:"spotHealing", icon:"✚", label:qsTr("Correcteur localisé"), implemented:true},
        {name:"remove", icon:"✦", label:qsTr("Suppression"), implemented:true},
        {name:"healing", icon:"▰", label:qsTr("Correcteur"), implemented:true},
        {name:"patch", icon:"▧", label:qsTr("Pièce"), implemented:true},
        {name:"contentAwareMove", icon:"↝", label:qsTr("Déplacement basé sur le contenu"), implemented:true},
        {name:"redEye", icon:"◉", label:qsTr("Œil rouge"), implemented:true},
        {name:"brush", icon:"╱", label:qsTr("Pinceau"), implemented:true},
        {name:"pencil", icon:"✎", label:qsTr("Crayon"), implemented:true},
        {name:"colorReplacement", icon:"♢", label:qsTr("Remplacement de couleur"), implemented:true},
        {name:"mixerBrush", icon:"♨", label:qsTr("Pinceau mélangeur"), implemented:true},
        {name:"cloneStamp", icon:"♟", label:qsTr("Tampon de duplication"), implemented:true},
        {name:"patternStamp", icon:"♟", label:qsTr("Tampon de motif"), implemented:true},
        {name:"historyBrush", icon:"◴", label:qsTr("Forme d’historique"), implemented:true},
        {name:"artHistoryBrush", icon:"◔", label:qsTr("Forme d’historique artistique"), implemented:true},
        {name:"eraser", icon:"▱", label:qsTr("Gomme"), implemented:true},
        {name:"backgroundEraser", icon:"◇", label:qsTr("Gomme d’arrière-plan"), implemented:false},
        {name:"magicEraser", icon:"✧", label:qsTr("Gomme magique"), implemented:false},
        {name:"gradient", icon:"▨", label:qsTr("Dégradé"), implemented:true},
        {name:"fill", icon:"◩", label:qsTr("Remplissage contigu"), implemented:true},
        {name:"blur", icon:"●", label:qsTr("Flou"), implemented:true},
        {name:"sharpen", icon:"▲", label:qsTr("Netteté"), implemented:true},
        {name:"smudge", icon:"≈", label:qsTr("Doigt"), implemented:true},
        {name:"adjustmentBrush", icon:"◒", label:qsTr("Pinceau de réglage"), implemented:false},
        {name:"dodge", icon:"●", label:qsTr("Densité −"), implemented:true},
        {name:"burn", icon:"◒", label:qsTr("Densité +"), implemented:true},
        {name:"sponge", icon:"◉", label:qsTr("Éponge"), implemented:true},
        {name:"pen", icon:"◇", label:qsTr("Plume"), implemented:false},
        {name:"freeformPen", icon:"♢", label:qsTr("Plume libre"), implemented:false},
        {name:"curvaturePen", icon:"♧", label:qsTr("Plume de courbure"), implemented:false},
        {name:"addAnchor", icon:"+◇", label:qsTr("Ajout de point d’ancrage"), implemented:false},
        {name:"deleteAnchor", icon:"−◇", label:qsTr("Suppression de point d’ancrage"), implemented:false},
        {name:"convertPoint", icon:"⌞", label:qsTr("Conversion de point"), implemented:false},
        {name:"text", icon:"T", label:qsTr("Texte horizontal rasterisé"), implemented:true},
        {name:"verticalText", icon:"↕T", label:qsTr("Texte vertical"), implemented:false},
        {name:"verticalTypeMask", icon:"↕T", label:qsTr("Masque de texte vertical"), implemented:false},
        {name:"horizontalTypeMask", icon:"T", label:qsTr("Masque de texte horizontal"), implemented:false},
        {name:"pathSelection", icon:"➤", label:qsTr("Sélection de tracé"), implemented:false},
        {name:"directSelection", icon:"➢", label:qsTr("Sélection directe"), implemented:false},
        {name:"rectangle", icon:"□", label:qsTr("Rectangle"), implemented:true},
        {name:"ellipse", icon:"○", label:qsTr("Ellipse"), implemented:true},
        {name:"triangle", icon:"△", label:qsTr("Triangle"), implemented:true},
        {name:"polygon", icon:"⬡", label:qsTr("Polygone"), implemented:true},
        {name:"star", icon:"☆", label:qsTr("Étoile"), implemented:true},
        {name:"line", icon:"╲", label:qsTr("Ligne"), implemented:true},
        {name:"customShape", icon:"✾", label:qsTr("Forme personnalisée"), implemented:false},
        {name:"hand", icon:"✥", label:qsTr("Main"), implemented:true},
        {name:"rotateView", icon:"⟳", label:qsTr("Rotation de l’affichage"), implemented:true},
        {name:"zoomTool", icon:"⌕", label:qsTr("Zoom"), implemented:true}
    ]
    property var toolLayout: []
    property int toolbarRevision: 0
    property string toolMenuGroupId: ""
    property var activeInfo: {
        const list = editor.layers
        for (let i=0; i<list.length; ++i) if (list[i].layerIndex === editor.activeLayer) return list[i]
        return {name:"", opacity:1, blend:0, locked:false, kind:"pixel", parameters:({}), hasMask:false, maskEnabled:true, clippingMask:false, groupId:"", groupCollapsed:false, selected:false}
    }
    property string toolName: {
        for (let i=0; i<toolCatalog.length; ++i) if (toolCatalog[i].name === editor.tool) return toolCatalog[i].label
        return qsTr("Pinceau")
    }
    function toolData(name) {
        for (let i = 0; i < toolCatalog.length; ++i)
            if (toolCatalog[i].name === name) return toolCatalog[i]
        return null
    }
    function cloneToolLayout(source) {
        const result = []
        for (let i = 0; i < source.length; ++i) {
            const group = source[i]
            result.push({id: String(group.id), shortcut: String(group.shortcut),
                         key: String(group.key || ""), active: String(group.active),
                         tools: Array.from(group.tools || []),
                         hidden: Array.from(group.hidden || [])})
        }
        return result
    }
    function refreshToolLayout() {
        toolLayout = cloneToolLayout(workspaceSettings.toolLayout || [])
        ++toolbarRevision
    }
    function groupById(groupId) {
        for (let i = 0; i < toolLayout.length; ++i)
            if (toolLayout[i].id === groupId) return toolLayout[i]
        return null
    }
    function groupTools(group, includeHidden, includeNotImplemented) {
        const result = []
        if (!group) return result
        const hidden = group.hidden || []
        for (let i = 0; i < group.tools.length; ++i) {
            const tool = toolData(String(group.tools[i]))
            if (tool && (includeNotImplemented || tool.implemented)
                    && (includeHidden || hidden.indexOf(tool.name) < 0)) result.push(tool)
        }
        return result
    }
    function visibleToolGroups() {
        const result = []
        for (let i = 0; i < toolLayout.length; ++i)
            if (groupTools(toolLayout[i], false, false).length > 0) result.push(toolLayout[i])
        return result
    }
    function toolGroupContains(group, name) {
        return group && Array.from(group.tools || []).indexOf(name) >= 0
    }
    function toolGroupLabel(group) {
        const labels = ({move: qsTr("Déplacement"), selection: qsTr("Sélections géométriques"),
            lasso: qsTr("Lassos"), smartSelection: qsTr("Sélections intelligentes"),
            crop: qsTr("Recadrage et tranches"), frame: qsTr("Cadre"),
            measure: qsTr("Mesure et annotation"), healing: qsTr("Correction"),
            brush: qsTr("Dessin"), stamp: qsTr("Tampons"),
            historyBrush: qsTr("Historique"), eraser: qsTr("Gommes"),
            paint: qsTr("Dégradé et remplissage"), blur: qsTr("Retouche"),
            adjustmentBrush: qsTr("Réglage local"), dodge: qsTr("Densité"),
            pen: qsTr("Plumes"), type: qsTr("Texte"),
            pathSelection: qsTr("Sélection de tracé"), shape: qsTr("Formes"),
            hand: qsTr("Navigation"), zoom: qsTr("Zoom")})
        return labels[group.id] || group.id
    }
    function toolGroupShortcut(group) {
        const configured = group && group.shortcut.length > 0
                         ? shortcuts.displaySequence(group.shortcut) : ""
        return configured.length > 0 ? configured : (group ? group.key : "")
    }
    function activeToolForGroup(group) {
        const visible = groupTools(group, false, false)
        if (visible.length === 0) return null
        if (toolGroupContains(group, editor.tool)) {
            for (let i = 0; i < visible.length; ++i)
                if (visible[i].name === editor.tool) return visible[i]
        }
        for (let i = 0; i < visible.length; ++i)
            if (visible[i].name === group.active) return visible[i]
        return visible[0]
    }
    function saveToolLayout(layout) {
        toolLayout = cloneToolLayout(layout)
        ++toolbarRevision
        workspaceSettings.saveToolLayout(toolLayout)
    }
    function activateGroupTool(groupId, toolName) {
        const layout = cloneToolLayout(toolLayout)
        for (let i = 0; i < layout.length; ++i) {
            if (layout[i].id !== groupId) continue
            layout[i].active = toolName
            saveToolLayout(layout)
            window.spacePressed = false
            editor.tool = toolName
            workspace.forceActiveFocus()
            return
        }
    }
    function selectActiveGroupTool(groupId) {
        const tool = activeToolForGroup(groupById(groupId))
        if (tool) {
            window.spacePressed = false
            editor.tool = tool.name
            workspace.forceActiveFocus()
        }
    }
    function openToolGroupMenu(groupId, anchor) {
        const group = groupById(groupId)
        if (groupTools(group, false, true).length < 2) return
        toolMenuGroupId = groupId
        if (anchor) toolGroupMenu.popup(anchor, 0, anchor.height)
        else toolGroupMenu.popup()
    }
    function moveToolGroup(index, delta) {
        const target = index + delta
        if (index < 0 || target < 0 || target >= toolLayout.length) return
        const layout = cloneToolLayout(toolLayout)
        const value = layout.splice(index, 1)[0]
        layout.splice(target, 0, value)
        saveToolLayout(layout)
    }
    function moveToolInGroup(groupId, index, delta) {
        const layout = cloneToolLayout(toolLayout)
        for (let i = 0; i < layout.length; ++i) {
            if (layout[i].id !== groupId) continue
            const target = index + delta
            if (target < 0 || target >= layout[i].tools.length) return
            const value = layout[i].tools.splice(index, 1)[0]
            layout[i].tools.splice(target, 0, value)
            saveToolLayout(layout)
            return
        }
    }
    function setToolVisible(groupId, toolName, visible) {
        const layout = cloneToolLayout(toolLayout)
        for (let i = 0; i < layout.length; ++i) {
            if (layout[i].id !== groupId) continue
            const hidden = layout[i].hidden
            const at = hidden.indexOf(toolName)
            if (visible && at >= 0) hidden.splice(at, 1)
            else if (!visible && at < 0) hidden.push(toolName)
            let nextTool = ""
            if (!visible && layout[i].active === toolName) {
                for (let j = 0; j < layout[i].tools.length; ++j) {
                    if (hidden.indexOf(layout[i].tools[j]) < 0) {
                        nextTool = layout[i].tools[j]
                        break
                    }
                }
                if (nextTool.length > 0) layout[i].active = nextTool
            }
            saveToolLayout(layout)
            if (!visible && editor.tool === toolName && nextTool.length > 0)
                editor.tool = nextTool
            return
        }
    }
    function resetToolLayout() { workspaceSettings.resetToolLayout() }
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
    function dockPanelRow(panelId) {
        const group = dockPanelGroup(panelId)
        const column = dockPanelColumn(panelId)
        const seen = ({})
        let row = 0
        for (let i = 0; i < dockPanelOrder.length; ++i) {
            const candidate = dockPanelOrder[i]
            if (dockPanelColumn(candidate) !== column) continue
            const candidateGroup = dockPanelGroup(candidate)
            if (candidateGroup === group) return row
            if (!seen[candidateGroup]) { seen[candidateGroup] = true; ++row }
        }
        return row
    }
    function dockPanelColumn(panelId) {
        return Number(dockPanelColumns[panelId] || 0)
    }
    function dockPanelGroup(panelId) {
        return String(dockPanelGroups[panelId] || panelId)
    }
    function dockPanelTitle(panelId) {
        if (panelId === "histogram") return qsTr("Histogramme")
        if (panelId === "navigator") return qsTr("Navigateur")
        if (panelId === "color") return qsTr("Couleur")
        if (panelId === "layers") return qsTr("Calques")
        if (panelId === "channels") return qsTr("Couches")
        if (panelId === "information") return qsTr("Informations")
        return qsTr("Historique")
    }
    function dockPanelVisible(panelId) {
        return dockPanelVisibility[panelId] !== false
    }
    function dockPanelObject(panelId) {
        if (panelId === "histogram") return histogramDock
        if (panelId === "navigator") return navigatorDock
        if (panelId === "color") return colorDock
        if (panelId === "layers") return layersDock
        if (panelId === "channels") return channelsDock
        if (panelId === "information") return informationDock
        return historyDock
    }
    function dockGroupTabs(panelId) {
        const group = dockPanelGroup(panelId)
        const tabs = []
        for (let i = 0; i < dockPanelOrder.length; ++i) {
            const id = dockPanelOrder[i]
            if (dockPanelGroup(id) === group) tabs.push({panelId:id, title:dockPanelTitle(id)})
        }
        return tabs
    }
    function activeDockPanel(panelId) {
        const group = dockPanelGroup(panelId)
        return String(dockGroupActive[group] || panelId)
    }
    function selectDockPanel(panelId) {
        const group = dockPanelGroup(panelId)
        const active = ({})
        for (const key in dockGroupActive) active[key] = dockGroupActive[key]
        active[group] = panelId
        dockGroupActive = active
    }
    function ungroupDockPanel(panelId) {
        const oldGroup = dockPanelGroup(panelId)
        const remaining = []
        for (let i = 0; i < dockPanelOrder.length; ++i) {
            const id = dockPanelOrder[i]
            if (id !== panelId && dockPanelGroup(id) === oldGroup) remaining.push(id)
        }
        const groups = ({})
        for (const key in dockPanelGroups) groups[key] = dockPanelGroups[key]
        groups[panelId] = panelId
        if (oldGroup === panelId && remaining.length > 0) {
            const remainingGroup = remaining[0]
            for (let i = 0; i < remaining.length; ++i) groups[remaining[i]] = remainingGroup
        }
        dockPanelGroups = groups
        const active = ({})
        for (const key in dockGroupActive) active[key] = dockGroupActive[key]
        active[panelId] = panelId
        if (oldGroup === panelId && remaining.length > 0) {
            const remainingGroup = remaining[0]
            active[remainingGroup] = active[oldGroup] === panelId ? remainingGroup : active[oldGroup]
        } else if (active[oldGroup] === panelId && remaining.length > 0) {
            active[oldGroup] = remaining[0]
        }
        dockGroupActive = active
    }
    function groupDockPanels(sourceId, targetId) {
        if (sourceId === targetId) return
        const targetGroup = dockPanelGroup(targetId)
        const columns = ({})
        for (const key in dockPanelColumns) columns[key] = dockPanelColumns[key]
        columns[sourceId] = dockPanelColumn(targetId)
        dockPanelColumns = columns
        const groups = ({})
        for (const key in dockPanelGroups) groups[key] = dockPanelGroups[key]
        groups[sourceId] = targetGroup
        dockPanelGroups = groups
        const active = ({})
        for (const key in dockGroupActive) active[key] = dockGroupActive[key]
        active[targetGroup] = sourceId
        dockGroupActive = active
    }
    function dockPanelAt(globalX, globalY, sourceId) {
        const panels = [histogramDock, navigatorDock, colorDock, layersDock, channelsDock, historyDock, informationDock]
        for (let i = 0; i < panels.length; ++i) {
            const panel = panels[i]
            if (panel.panelId === sourceId || panel.floating || !panel.visible) continue
            const point = panel.mapFromGlobal(globalX, globalY)
            if (point.x >= 0 && point.x <= panel.width && point.y >= 0 && point.y <= panel.height) {
                let zone = "after"
                if (point.y <= 16) zone = "before"
                else if (point.y <= panel.headerHeight) zone = "tab"
                else if (point.y >= panel.height - 16) zone = "after"
                else if (point.y < panel.height / 2) zone = "before"
                return {panelId:panel.panelId, zone:zone}
            }
        }
        let nearest = null
        let nearestDistance = Number.MAX_VALUE
        for (let i = 0; i < panels.length; ++i) {
            const panel = panels[i]
            if (panel.panelId === sourceId || panel.floating || !panel.visible) continue
            const topLeft = panel.mapToGlobal(0, 0)
            if (globalX < topLeft.x || globalX > topLeft.x + panel.width) continue
            const topDistance = Math.abs(globalY - topLeft.y)
            const bottomDistance = Math.abs(globalY - topLeft.y - panel.height)
            if (topDistance < nearestDistance) {
                nearestDistance = topDistance
                nearest = {panelId:panel.panelId, zone:"before"}
            }
            if (bottomDistance < nearestDistance) {
                nearestDistance = bottomDistance
                nearest = {panelId:panel.panelId, zone:"after"}
            }
        }
        if (nearest !== null && nearestDistance <= 80) return nearest
        return null
    }
    function updateDockDropIndicator(sourceId, globalX, globalY) {
        const target = dockPanelAt(globalX, globalY, sourceId)
        dockDropPanelId = target === null ? "" : target.panelId
        dockDropZone = target === null ? "" : target.zone
    }
    function clearDockDropIndicator() {
        dockDropPanelId = ""
        dockDropZone = ""
    }
    function setDockPanelVisible(panelId, shown) {
        const visibility = ({})
        for (const key in dockPanelVisibility) visibility[key] = dockPanelVisibility[key]
        visibility[panelId] = shown
        dockPanelVisibility = visibility
        if (shown) {
            selectDockPanel(panelId)
            if (panelId === "information" && informationFloating && !informationDock.floating) {
                if (informationDock.floatingX === 0 && informationDock.floatingY === 0) {
                    informationDock.floatingX = window.x + Math.max(20, window.width - dockWidth - 330)
                    informationDock.floatingY = window.y + 110
                }
                informationDock.setPanelFloating(true)
                window.requestActivate()
                Qt.callLater(drawingArea.refreshSystemCursor)
            }
        } else {
            const group = dockPanelGroup(panelId)
            if (activeDockPanel(panelId) === panelId) {
                for (let i = 0; i < dockPanelOrder.length; ++i) {
                    const candidate = dockPanelOrder[i]
                    if (candidate !== panelId && dockPanelGroup(candidate) === group &&
                        dockPanelVisible(candidate)) {
                        selectDockPanel(candidate)
                        break
                    }
                }
            }
        }
    }
    function toggleDockPanel(panelId) {
        setDockPanelVisible(panelId, !dockPanelVisible(panelId))
    }
    function workspacePanelState(panelId) {
        const panel = dockPanelObject(panelId)
        return {
            collapsed: panel.collapsed,
            dockedHeight: panel.dockedHeight,
            floating: panelId === "information" ? informationFloating : panel.floating,
            floatingX: panel.floatingX,
            floatingY: panel.floatingY,
            floatingWidth: panel.floatingWidth,
            floatingHeight: panel.floatingHeight
        }
    }
    function saveWorkspaceLayout() {
        if (!workspaceRestored) return
        const panels = ({})
        for (let i = 0; i < dockPanelIds.length; ++i) {
            const id = dockPanelIds[i]
            panels[id] = workspacePanelState(id)
        }
        workspaceSettings.saveLayout({
            version: 1,
            dockWidth: dockWidth,
            order: dockPanelOrder,
            columns: dockPanelColumns,
            groups: dockPanelGroups,
            active: dockGroupActive,
            visibility: dockPanelVisibility,
            panels: panels
        })
    }
    function scheduleWorkspaceSave() {
        if (workspaceRestored) workspaceSaveTimer.restart()
    }
    function restoreWorkspaceLayout() {
        const saved = workspaceSettings.layout
        if (saved && Number(saved.version) === 1) {
            const savedOrder = saved.order || []
            const order = []
            for (let i = 0; i < savedOrder.length; ++i) {
                const id = String(savedOrder[i])
                if (dockPanelIds.indexOf(id) >= 0 && order.indexOf(id) < 0) order.push(id)
            }
            for (let i = 0; i < dockPanelIds.length; ++i)
                if (order.indexOf(dockPanelIds[i]) < 0) order.push(dockPanelIds[i])
            dockPanelOrder = order
            const columns = ({})
            const groups = ({})
            const active = ({})
            const visibility = ({})
            for (let i = 0; i < dockPanelIds.length; ++i) {
                const id = dockPanelIds[i]
                columns[id] = saved.columns && saved.columns[id] === 1 ? 1 : 0
                const savedGroup = saved.groups ? String(saved.groups[id] || id) : id
                groups[id] = dockPanelIds.indexOf(savedGroup) >= 0 ? savedGroup : id
                const savedActive = saved.active ? String(saved.active[savedGroup] || id) : id
                active[groups[id]] = dockPanelIds.indexOf(savedActive) >= 0 ? savedActive : id
                visibility[id] = id === "information" ? Boolean(saved.visibility && saved.visibility[id])
                                                       : !saved.visibility || saved.visibility[id] !== false
            }
            dockPanelColumns = columns
            dockPanelGroups = groups
            dockGroupActive = active
            dockPanelVisibility = visibility
            dockWidth = Math.max(230, Math.min(760, Number(saved.dockWidth || 300)))
            const panelStates = saved.panels || ({})
            for (let i = 0; i < dockPanelIds.length; ++i) {
                const id = dockPanelIds[i]
                const state = panelStates[id]
                if (!state) continue
                const panel = dockPanelObject(id)
                if (id === "information") informationFloating = Boolean(state.floating)
                panel.collapsed = Boolean(state.collapsed)
                panel.dockedHeight = Math.max(panel.minimumContentHeight + panel.headerHeight,
                                              Number(state.dockedHeight || panel.dockedHeight))
                panel.floatingX = Number(state.floatingX || panel.floatingX)
                panel.floatingY = Number(state.floatingY || panel.floatingY)
                panel.floatingWidth = Math.max(230, Number(state.floatingWidth || panel.floatingWidth))
                panel.floatingHeight = Math.max(110, Number(state.floatingHeight || panel.floatingHeight))
            }
            Qt.callLater(function() {
                for (let i = 0; i < dockPanelIds.length; ++i) {
                    const id = dockPanelIds[i]
                    const state = panelStates[id]
                    if (state && state.floating && dockPanelVisible(id))
                        dockPanelObject(id).setPanelFloating(true)
                }
            })
        }
        workspaceRestored = true
    }
    function moveDockPanelTo(sourceId, targetId, after, column) {
        if (dockGroupTabs(sourceId).length > 1) ungroupDockPanel(sourceId)
        const next = dockPanelOrder.slice()
        const from = next.indexOf(sourceId)
        if (from >= 0) next.splice(from, 1)
        let target = targetId.length > 0 ? next.indexOf(targetId) : next.length
        if (target < 0) target = next.length
        if (after && targetId.length > 0) ++target
        next.splice(target, 0, sourceId)
        dockPanelOrder = next
        const columns = ({})
        for (const key in dockPanelColumns) columns[key] = dockPanelColumns[key]
        columns[sourceId] = column
        dockPanelColumns = columns
        if (column === 1) dockWidth = Math.max(dockWidth, 560)
    }
    function handleDockDrop(panelId, globalX, globalY, horizontalDelta, verticalDelta) {
        const target = dockPanelAt(globalX, globalY, panelId)
        if (target !== null && target.zone === "tab") { groupDockPanels(panelId, target.panelId); return }
        if (target !== null) {
            moveDockPanelTo(panelId, target.panelId, target.zone === "after",
                            dockPanelColumn(target.panelId))
            return
        }
        const dockPoint = dockArea.mapFromGlobal(globalX, globalY)
        const column = dockWidth >= 500 && dockPoint.x > dockArea.width / 2 ? 1 : 0
        const ids = dockPanelOrder.filter(function(id) { return id !== panelId && dockPanelColumn(id) === column })
        moveDockPanelTo(panelId, ids.length > 0 ? ids[ids.length - 1] : "", true, column)
    }
    function zoomAt(value, pointerX, pointerY) {
        const oldZoom=zoom
        const documentX=(viewport.contentX+pointerX-artboard.x)/oldZoom
        const documentY=(viewport.contentY+pointerY-artboard.y)/oldZoom
        setZoom(value)
        viewport.contentX=clampScroll(artboard.x+documentX*zoom-pointerX,viewport.contentWidth-viewport.width)
        viewport.contentY=clampScroll(artboard.y+documentY*zoom-pointerY,viewport.contentHeight-viewport.height)
    }
    function showInformationPanel() {
        if (!dockPanelVisible("information")) setDockPanelVisible("information", true)
        informationDock.collapsed = false
        selectDockPanel("information")
    }
    function updateInformationPointer(x, y) {
        informationPosition = Qt.point(Math.floor(x), Math.floor(y))
        informationPointerValid = x >= 0 && y >= 0 && x < editor.width && y < editor.height
        if (dockPanelVisible("information") && !informationPointerTimer.running)
            informationPointerTimer.start()
    }
    function refreshInformationColor() {
        if (dockPanelVisible("information") && informationPointerValid)
            informationColor = editor.sampleToolColor(informationPosition.x, informationPosition.y)
    }
    function colorText(color) {
        return "R " + Math.round(color.r*255) + "  V " + Math.round(color.g*255)
                + "  B " + Math.round(color.b*255)
    }
    function nearestColorSample(x,y) {
        let nearest=-1, best=144/(zoom*zoom)
        for(let i=0;i<colorSamples.length;++i) {
            const dx=colorSamples[i].x-x,dy=colorSamples[i].y-y,d=dx*dx+dy*dy
            if(d<best){best=d;nearest=i}
        }
        return nearest
    }
    function updateColorSample(index,x,y) {
        if(x<0 || y<0 || x>=editor.width || y>=editor.height) return
        const sampled=editor.sampleToolColor(Math.floor(x),Math.floor(y))
        const next=Array.from(colorSamples)
        next[index]={x:x,y:y,color:sampled,label:colorText(sampled)}
        colorSamples=next
        showInformationPanel()
    }
    function addColorSample(x,y) {
        if(colorSamples.length>=4 || x<0 || y<0 || x>=editor.width || y>=editor.height) return -1
        const index=colorSamples.length
        updateColorSample(index,x,y)
        return index
    }
    function refreshColorSamples() {
        colorSamples=colorSamples.map(function(sample) {
            const color=editor.sampleToolColor(Math.floor(sample.x),Math.floor(sample.y))
            return {x:sample.x,y:sample.y,color:color,label:colorText(color)}
        })
    }
    function removeColorSample(index) {
        if(index<0 || index>=colorSamples.length) return
        const next=Array.from(colorSamples);next.splice(index,1);colorSamples=next
    }
    function removeNearestColorSample(x,y) { removeColorSample(nearestColorSample(x,y)) }
    function activateMaskThumbnail(layerIndex, modifiers) {
        editor.activeLayer = layerIndex
        editor.setEditingMask(true)
        if (modifiers & Qt.AltModifier)
            editor.toggleMaskOnlyView(layerIndex)
        else {
            editor.showCompositeView()
            layerPropertiesDialog.open()
        }
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
    function openPreferences() { preferencesDialog.open() }
    onClosing: function(event) {
        saveWorkspaceLayout()
        if (!allowClose && editor.modified) { event.accepted = false; guard("close") }
    }
    onDockWidthChanged: scheduleWorkspaceSave()
    onDockPanelOrderChanged: scheduleWorkspaceSave()
    onDockPanelColumnsChanged: scheduleWorkspaceSave()
    onDockPanelGroupsChanged: scheduleWorkspaceSave()
    onDockGroupActiveChanged: scheduleWorkspaceSave()
    onDockPanelVisibilityChanged: scheduleWorkspaceSave()
    Component.onCompleted: {
        sampleDocumentSession=editor.documentSession
        refreshToolLayout()
        restoreWorkspaceLayout()
        Qt.callLater(fitCanvas)
    }
    Connections {
        target: workspaceSettings
        function onToolLayoutChanged() { window.refreshToolLayout() }
    }
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
    Timer { id: informationPointerTimer; interval: 16; onTriggered: window.refreshInformationColor() }
    onInformationToolChanged: {
        if (workspaceRestored && ["picker", "colorSampler", "ruler"].indexOf(informationTool) >= 0)
            showInformationPanel()
    }
    onCurvePickerModeChanged: if (curvePickerMode.length) showInformationPanel()
    onColorPickerModeChanged: if (colorPickerMode.length) showInformationPanel()
    Timer { id: statusTimer; interval: 5000; onTriggered: window.status = qsTr("Prêt à créer") }
    Timer { id: layerPreviewTimer; interval: 16; onTriggered: window.flushLayerPreview() }
    Timer { id: workspaceSaveTimer; interval: 250; onTriggered: window.saveWorkspaceLayout() }

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
    Action { id: preferencesAction; objectName: "preferencesAction"; text: Qt.platform.os === "osx" ? "Preferences…" : qsTr("Réglages…"); shortcut: (shortcuts.revision, shortcuts.sequence("preferences")); enabled: !window.modalOpen; onTriggered: window.openPreferences() }
    Instantiator {
        model: (window.toolbarRevision, window.toolLayout)
        delegate: Shortcut {
            required property var modelData
            sequence: (shortcuts.revision, shortcuts.sequence(modelData.shortcut))
            enabled: !window.editingText && !window.modalOpen
            onActivated: window.selectActiveGroupTool(modelData.id)
        }
    }
    Shortcut {
        enabled: !window.modalOpen
        sequence: "Escape"
        onActivated: {
            const movingContent = editor.removeHasPending || editor.removeBusy || editor.tool === "remove" || editor.contentMoveBusy
                    || (editor.tool === "contentAwareMove" && editor.patchDragging)
            editor.cancelStroke()
            editor.cancelPolygonLasso()
            if (!movingContent) editor.deselect()
        }
    }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("zoomFit")); onActivated: fitCanvas() }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("zoomActual")); onActivated: setZoom(1) }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("toolRotate")); onActivated: window.activateGroupTool("hand", "rotateView") }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("brushSmaller")); onActivated: editor.brushSize = editor.brushSize-5 }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("brushLarger")); onActivated: editor.brushSize = editor.brushSize+5 }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("brushSofter")); onActivated: editor.brushHardness = editor.brushHardness-0.1 }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("brushHarder")); onActivated: editor.brushHardness = editor.brushHardness+0.1 }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("defaultColors")); onActivated: editor.resetColors() }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: (shortcuts.revision, shortcuts.sequence("swapColors")); onActivated: editor.swapColors() }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: Qt.platform.os === "osx" ? "Alt+Delete" : "Alt+Backspace"; onActivated: editor.fillWithColor(true, false) }
    Shortcut { objectName: "backgroundFillShortcut"; enabled: !window.editingText && !window.modalOpen; sequences: Qt.platform.os === "osx" ? ["Meta+Backspace", "Meta+Delete"] : ["Ctrl+Backspace"]; onActivated: editor.fillWithColor(false, false) }
    Shortcut {
        objectName: "maskColorFillShortcut"
        enabled: editor.editingMask && !window.editingText && !window.modalOpen
        sequences: Qt.platform.os === "osx" ? ["Shift+Backspace", "Shift+Delete"] : ["Shift+Backspace"]
        onActivated: {
            fillingMaskColor = true
            changingSecondary = false
            changingSolidColor = false
            changingBlackWhiteTint = false
            changingPhotoFilterColor = false
            colorDialog.selectedColor = editor.color
            colorDialog.open()
        }
    }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: Qt.platform.os === "osx" ? "Alt+Shift+Delete" : "Alt+Shift+Backspace"; onActivated: editor.fillWithColor(true, true) }
    Shortcut { enabled: !window.editingText && !window.modalOpen; sequence: Qt.platform.os === "osx" ? "Meta+Shift+Delete" : "Ctrl+Shift+Backspace"; onActivated: editor.fillWithColor(false, true) }

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
                DarkMenuItem { text: qsTr("Couleur unie…"); onTriggered: addAdjustmentLayer("solidColor") }
                DarkMenuItem { text: qsTr("Lumière"); onTriggered: addAdjustmentLayer("light") }
                DarkMenuItem { text: qsTr("Niveaux"); onTriggered: addAdjustmentLayer("levels") }
                DarkMenuItem { text: qsTr("Courbes"); onTriggered: addAdjustmentLayer("curves") }
                DarkMenuItem { text: qsTr("Couleur"); onTriggered: addAdjustmentLayer("color") }
                DarkMenuItem { text: qsTr("Balance des couleurs"); onTriggered: addAdjustmentLayer("colorBalance") }
                DarkMenuItem { text: qsTr("Noir et blanc"); onTriggered: addAdjustmentLayer("blackWhite") }
                DarkMenuItem { text: qsTr("Filtre photo"); onTriggered: addAdjustmentLayer("photoFilter") }
                DarkMenuItem { text: qsTr("Mélangeur de couches"); onTriggered: addAdjustmentLayer("channelMixer") }
                DarkMenuItem { text: qsTr("Style colorimétrique (LUT)"); onTriggered: addAdjustmentLayer("colorLookup") }
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
            DarkMenuItem { objectName: "settingsLightMenuItem"; text: qsTr("Lumière"); onTriggered: addAdjustmentLayer("light") }
            DarkMenuItem { text: qsTr("Niveaux"); onTriggered: addAdjustmentLayer("levels") }
            DarkMenuItem { text: qsTr("Courbes"); onTriggered: addAdjustmentLayer("curves") }
            DarkMenuItem { text: qsTr("Couleur"); onTriggered: addAdjustmentLayer("color") }
            DarkMenuItem { text: qsTr("Balance des couleurs"); onTriggered: addAdjustmentLayer("colorBalance") }
            MenuSeparator {}
            DarkMenuItem { text: qsTr("Noir et blanc"); onTriggered: addAdjustmentLayer("blackWhite") }
            DarkMenuItem { text: qsTr("Filtre photo"); onTriggered: addAdjustmentLayer("photoFilter") }
            DarkMenuItem { text: qsTr("Mélangeur de couches"); onTriggered: addAdjustmentLayer("channelMixer") }
            DarkMenuItem { text: qsTr("Style colorimétrique (LUT)"); onTriggered: addAdjustmentLayer("colorLookup") }
            MenuSeparator {}
            DarkMenuItem { text: qsTr("Négatif"); onTriggered: addAdjustmentLayer("invert") }
            DarkMenuItem { text: qsTr("Isohélie"); onTriggered: addAdjustmentLayer("posterize") }
            DarkMenuItem { text: qsTr("Seuil"); onTriggered: addAdjustmentLayer("threshold") }
            DarkMenuItem { text: qsTr("Transfert de dégradé"); onTriggered: addAdjustmentLayer("gradientMap") }
            DarkMenuItem { text: qsTr("Correction sélective"); onTriggered: addAdjustmentLayer("selectiveColor") }
        }
        DarkMenu {
            title: qsTr("Affichage")
            DarkMenu {
                title: qsTr("Panneaux")
                DarkMenuItem { text: qsTr("Histogramme"); checkable: true; checked: window.dockPanelVisible("histogram"); onTriggered: window.toggleDockPanel("histogram") }
                DarkMenuItem { text: qsTr("Navigateur"); checkable: true; checked: window.dockPanelVisible("navigator"); onTriggered: window.toggleDockPanel("navigator") }
                DarkMenuItem { text: qsTr("Couleur"); checkable: true; checked: window.dockPanelVisible("color"); onTriggered: window.toggleDockPanel("color") }
                DarkMenuItem { text: qsTr("Calques"); checkable: true; checked: window.dockPanelVisible("layers"); onTriggered: window.toggleDockPanel("layers") }
                DarkMenuItem { text: qsTr("Couches"); checkable: true; checked: window.dockPanelVisible("channels"); onTriggered: window.toggleDockPanel("channels") }
                DarkMenuItem { objectName: "informationPanelMenuItem"; text: qsTr("Informations"); checkable: true; checked: window.dockPanelVisible("information"); onTriggered: window.toggleDockPanel("information") }
                DarkMenuItem { text: qsTr("Historique"); checkable: true; checked: window.dockPanelVisible("history"); onTriggered: window.toggleDockPanel("history") }
            }
            MenuSeparator {}
            // Avoid Qt's macOS Preferences heuristic (Options/Settings/Config prefixes).
            DarkMenuItem { objectName: "brushOptionsMenuItem"; text: qsTr("Pinceau…"); onTriggered: brushSettingsDialog.open() }
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

    DarkMenu {
        id: toolGroupMenu
        objectName: "toolGroupMenu"
        Instantiator {
            model: (window.toolbarRevision, window.groupTools(window.groupById(window.toolMenuGroupId), false, true))
            delegate: DarkMenuItem {
                required property var modelData
                text: modelData.label + (modelData.implemented ? "" : qsTr(" (NYI)"))
                enabled: modelData.implemented
                checkable: true
                checked: window.groupById(window.toolMenuGroupId)
                         && window.groupById(window.toolMenuGroupId).active === modelData.name
                onTriggered: window.activateGroupTool(window.toolMenuGroupId, modelData.name)
            }
            onObjectAdded: function(index, object) { toolGroupMenu.insertItem(index, object) }
            onObjectRemoved: function(index, object) { toolGroupMenu.removeItem(object) }
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
                RowLayout {
                    visible: window.smartSelectionToolActive
                    spacing: 1
                    Repeater {
                        model: [
                            {glyph:"□", hint:qsTr("Nouvelle sélection")},
                            {glyph:"+", hint:qsTr("Ajouter à la sélection")},
                            {glyph:"−", hint:qsTr("Soustraire de la sélection")},
                            {glyph:"∩", hint:qsTr("Intersection avec la sélection")}
                        ]
                        delegate: IconButton {
                            required property var modelData
                            required property int index
                            implicitWidth: 30; implicitHeight: 30
                            glyph: modelData.glyph
                            hint: modelData.hint
                            selected: editor.selectionMode === index
                            onClicked: editor.selectionMode = index
                        }
                    }
                }
                Label { visible: window.paintToolActive || editor.tool === "quickSelection"; text: qsTr("Taille"); color: "#87929e" }
                DarkSpinBox { visible: window.paintToolActive || editor.tool === "quickSelection"; from: 1; to: 500; value: Math.round(editor.brushSize); suffix: " px"; implicitWidth: 110; onValueModified: editor.brushSize=value }
                Label { visible: editor.tool==="brush" || editor.tool==="eraser" || window.stampToolActive || editor.tool === "healing" || (editor.tool === "historyBrush" || editor.tool === "artHistoryBrush") || editor.tool === "spotHealing" || editor.tool === "smudge" || editor.tool === "colorReplacement" || editor.tool === "mixerBrush" || editor.tool === "quickSelection"; text: qsTr("Dureté"); color: "#87929e"; Layout.leftMargin: 4 }
                DarkSpinBox { objectName: "brushHardnessTop"; visible: editor.tool==="brush" || editor.tool==="eraser" || window.stampToolActive || editor.tool === "healing" || (editor.tool === "historyBrush" || editor.tool === "artHistoryBrush") || editor.tool === "spotHealing" || editor.tool === "smudge" || editor.tool === "colorReplacement" || editor.tool === "mixerBrush" || editor.tool === "quickSelection"; from: 0; to: 100; value: Math.round(editor.brushHardness*100); suffix: " %"; implicitWidth: 100; onValueModified: editor.brushHardness=value/100 }
                Label { visible: editor.tool === "magicWand"; text: qsTr("Tolérance"); color: "#87929e" }
                DarkSpinBox {
                    objectName: "selectionTolerance"
                    visible: editor.tool === "magicWand"
                    from: 0; to: 255; value: editor.selectionTolerance
                    implicitWidth: 82
                    onValueModified: editor.selectionTolerance = value
                }
                CheckBox {
                    objectName: "selectionContiguous"
                    visible: editor.tool === "magicWand"
                    text: qsTr("Pixels contigus")
                    checked: editor.selectionContiguous
                    onToggled: editor.selectionContiguous = checked
                }
                CheckBox {
                    objectName: "selectionSampleAllLayers"
                    visible: window.smartSelectionToolActive
                    text: qsTr("Tous les calques")
                    checked: editor.selectionSampleAllLayers
                    onToggled: editor.selectionSampleAllLayers = checked
                }
                CheckBox {
                    objectName: "selectionEnhanceEdge"
                    visible: editor.tool === "quickSelection"
                    text: qsTr("Améliorer le contour")
                    checked: editor.selectionEnhanceEdge
                    onToggled: editor.selectionEnhanceEdge = checked
                }
                Label { visible: window.opacityToolActive; text: qsTr("Opacité"); color: "#87929e"; Layout.leftMargin: 8 }
                DarkSpinBox { visible: window.opacityToolActive; from: 1; to: 100; value: Math.round(editor.brushOpacity*100); suffix: " %"; implicitWidth: 100; onValueModified: editor.brushOpacity=value/100 }
                Label { visible: editor.tool==="brush" || editor.tool==="pencil" || editor.tool==="eraser" || window.stampToolActive || editor.tool === "healing" || (editor.tool === "historyBrush" || editor.tool === "artHistoryBrush") || editor.tool === "spotHealing"; text: qsTr("Débit"); color: "#87929e"; Layout.leftMargin: 4 }
                DarkSpinBox { visible: editor.tool==="brush" || editor.tool==="pencil" || editor.tool==="eraser" || window.stampToolActive || editor.tool === "healing" || (editor.tool === "historyBrush" || editor.tool === "artHistoryBrush") || editor.tool === "spotHealing"; from: 1; to: 100; value: Math.round(editor.brushFlow*100); suffix: " %"; implicitWidth: 95; onValueModified: editor.brushFlow=value/100 }
                Button { objectName: "brushOptionsButton"; visible: editor.tool==="brush" || editor.tool==="pencil" || editor.tool==="eraser" || window.stampToolActive || editor.tool === "healing" || (editor.tool === "historyBrush" || editor.tool === "artHistoryBrush") || editor.tool === "spotHealing" || editor.tool === "smudge" || editor.tool === "mixerBrush"; text: qsTr("Pinceau…"); implicitHeight: 32; onClicked: brushSettingsDialog.open() }
                CheckBox {
                    objectName: "stampAligned"
                    visible: window.stampToolActive
                    text: qsTr("Aligné")
                    checked: editor.stampAligned
                    onToggled: editor.stampAligned = checked
                }
                ComboBox {
                    objectName: "cloneSampleMode"
                    visible: editor.tool === "cloneStamp"
                    model: [qsTr("Calque actif"), qsTr("Actif et inférieurs"), qsTr("Tous les calques")]
                    currentIndex: editor.cloneSampleMode
                    implicitWidth: 165; implicitHeight: 30
                    onActivated: editor.cloneSampleMode = currentIndex
                }
                ComboBox {
                    objectName: "stampPattern"
                    visible: editor.tool === "patternStamp"
                    model: editor.hasCustomStampPattern
                           ? [qsTr("Damier"), qsTr("Points"), qsTr("Rayures"), qsTr("Motif personnalisé")]
                           : [qsTr("Damier"), qsTr("Points"), qsTr("Rayures")]
                    currentIndex: editor.stampPattern
                    implicitWidth: 145; implicitHeight: 30
                    onActivated: editor.stampPattern = currentIndex
                }
                Button {
                    objectName: "defineStampPattern"
                    visible: editor.tool === "patternStamp"
                    enabled: editor.hasSelection
                    text: qsTr("Définir le motif…")
                    implicitHeight: 32
                    onClicked: editor.defineStampPattern()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Utiliser la zone sélectionnée comme motif répétitif")
                }
                CheckBox {
                    objectName: "pencilAutoErase"
                    visible: editor.tool === "pencil"
                    text: qsTr("Effacement auto")
                    checked: editor.pencilAutoErase
                    onToggled: editor.pencilAutoErase=checked
                }
                Label { visible: editor.tool === "blur" || editor.tool === "sharpen" || editor.tool === "smudge"; text: qsTr("Intensité"); color: "#87929e" }
                DarkSpinBox {
                    objectName: "retouchStrength"
                    visible: editor.tool === "blur" || editor.tool === "sharpen" || editor.tool === "smudge"
                    from: 1; to: 100; value: Math.round(editor.toolStrength*100); suffix: " %"
                    implicitWidth: 92
                    onValueModified: editor.toolStrength=value/100
                }
                ComboBox {
                    objectName: "smudgeBlendMode"
                    visible: editor.tool === "smudge"
                    model: window.blendModeNames
                    currentIndex: editor.brushBlend
                    implicitWidth: 135; implicitHeight: 30
                    onActivated: editor.brushBlend = currentIndex
                }
                CheckBox {
                    objectName: "smudgeSampleAllLayers"
                    visible: editor.tool === "smudge"
                    enabled: !editor.editingMask
                    text: qsTr("Tous les calques")
                    checked: editor.smudgeSampleAllLayers
                    onToggled: editor.smudgeSampleAllLayers = checked
                }
                CheckBox {
                    objectName: "smudgeFingerPainting"
                    visible: editor.tool === "smudge"
                    text: qsTr("Peinture au doigt")
                    checked: editor.smudgeFingerPainting
                    onToggled: editor.smudgeFingerPainting = checked
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Commencer avec la couleur de premier plan ; Option/Alt l’active temporairement")
                }
                ComboBox {
                    objectName: "replacementMode"
                    visible: editor.tool === "colorReplacement"
                    model: [qsTr("Couleur"), qsTr("Teinte"), qsTr("Saturation"), qsTr("Luminosité")]
                    currentIndex: editor.replacementMode
                    implicitWidth: 116; implicitHeight: 30
                    onActivated: editor.replacementMode = currentIndex
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Mode de remplacement")
                }
                ComboBox {
                    objectName: "replacementSampling"
                    visible: editor.tool === "colorReplacement"
                    model: [qsTr("En continu"), qsTr("Une fois"), qsTr("Arrière-plan")]
                    currentIndex: editor.replacementSampling
                    implicitWidth: 126; implicitHeight: 30
                    onActivated: editor.replacementSampling = currentIndex
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Couleur à remplacer : sous la pointe, au début du trait ou couleur d’arrière-plan")
                }
                ComboBox {
                    objectName: "replacementLimits"
                    visible: editor.tool === "colorReplacement"
                    model: [qsTr("Contiguës"), qsTr("Discontiguës"), qsTr("Contours")]
                    currentIndex: editor.replacementLimits
                    implicitWidth: 126; implicitHeight: 30
                    onActivated: editor.replacementLimits = currentIndex
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Limites du remplacement")
                }
                Label { visible: editor.tool === "colorReplacement"; text: qsTr("Tolérance"); color: "#87929e" }
                DarkSpinBox {
                    objectName: "replacementTolerance"
                    visible: editor.tool === "colorReplacement"
                    from: 0; to: 100; value: editor.replacementTolerance
                    suffix: " %"; implicitWidth: 90
                    onValueModified: editor.replacementTolerance = value
                }
                CheckBox {
                    objectName: "replacementAntialias"
                    visible: editor.tool === "colorReplacement"
                    text: qsTr("Lissage")
                    checked: editor.replacementAntialias
                    onToggled: editor.replacementAntialias = checked
                }
                Label { visible: editor.tool === "dodge" || editor.tool === "burn"; text: qsTr("Gamme"); color: "#87929e" }
                ComboBox {
                    objectName: "retouchRange"
                    visible: editor.tool === "dodge" || editor.tool === "burn"
                    model: [qsTr("Tons foncés"), qsTr("Tons moyens"), qsTr("Tons clairs")]
                    currentIndex: editor.retouchRange
                    implicitWidth: 118; implicitHeight: 30
                    onActivated: editor.retouchRange=currentIndex
                }
                Label { visible: editor.tool === "dodge" || editor.tool === "burn"; text: qsTr("Exposition"); color: "#87929e" }
                DarkSpinBox {
                    visible: editor.tool === "dodge" || editor.tool === "burn"
                    from: 1; to: 100; value: Math.round(editor.toolStrength*100); suffix: " %"
                    implicitWidth: 92
                    onValueModified: editor.toolStrength=value/100
                }
                CheckBox {
                    objectName: "protectTones"
                    visible: editor.tool === "dodge" || editor.tool === "burn"
                    text: qsTr("Protéger les tons")
                    checked: editor.protectTones
                    onToggled: editor.protectTones=checked
                }
                Label { visible: editor.tool === "sponge"; text: qsTr("Mode"); color: "#87929e" }
                ComboBox {
                    objectName: "spongeMode"
                    visible: editor.tool === "sponge"
                    model: [qsTr("Désaturer"), qsTr("Saturer")]
                    currentIndex: editor.spongeSaturate ? 1 : 0
                    implicitWidth: 108; implicitHeight: 30
                    onActivated: editor.spongeSaturate=currentIndex === 1
                }
                Label { visible: editor.tool === "sponge"; text: qsTr("Débit"); color: "#87929e" }
                DarkSpinBox {
                    visible: editor.tool === "sponge"
                    from: 1; to: 100; value: Math.round(editor.toolStrength*100); suffix: " %"
                    implicitWidth: 92
                    onValueModified: editor.toolStrength=value/100
                }
                Label { visible: window.shapeToolActive && window.width >= 1250; text: qsTr("Pixels"); color: "#aab3bd" }
                CheckBox {
                    objectName: "shapeFillEnabled"
                    visible: window.closedShapeToolActive
                    text: qsTr("Fond")
                    checked: editor.filledShapes
                    onToggled: editor.filledShapes=checked
                }
                Button {
                    objectName: "shapeFillColor"
                    visible: window.closedShapeToolActive
                    implicitWidth: 38; implicitHeight: 30
                    enabled: editor.filledShapes
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Couleur de remplissage")
                    background: Rectangle {
                        radius: 4; color: "#161b21"; border.color: "#606a75"
                        Rectangle { anchors.fill: parent; anchors.margins: 5; color: editor.shapeFillColor; border.color: "#d5d9dd" }
                    }
                    onClicked: {
                        changingShapeFill=true
                        changingShapeStroke=false
                        colorDialog.selectedColor=editor.shapeFillColor
                        colorDialog.open()
                    }
                }
                CheckBox {
                    objectName: "shapeStrokeEnabled"
                    visible: window.shapeToolActive
                    text: qsTr("Contour")
                    checked: editor.shapeStrokeEnabled
                    onToggled: editor.shapeStrokeEnabled=checked
                }
                Button {
                    objectName: "shapeStrokeColor"
                    visible: window.shapeToolActive
                    implicitWidth: 38; implicitHeight: 30
                    enabled: editor.shapeStrokeEnabled
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Couleur du contour")
                    background: Rectangle {
                        radius: 4; color: "#161b21"; border.color: "#606a75"
                        Rectangle { anchors.fill: parent; anchors.margins: 5; color: editor.shapeStrokeColor; border.color: "#d5d9dd" }
                    }
                    onClicked: {
                        changingShapeStroke=true
                        changingShapeFill=false
                        colorDialog.selectedColor=editor.shapeStrokeColor
                        colorDialog.open()
                    }
                }
                DarkSpinBox {
                    objectName: "shapeStrokeWidth"
                    visible: window.shapeToolActive
                    enabled: editor.shapeStrokeEnabled
                    from: 1; to: 500; value: Math.round(editor.shapeStrokeWidth); suffix: " px"
                    implicitWidth: 88
                    onValueModified: editor.shapeStrokeWidth=value
                }
                ComboBox {
                    objectName: "shapeStrokeStyle"
                    visible: window.shapeToolActive
                    enabled: editor.shapeStrokeEnabled
                    model: [qsTr("Trait plein"), qsTr("Tirets"), qsTr("Pointillés")]
                    currentIndex: editor.shapeStrokeStyle
                    implicitWidth: 104; implicitHeight: 30
                    onActivated: editor.shapeStrokeStyle=currentIndex
                }
                Label { visible: editor.tool === "rectangle"; text: qsTr("Rayon"); color: "#87929e" }
                DarkSpinBox {
                    objectName: "shapeCornerRadius"
                    visible: editor.tool === "rectangle"
                    from: 0; to: 500; value: Math.round(editor.shapeCornerRadius); suffix: " px"
                    implicitWidth: 88
                    onValueModified: editor.shapeCornerRadius=value
                }
                Label { visible: editor.tool === "polygon" || editor.tool === "star"; text: qsTr("Côtés"); color: "#87929e" }
                DarkSpinBox {
                    objectName: "shapeSides"
                    visible: editor.tool === "polygon" || editor.tool === "star"
                    from: 3; to: 100; value: editor.shapeSides
                    implicitWidth: 70
                    onValueModified: editor.shapeSides=value
                }
                Label { visible: editor.tool === "star"; text: qsTr("Creux"); color: "#87929e" }
                DarkSpinBox {
                    objectName: "shapeStarRatio"
                    visible: editor.tool === "star"
                    from: 5; to: 95; value: Math.round(editor.shapeStarRatio*100); suffix: " %"
                    implicitWidth: 86
                    onValueModified: editor.shapeStarRatio=value/100
                }
                CheckBox {
                    objectName: "shapeAlignPixels"
                    visible: window.shapeToolActive && window.width >= 1420
                    text: qsTr("Aligner les pixels")
                    checked: editor.shapeAlignPixels
                    onToggled: editor.shapeAlignPixels=checked
                }
                Label { visible: editor.tool === "rotateView"; text: qsTr("Angle"); color: "#87929e" }
                DarkSpinBox {
                    objectName: "viewRotationAngle"
                    visible: editor.tool === "rotateView"
                    from: -180; to: 180; value: Math.round(window.canvasRotation); suffix: " °"
                    implicitWidth: 92
                    onValueModified: window.canvasRotation=value
                }
                Button {
                    objectName: "resetViewRotation"
                    visible: editor.tool === "rotateView"
                    text: qsTr("Réinitialiser la vue")
                    implicitHeight: 32
                    onClicked: window.canvasRotation=0
                }
                Button {
                    visible: editor.tool === "zoomTool"
                    text: qsTr("−")
                    implicitHeight: 32
                    onClicked: setZoom(zoom/1.25)
                }
                Button {
                    visible: editor.tool === "zoomTool"
                    text: qsTr("+")
                    implicitHeight: 32
                    onClicked: setZoom(zoom*1.25)
                }
                Button {
                    visible: editor.tool === "zoomTool"
                    text: qsTr("Adapter")
                    implicitHeight: 32
                    onClicked: fitCanvas()
                }
                Button { text: qsTr("Recadrer"); visible: editor.tool==="crop"; enabled: editor.hasSelection; implicitHeight: 32; onClicked: editor.cropToSelection() }
                Item { Layout.fillWidth: true }
                IconButton { glyph: "↶"; hint: qsTr("Annuler"); enabled: editor.canUndo; onClicked: editor.undo() }
                IconButton { glyph: "↷"; hint: qsTr("Rétablir"); enabled: editor.canRedo; onClicked: editor.redo() }
                Button {
                    text: qsTr("Exporter ↗"); implicitWidth: 116; implicitHeight: 32
                    background: Rectangle { color: parent.down ? "#94caa4" : "#beeccb"; radius: 5 }
                    contentItem: Text { text: parent.text; color: "#203b2c"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: exportDialog.open()
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#10141a" }
        RulerOptions {
            objectName:"rulerOptions"
            Layout.fillWidth:true
            visible:editor.tool==="ruler"
            start:window.rulerStart;end:window.rulerEnd;hasMeasure:window.rulerVisible
            onClearMeasure:window.rulerVisible=false
        }
        EyedropperOptions {
            objectName: "eyedropperOptions"
            Layout.fillWidth: true
            visible: window.eyedropperToolActive
            document: editor
            samples: window.colorSamples
            onClearSamples: window.colorSamples=[]
        }
        HistoryBrushOptions {
            objectName: "historyBrushOptions"
            Layout.fillWidth: true
            visible: editor.tool === "historyBrush"
            document: editor
            blendModes: window.blendModeNames
        }
        ArtHistoryBrushOptions {
            objectName: "artHistoryBrushOptions"
            Layout.fillWidth: true
            visible: editor.tool === "artHistoryBrush"
            document: editor
            blendModes: window.blendModeNames
        }
        RemoveOptions {
            objectName: "removeOptions"
            Layout.fillWidth: true
            visible: editor.tool === "remove"
            document: editor
        }
        ContentAwareMoveOptions {
            objectName: "contentAwareMoveOptions"
            Layout.fillWidth: true
            visible: editor.tool === "contentAwareMove"
            document: editor
        }
        PatchOptions {
            objectName: "patchOptions"
            Layout.fillWidth: true
            visible: editor.tool === "patch"
            document: editor
        }
        SpotHealingOptions {
            objectName: "spotHealingOptions"
            Layout.fillWidth: true
            visible: editor.tool === "spotHealing"
            document: editor
        }
        HealingBrushOptions {
            objectName: "healingBrushOptions"
            Layout.fillWidth: true
            visible: editor.tool === "healing"
            document: editor
        }
        RedEyeOptions {
            objectName: "redEyeOptions"
            Layout.fillWidth: true
            visible: editor.tool === "redEye"
            document: editor
        }
        MixerBrushOptions {
            objectName: "mixerBrushOptions"
            Layout.fillWidth: true
            visible: editor.tool === "mixerBrush"
            document: editor
            onColorRequested: {
                changingSecondary = false
                colorDialog.selectedColor = editor.mixerLoadColor
                colorDialog.open()
            }
        }
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
                        model: (window.toolbarRevision, window.visibleToolGroups())
                        delegate: IconButton {
                            id: toolButton
                            required property var modelData
                            objectName: "toolGroup_" + modelData.id
                            groupId: modelData.id
                            readonly property var displayedTool: window.activeToolForGroup(modelData)
                            readonly property var displayedTools: window.groupTools(modelData, false, false)
                            glyph: displayedTool ? displayedTool.icon : "?"
                            toolName: displayedTool ? displayedTool.name : ""
                            hint: displayedTool ? displayedTool.label + " (" + window.toolGroupShortcut(modelData) + ")" : ""
                            selected: window.toolGroupContains(modelData, editor.tool)
                            hasMenu: window.groupTools(modelData, false, true).length > 1
                            onClicked: window.selectActiveGroupTool(modelData.id)
                            onContextRequested: window.openToolGroupMenu(modelData.id, toolButton)
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
                        if (event.key === Qt.Key_Alt && !event.isAutoRepeat) {
                            window.stampSourcePicking = true
                        }
                        if (editor.polygonLassoActive && !event.isAutoRepeat
                                && (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete)) {
                            editor.removeLastPolygonLassoPoint()
                            event.accepted = true
                            return
                        }
                        if (editor.tool === "remove" && !window.editingText && !window.modalOpen
                                && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                            editor.applyRemoval()
                            event.accepted = true
                            return
                        }
                        if (editor.polygonLassoActive && !event.isAutoRepeat
                                && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                            editor.finishPolygonLasso()
                            event.accepted = true
                            return
                        }
                        if(event.key===Qt.Key_Space && !event.isAutoRepeat && !window.editingText && !window.modalOpen) {
                            window.spacePressed=true;event.accepted=true
                        }
                    }
                    Keys.onReleased: function(event) {
                        if (event.key === Qt.Key_Alt && !event.isAutoRepeat)
                            window.stampSourcePicking = false
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
                            property real selectionAntPhase: 0
                            onSelectionAntPhaseChanged: {
                                rectangularSelectionOverlay.requestPaint()
                                ellipseSelectionOverlay.requestPaint()
                                lassoSelectionOverlay.requestPaint()
                            }
                            function strokeSelectionPath(context) {
                                context.lineWidth = 1.5
                                context.setLineDash([5, 5])
                                context.lineDashOffset = selectionAntPhase
                                context.strokeStyle = "#111111"
                                context.stroke()
                                context.lineDashOffset = selectionAntPhase + 5
                                context.strokeStyle = "#ffffff"
                                context.stroke()
                                context.setLineDash([])
                            }
                            x: (viewport.contentWidth-width)/2; y: (viewport.contentHeight-height)/2
                            width: editor.width*zoom; height: editor.height*zoom
                            rotation: window.canvasRotation
                            transformOrigin: Item.Center
                            onXChanged: horizontalRuler.requestPaint()
                            onYChanged: verticalRuler.requestPaint()
                            Rectangle { x: 5; y: 9; width: parent.width; height: parent.height; color: "#0c1015"; opacity: 0.6 }
                            ImageCanvas {
                                id: imageCanvas
                                objectName: "imageCanvas"
                                anchors.fill: parent
                                document: editor
                                selectionPhase: artboard.selectionAntPhase
                                Component.onCompleted: editor.setPreviewSize(Math.round(width), Math.round(height))
                                onWidthChanged: editor.setPreviewSize(Math.round(width), Math.round(height))
                                onHeightChanged: editor.setPreviewSize(Math.round(width), Math.round(height))
                            }
                            NumberAnimation on selectionAntPhase {
                                from: 0
                                to: 10
                                duration: 700
                                loops: Animation.Infinite
                                running: editor.hasSelection || editor.polygonLassoActive
                            }
                            Canvas {
                                id: rectangularSelectionOverlay
                                objectName: "rectangularSelectionOverlay"
                                visible: editor.hasSelection && editor.selectionShape < 4
                                         && editor.selectionShape !== 1
                                x: editor.selection.x * zoom - 2
                                y: editor.selection.y * zoom - 2
                                width: editor.selection.width * zoom + 4
                                height: editor.selection.height * zoom + 4
                                z: 2
                                enabled: false
                                onPaint: {
                                    const c = getContext("2d")
                                    c.reset()
                                    c.clearRect(0, 0, width, height)
                                    if (width <= 4 || height <= 4) return
                                    c.beginPath()
                                    c.rect(2, 2, width - 4, height - 4)
                                    artboard.strokeSelectionPath(c)
                                }
                                onWidthChanged: requestPaint()
                                onHeightChanged: requestPaint()
                                onVisibleChanged: if (visible) requestPaint()
                                Connections {
                                    target: editor
                                    function onSelectionChanged() {
                                        rectangularSelectionOverlay.requestPaint()
                                    }
                                }
                            }
                            Rectangle {
                                objectName: "redEyeRegionOverlay"
                                x: editor.redEyeRegion.x * zoom
                                y: editor.redEyeRegion.y * zoom
                                width: editor.redEyeRegion.width * zoom
                                height: editor.redEyeRegion.height * zoom
                                z: 2
                                enabled: false
                                visible: editor.tool === "redEye" && width > 1 && height > 1
                                color: "transparent"
                                border.color: "#f5f7f9"
                                border.width: 1
                                Rectangle {
                                    anchors.fill: parent; anchors.margins: 1
                                    color: "transparent"; border.color: "#172028"; border.width: 1
                                }
                            }
                            Canvas {
                                id: ellipseSelectionOverlay
                                objectName: "ellipseSelectionOverlay"
                                x: editor.selection.x * zoom - 2
                                y: editor.selection.y * zoom - 2
                                width: editor.selection.width * zoom + 4
                                height: editor.selection.height * zoom + 4
                                z: 2
                                enabled: false
                                visible: editor.hasSelection && editor.selectionShape === 1
                                onPaint: {
                                    const c = getContext("2d")
                                    c.reset()
                                    c.clearRect(0, 0, width, height)
                                    if (width <= 4 || height <= 4) return
                                    c.save()
                                    c.translate(width / 2, height / 2)
                                    c.scale((width - 4) / 2, (height - 4) / 2)
                                    c.beginPath()
                                    c.arc(0, 0, 1, 0, Math.PI * 2)
                                    c.restore()
                                    artboard.strokeSelectionPath(c)
                                }
                                onWidthChanged: requestPaint()
                                onHeightChanged: requestPaint()
                                onVisibleChanged: if (visible) requestPaint()
                                Connections {
                                    target: editor
                                    function onSelectionChanged() {
                                        ellipseSelectionOverlay.requestPaint()
                                    }
                                }
                            }
                            Canvas {
                                id: lassoSelectionOverlay
                                anchors.fill: parent
                                z: 2
                                enabled: false
                                visible: editor.selectionShape >= 4 && editor.selectionPath.length > 0
                                onPaint: {
                                    const points = editor.selectionPath
                                    const c = getContext("2d")
                                    c.reset()
                                    c.clearRect(0, 0, width, height)
                                    if (!points || points.length < 1) return
                                    c.beginPath()
                                    c.moveTo(points[0].x * zoom, points[0].y * zoom)
                                    for (let i = 1; i < points.length; ++i)
                                        c.lineTo(points[i].x * zoom, points[i].y * zoom)
                                    if (editor.polygonLassoActive && drawingArea.containsMouse)
                                        c.lineTo(drawingArea.pointerX, drawingArea.pointerY)
                                    if (editor.hasSelection) {
                                        c.closePath()
                                        artboard.strokeSelectionPath(c)
                                    } else {
                                        c.strokeStyle = "#f5f7f9"
                                        c.lineWidth = 1
                                        c.stroke()
                                    }
                                    if (editor.polygonLassoActive) {
                                        for (let i = 0; i < points.length; ++i) {
                                            c.beginPath()
                                            c.arc(points[i].x * zoom, points[i].y * zoom, 3, 0,
                                                  Math.PI * 2)
                                            c.fillStyle = "#f5f7f9"
                                            c.fill()
                                            c.strokeStyle = "#172028"
                                            c.lineWidth = 1
                                            c.stroke()
                                        }
                                    }
                                }
                                Connections {
                                    target: editor
                                    function onSelectionChanged() { lassoSelectionOverlay.requestPaint() }
                                }
                                Connections {
                                    target: window
                                    function onZoomChanged() { lassoSelectionOverlay.requestPaint() }
                                }
                            }
                            MouseArea {
                                id: drawingArea
                                objectName: "drawingArea"
                                property real pointerX: 0
                                property real pointerY: 0
                                property int selectionModeBeforeGesture: -1
                                property bool patchInsideSelection: false
                                function updatePatchCursor() {
                                    patchInsideSelection = (editor.tool === "patch" || editor.tool === "contentAwareMove")
                                            && editor.selectionContains(pointerX / zoom, pointerY / zoom)
                                }
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: editor.tool!=="hand" && editor.tool!=="rotateView"
                                         && editor.tool!=="zoomTool"
                                preventStealing: true
                                cursorShape: window.canvasCursorShape
                                function syncSystemCursor() {
                                    const calibration=curvePickerArea.enabled && curvePickerArea.containsMouse
                                    const measuring=measureToolArea.enabled && measureToolArea.containsMouse
                                    const tool=calibration ? "curvePicker_" + (window.colorPickerMode || window.curvePickerMode)
                                              : measuring && editor.tool==="colorSampler" && measureToolArea.deleteHovered ? "colorSamplerDelete"
                                              : editor.tool
                                    imageCanvas.setSystemCursor(calibration || measuring ? Qt.CrossCursor : window.canvasCursorShape,
                                                                (containsMouse || calibration || measuring) && window.active && !window.modalOpen && !window.spacePressed,
                                                                tool, editor.selectionMode, window.stampSourcePicking)
                                }
                                function refreshSystemCursor() {
                                    syncSystemCursor()
                                    // Native window creation and tool bindings can finish after
                                    // the initial focus/hover notification. Reapply the settled state.
                                    Qt.callLater(syncSystemCursor)
                                }
                                onEnabledChanged: refreshSystemCursor()
                                onContainsMouseChanged: {
                                    refreshSystemCursor()
                                    if (!containsMouse) window.informationPointerValid = false
                                }
                                onPressed: function(mouse) {
                                    workspace.forceActiveFocus()
                                    pointerX = mouse.x
                                    pointerY = mouse.y
                                    window.updateInformationPointer(mouse.x / zoom, mouse.y / zoom)
                                    if (editor.tool === "picker") {
                                        window.showInformationPanel()
                                        if (mouse.modifiers & Qt.ShiftModifier) {
                                            window.addColorSample(mouse.x/zoom,mouse.y/zoom)
                                            return
                                        }
                                        window.pickerPreviousColor=(mouse.modifiers & Qt.AltModifier) ? editor.secondaryColor : editor.color
                                        window.pickerDragging=true
                                        window.pickerPosition=Qt.point(mouse.x,mouse.y)
                                        editor.pickToolColor(Math.floor(mouse.x/zoom),Math.floor(mouse.y/zoom),Boolean(mouse.modifiers & Qt.AltModifier))
                                        window.pickerSampleColor=(mouse.modifiers & Qt.AltModifier) ? editor.secondaryColor : editor.color
                                        return
                                    }
                                    if (editor.tool === "cloneStamp" && (mouse.modifiers & Qt.AltModifier)) {
                                        editor.setCloneSource(mouse.x / zoom, mouse.y / zoom)
                                        return
                                    }
                                    if (window.smartSelectionToolActive) {
                                        selectionModeBeforeGesture = editor.selectionMode
                                        if (mouse.modifiers & Qt.ShiftModifier)
                                            editor.selectionMode = 1
                                        else if (mouse.modifiers & Qt.AltModifier)
                                            editor.selectionMode = 2
                                    }
                                    if (editor.tool === "polygonalLasso")
                                        editor.addPolygonLassoPoint(mouse.x / zoom, mouse.y / zoom)
                                    else
                                        editor.beginStroke(mouse.x / zoom, mouse.y / zoom, Boolean(mouse.modifiers & Qt.AltModifier), Boolean(mouse.modifiers & Qt.ShiftModifier))
                                }
                                onPositionChanged: function(mouse) {
                                    pointerX = mouse.x
                                    pointerY = mouse.y
                                    window.updateInformationPointer(mouse.x / zoom, mouse.y / zoom)
                                    updatePatchCursor()
                                    window.stampSourcePicking = Boolean(mouse.modifiers & Qt.AltModifier)
                                    window.status = "X " + Math.round(mouse.x/zoom) + "   Y " + Math.round(mouse.y/zoom)
                                    if (brushCursor.visible) {
                                        brushCursor.pointerX=mouse.x;brushCursor.pointerY=mouse.y;brushCursor.requestPaint()
                                    }
                                    if (editor.tool === "picker" && window.pickerDragging) {
                                        window.pickerPosition=Qt.point(mouse.x,mouse.y)
                                        editor.pickToolColor(Math.floor(mouse.x/zoom),Math.floor(mouse.y/zoom),Boolean(mouse.modifiers & Qt.AltModifier))
                                        window.pickerSampleColor=(mouse.modifiers & Qt.AltModifier) ? editor.secondaryColor : editor.color
                                    } else if (editor.polygonLassoActive)
                                        lassoSelectionOverlay.requestPaint()
                                    else if(pressed) editor.continueStroke(mouse.x/zoom,mouse.y/zoom)
                                }
                                onReleased: {
                                    window.pickerDragging=false
                                    if (editor.tool !== "polygonalLasso") editor.endStroke()
                                    if (selectionModeBeforeGesture >= 0) {
                                        editor.selectionMode = selectionModeBeforeGesture
                                        selectionModeBeforeGesture = -1
                                    }
                                }
                                onDoubleClicked: {
                                    if (editor.tool === "polygonalLasso") editor.finishPolygonLasso()
                                }
                                onCanceled: {
                                    window.pickerDragging=false
                                    if (editor.tool !== "polygonalLasso") editor.cancelStroke()
                                    if (selectionModeBeforeGesture >= 0) {
                                        editor.selectionMode = selectionModeBeforeGesture
                                        selectionModeBeforeGesture = -1
                                    }
                                }
                                WheelHandler {
                                    id: wheelHandler
                                    objectName: "wheelHandler"
                                    target: null
                                    onWheel: function(wheel) {
                                        const viewportPoint = drawingArea.mapToItem(
                                                    viewport, wheel.x, wheel.y)
                                        const pixelY=wheel.pixelDelta.y
                                        const pixelX=wheel.pixelDelta.x
                                        const vertical=pixelY!==0 ? pixelY : wheel.angleDelta.y/2
                                        const horizontal=pixelX!==0 ? pixelX : wheel.angleDelta.x/2
                                        const primaryModifier=wheel.modifiers&(Qt.ControlModifier|Qt.MetaModifier)
                                        if(primaryModifier) {
                                            const steps=wheel.angleDelta.y!==0 ? wheel.angleDelta.y/120 : vertical/40
                                            zoomAt(zoom*Math.pow(1.15,steps),viewportPoint.x,viewportPoint.y)
                                        } else if(wheel.modifiers&Qt.ShiftModifier) {
                                            panCanvas(-(vertical!==0?vertical:horizontal),0)
                                        } else {
                                            panCanvas(-horizontal,-vertical)
                                        }
                                        wheel.accepted=true
                                    }
                                }
                            }
                            Connections {
                                target: window
                                function onCanvasCursorShapeChanged() { drawingArea.refreshSystemCursor() }
                                function onZoomChanged() { drawingArea.syncSystemCursor() }
                                function onModalOpenChanged() { drawingArea.syncSystemCursor() }
                                function onActiveChanged() { drawingArea.refreshSystemCursor() }
                                function onCurvePickerModeChanged() { drawingArea.refreshSystemCursor() }
                                function onColorPickerModeChanged() { drawingArea.refreshSystemCursor() }
                                function onStampSourcePickingChanged() { drawingArea.syncSystemCursor() }
                            }
                            Connections {
                                target: editor
                                function onImageChanged() { if (!sampleRefreshTimer.running) sampleRefreshTimer.start() }
                                function onDocumentChanged() {
                                    if(window.sampleDocumentSession!==editor.documentSession) {
                                        window.sampleDocumentSession=editor.documentSession
                                        window.colorSamples=[];window.rulerVisible=false;window.informationPointerValid=false
                                    } else if(window.colorSamples.length) {
                                        window.colorSamples=window.colorSamples.filter(function(s){return s.x<editor.width && s.y<editor.height})
                                        sampleRefreshTimer.restart()
                                    }
                                }
                                function onSettingsChanged() { drawingArea.updatePatchCursor(); drawingArea.refreshSystemCursor(); if (!sampleRefreshTimer.running) sampleRefreshTimer.start() }
                                function onSelectionChanged() { drawingArea.updatePatchCursor() }
                                function onPatchPreviewChanged() { drawingArea.updatePatchCursor() }
                            }
                            Timer {
                                id: sampleRefreshTimer; interval:40
                                onTriggered: { window.refreshColorSamples(); window.refreshInformationColor() }
                            }
                            Canvas {
                                id: rulerOverlay
                                anchors.fill: parent
                                z: 4
                                enabled: false
                                visible: window.rulerVisible
                                onPaint: {
                                    const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
                                    const x1=window.rulerStart.x*zoom,y1=window.rulerStart.y*zoom
                                    const x2=window.rulerEnd.x*zoom,y2=window.rulerEnd.y*zoom
                                    c.strokeStyle="rgba(0,0,0,.85)";c.lineWidth=4
                                    c.beginPath();c.moveTo(x1,y1);c.lineTo(x2,y2);c.stroke()
                                    c.strokeStyle="#f5f7f9";c.lineWidth=2
                                    c.beginPath();c.moveTo(x1,y1);c.lineTo(x2,y2);c.stroke()
                                    for(const p of [{x:x1,y:y1},{x:x2,y:y2}]) {
                                        c.beginPath();c.arc(p.x,p.y,4,0,Math.PI*2)
                                        c.fillStyle="#f5f7f9";c.fill();c.strokeStyle="#101419";c.lineWidth=1;c.stroke()
                                    }
                                }
                                Connections {
                                    target: window
                                    function onRulerStartChanged(){rulerOverlay.requestPaint()}
                                    function onRulerEndChanged(){rulerOverlay.requestPaint()}
                                    function onZoomChanged(){rulerOverlay.requestPaint()}
                                }
                            }
                            Canvas {
                                id: samplingRing
                                objectName: "samplingRing"
                                anchors.fill: parent; z: 7; enabled: false
                                visible: window.pickerDragging && editor.eyedropperRing && editor.tool==="picker"
                                onPaint: {
                                    const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
                                    const x=window.pickerPosition.x,y=window.pickerPosition.y
                                    c.lineWidth=10
                                    c.beginPath();c.arc(x,y,35,Math.PI,Math.PI*2);c.strokeStyle=window.pickerSampleColor;c.stroke()
                                    c.beginPath();c.arc(x,y,35,0,Math.PI);c.strokeStyle=window.pickerPreviousColor;c.stroke()
                                    c.lineWidth=1;c.strokeStyle="#111111"
                                    c.beginPath();c.arc(x,y,41,0,Math.PI*2);c.stroke()
                                    c.strokeStyle="#ffffff";c.beginPath();c.arc(x,y,29,0,Math.PI*2);c.stroke()
                                }
                                Connections {
                                    target: window
                                    function onPickerPositionChanged(){samplingRing.requestPaint()}
                                    function onPickerSampleColorChanged(){samplingRing.requestPaint()}
                                    function onPickerPreviousColorChanged(){samplingRing.requestPaint()}
                                }
                            }
                            Repeater {
                                model: window.colorSamples
                                delegate: Canvas {
                                    required property var modelData
                                    required property int index
                                    x:modelData.x*zoom-10;y:modelData.y*zoom-10
                                    z:5;width:28;height:28;enabled:false
                                    onPaint: {
                                        const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
                                        for(const style of [{color:"#111111",width:3},{color:"#ffffff",width:1}]) {
                                            c.strokeStyle=style.color;c.lineWidth=style.width
                                            c.beginPath();c.arc(10,10,5,0,Math.PI*2);c.stroke()
                                            c.beginPath();c.moveTo(0,10);c.lineTo(20,10);c.moveTo(10,0);c.lineTo(10,20);c.stroke()
                                        }
                                        c.font="bold 10px sans-serif";c.lineWidth=3;c.strokeStyle="#111111";c.strokeText(index+1,19,26)
                                        c.fillStyle="#ffffff";c.fillText(index+1,19,26)
                                    }
                                }
                            }
                            MouseArea {
                                id: measureToolArea
                                objectName: "measureToolArea"
                                property int draggedSample: -1
                                property point pointer:Qt.point(-100,-100)
                                readonly property bool deleteHovered: window.stampSourcePicking && window.nearestColorSample(pointer.x/zoom,pointer.y/zoom)>=0
                                anchors.fill: parent; z:5
                                enabled: editor.tool === "ruler" || editor.tool === "colorSampler"
                                hoverEnabled: enabled; preventStealing:true
                                cursorShape: Qt.CrossCursor
                                onEnabledChanged: drawingArea.refreshSystemCursor()
                                onContainsMouseChanged: {
                                    drawingArea.refreshSystemCursor()
                                    if (!containsMouse) window.informationPointerValid = false
                                }
                                onDeleteHoveredChanged: drawingArea.syncSystemCursor()
                                onPressed: function(mouse) {
                                    workspace.forceActiveFocus()
                                    const x=mouse.x/zoom,y=mouse.y/zoom
                                    window.showInformationPanel()
                                    window.updateInformationPointer(x, y)
                                    if(editor.tool==="colorSampler") {
                                        const existing=window.nearestColorSample(x,y)
                                        if(mouse.modifiers&Qt.AltModifier) window.removeColorSample(existing)
                                        else draggedSample=existing>=0 ? existing : window.addColorSample(x,y)
                                    } else {
                                        window.rulerStart=Qt.point(x,y);window.rulerEnd=Qt.point(x,y);window.rulerVisible=true
                                    }
                                }
                                onPositionChanged: function(mouse) {
                                    const x=mouse.x/zoom,y=mouse.y/zoom
                                    pointer=Qt.point(mouse.x,mouse.y)
                                    window.updateInformationPointer(x, y)
                                    window.stampSourcePicking=Boolean(mouse.modifiers&Qt.AltModifier)
                                    if(pressed && editor.tool==="ruler") {
                                        const dx=x-window.rulerStart.x,dy=y-window.rulerStart.y
                                        const angle=Math.round(Math.atan2(dy,dx)/(Math.PI/4))*Math.PI/4
                                        const length=Math.sqrt(dx*dx+dy*dy)
                                        window.rulerEnd=(mouse.modifiers&Qt.ShiftModifier)
                                                ? Qt.point(window.rulerStart.x+Math.cos(angle)*length,window.rulerStart.y+Math.sin(angle)*length)
                                                : Qt.point(x,y)
                                    }
                                    else if(pressed && draggedSample>=0) window.updateColorSample(draggedSample,x,y)
                                }
                                onReleased: function(mouse) {
                                    if(draggedSample>=0 && (mouse.x<0 || mouse.y<0 || mouse.x>=width || mouse.y>=height)) window.removeColorSample(draggedSample)
                                    draggedSample=-1
                                }
                                onCanceled: draggedSample=-1
                            }
                            MouseArea {
                                id: curvePickerArea
                                objectName: "curvePickerArea"
                                property real pointerX: 0
                                property real pointerY: 0
                                anchors.fill: parent
                                z: 6
                                enabled: (window.curvePickerMode.length > 0 || window.colorPickerMode.length > 0) && layerPropertiesDialog.opened
                                hoverEnabled: enabled
                                cursorShape: Qt.BlankCursor
                                onContainsMouseChanged: drawingArea.refreshSystemCursor()
                                onEnabledChanged: drawingArea.refreshSystemCursor()
                                onPositionChanged: function(mouse) {
                                    pointerX = mouse.x
                                    pointerY = mouse.y
                                    window.updateInformationPointer(mouse.x / zoom, mouse.y / zoom)
                                }
                                onClicked: function(mouse) {
                                    const documentX = Math.floor(mouse.x/zoom)
                                    const documentY = Math.floor(mouse.y/zoom)
                                    if (window.colorPickerMode === "whiteBalance")
                                        colorEditorControl.applyWhiteBalance(editor.sampleCompositeColor(documentX,documentY))
                                    else if (window.curvePickerMode === "color")
                                        curvesEditorControl.addColorSample(
                                            editor.sampleCompositeColor(documentX,documentY))
                                    else
                                        editor.sampleCurvePoint(window.curvePickerMode,
                                                                documentX,documentY)
                                }
                            }
                            Canvas {
                                id: cloneSourceMarker
                                objectName: "cloneSourceMarker"
                                width: 21; height: 21; z: 5; enabled: false
                                visible: drawingArea.containsMouse && drawingArea.pressed
                                         && ((editor.tool === "cloneStamp" && editor.cloneSourceValid)
                                             || (editor.tool === "healing" && editor.healingSourceValid))
                                         && !window.stampSourcePicking
                                x: drawingArea.pointerX + (editor.tool === "healing" ? editor.healingOffset.x : editor.cloneOffset.x) * zoom - width / 2
                                y: drawingArea.pointerY + (editor.tool === "healing" ? editor.healingOffset.y : editor.cloneOffset.y) * zoom - height / 2
                                onPaint: {
                                    const c = getContext("2d")
                                    c.reset(); c.clearRect(0, 0, width, height)
                                    c.beginPath(); c.moveTo(10, 2); c.lineTo(10, 18)
                                    c.moveTo(2, 10); c.lineTo(18, 10)
                                    c.lineWidth = 3; c.strokeStyle = "#111111"; c.stroke()
                                    c.lineWidth = 1; c.strokeStyle = "#ffffff"; c.stroke()
                                }
                            }
                            Canvas {
                                id: brushCursor
                                property real pointerX: 0
                                property real pointerY: 0
                                visible: drawingArea.containsMouse && !window.stampToolActive && editor.tool !== "remove" && editor.tool !== "historyBrush" && editor.tool !== "artHistoryBrush" && editor.tool !== "smudge" && editor.tool !== "healing" && editor.tool !== "spotHealing" && editor.tool !== "colorReplacement" && editor.tool !== "mixerBrush" && (window.paintToolActive || editor.tool === "quickSelection")
                                enabled: false; z: 5
                                width: Math.max(8,editor.brushSize*zoom+8); height: width
                                x: pointerX-width/2; y: pointerY-height/2
                                onPaint: {
                                    const c=getContext("2d");c.reset();c.clearRect(0,0,width,height);c.translate(width/2,height/2)
                                    c.rotate(editor.brushAngle*Math.PI/180);c.scale(1,editor.brushRoundness)
                                    const radius=editor.brushSize*zoom/2;c.beginPath();c.arc(0,0,radius,0,Math.PI*2)
                                    c.lineWidth=2/editor.brushRoundness;c.strokeStyle="rgba(0,0,0,.75)";c.stroke()
                                    c.lineWidth=1/editor.brushRoundness;c.strokeStyle="rgba(255,255,255,.95)";c.stroke()
                                    if (editor.tool === "quickSelection") {
                                        const sign = editor.selectionMode === 2 ? -1 : 1
                                        const px = Math.min(width - 5, width / 2 + radius * .7)
                                        const py = Math.max(5, height / 2 - radius * .7)
                                        c.beginPath();c.moveTo(px-3,py);c.lineTo(px+3,py)
                                        if(sign>0){c.moveTo(px,py-3);c.lineTo(px,py+3)}
                                        c.lineWidth=3;c.strokeStyle="rgba(0,0,0,.85)";c.stroke()
                                        c.lineWidth=1;c.strokeStyle="#ffffff";c.stroke()
                                    }
                                }
                                Connections { target: editor; function onSettingsChanged() { brushCursor.requestPaint() } }
                                Connections { target: window; function onZoomChanged() { brushCursor.requestPaint() } }
                            }
                        }
                    }
                    MouseArea {
                        id: zoomToolArea
                        objectName: "zoomToolArea"
                        anchors.fill: viewport
                        z: 22
                        enabled: editor.tool === "zoomTool" && !window.spacePressed
                        hoverEnabled: enabled
                        preventStealing: true
                        cursorShape: Qt.CrossCursor
                        property real pressX: 0
                        property real pressY: 0
                        property real initialZoom: 1
                        property bool dragged: false
                        property bool zoomOut: false
                        onPressed: function(mouse) {
                            workspace.forceActiveFocus()
                            pressX=mouse.x;pressY=mouse.y;initialZoom=window.zoom;dragged=false
                            zoomOut=Boolean(mouse.modifiers & Qt.AltModifier)
                        }
                        onPositionChanged: function(mouse) {
                            if(!pressed) return
                            const delta=mouse.x-pressX
                            if(Math.abs(delta)<3 && !dragged) return
                            dragged=true
                            const factor=Math.exp((zoomOut?-delta:delta)/160)
                            window.zoomAt(initialZoom*factor,pressX,pressY)
                        }
                        onReleased: function(mouse) {
                            if(!dragged) window.zoomAt(window.zoom*(zoomOut?0.8:1.25),mouse.x,mouse.y)
                        }
                    }
                    MouseArea {
                        id: rotateToolArea
                        objectName: "rotateToolArea"
                        anchors.fill: viewport
                        z: 22
                        enabled: editor.tool === "rotateView" && !window.spacePressed
                        hoverEnabled: enabled
                        preventStealing: true
                        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                        property real startPointerAngle: 0
                        property real startCanvasRotation: 0
                        function pointerAngle(x,y) {
                            const center=artboard.mapToItem(rotateToolArea,artboard.width/2,artboard.height/2)
                            return Math.atan2(y-center.y,x-center.x)*180/Math.PI
                        }
                        onPressed: function(mouse) {
                            workspace.forceActiveFocus()
                            startPointerAngle=pointerAngle(mouse.x,mouse.y)
                            startCanvasRotation=window.canvasRotation
                        }
                        onPositionChanged: function(mouse) {
                            if(pressed) window.canvasRotation=startCanvasRotation
                                    + pointerAngle(mouse.x,mouse.y)-startPointerAngle
                        }
                        onDoubleClicked: window.canvasRotation=0
                    }
                    MouseArea {
                        id: panArea
                        objectName: "panArea"
                        anchors.fill: viewport
                        z: 21
                        enabled: window.spacePressed || editor.tool==="hand"
                        hoverEnabled: enabled
                        preventStealing: true
                        cursorShape: enabled && pressed ? Qt.ClosedHandCursor
                                                       : window.canvasCursorShape
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
                id: dockArea
                Layout.preferredWidth: window.dockWidth
                Layout.minimumWidth: 230
                Layout.maximumWidth: 760
                Layout.fillHeight: true
                color: "#20262e"

                MouseArea {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 7
                    z: 20
                    cursorShape: Qt.SizeHorCursor
                    property real pressX: 0
                    property real initialWidth: 0
                    onPressed: function(mouse) {
                        pressX = mapToGlobal(mouse.x, mouse.y).x
                        initialWidth = window.dockWidth
                    }
                    onPositionChanged: function(mouse) {
                        if (!pressed) return
                        const globalX = mapToGlobal(mouse.x, mouse.y).x
                        window.dockWidth = Math.max(230, Math.min(760, initialWidth + pressX - globalX))
                    }
                }

                Flickable {
                    id: dockScroll
                    anchors.fill: parent
                    anchors.leftMargin: 7
                    clip: true
                    contentWidth: width
                    contentHeight: dockGrid.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    GridLayout {
                    id: dockGrid
                    width: dockScroll.width
                    columns: 2
                    columnSpacing: 4
                    rowSpacing: 4

                    DockablePanel {
                        id: histogramDock
                        objectName: "histogramDockPanel"
                        panelId: "histogram"
                        title: qsTr("Histogramme")
                        panelShown: window.dockPanelVisible(panelId)
                        dockedHeight: 142
                        minimumContentHeight: 70
                        property int dockColumn: window.dockPanelColumn(panelId)
                        groupTabs: window.dockGroupTabs(panelId)
                        activeGroupPanel: window.activeDockPanel(panelId)
                        dropIndicator: window.dockDropPanelId === panelId ? window.dockDropZone : ""
                        Layout.column: dockColumn
                        Layout.row: window.dockPanelRow(panelId)
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        onGroupTabSelected: function(id) { window.selectDockPanel(id) }
                        onUngroupRequested: function(id) { window.ungroupDockPanel(id) }
                        onDropRequested: function(gx, gy, dx, dy) { window.handleDockDrop(panelId, gx, gy, dx, dy) }
                        onDragUpdated: function(gx, gy) { window.updateDockDropIndicator(panelId, gx, gy) }
                        onDragFinished: window.clearDockDropIndicator()
                        onFloatingChangedByUser: function(isFloating) { if (isFloating) window.ungroupDockPanel(panelId) }
                        onLayoutChanged: window.scheduleWorkspaceSave()
                        contentComponent: Component { Rectangle {
                            anchors.fill: parent
                            color: "#181e25"; radius: 5
                            property var histogramData: (editor.revision, editor.colorHistogram())
                            onHistogramDataChanged: histogramCanvas.requestPaint()
                            Canvas {
                                id: histogramCanvas
                                objectName: "colorHistogramCanvas"
                                anchors.fill: parent; anchors.margins: 8
                                onPaint: {
                                    const context = getContext("2d")
                                    context.reset()
                                    context.fillStyle = "#14191f"
                                    context.fillRect(0, 0, width, height)
                                    const data = parent.histogramData
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
                            }
                        } }
                    }

                    DockablePanel {
                        id: navigatorDock
                        objectName: "navigatorDockPanel"
                        panelId: "navigator"
                        title: qsTr("Navigateur")
                        panelShown: window.dockPanelVisible(panelId)
                        dockedHeight: 155
                        minimumContentHeight: 80
                        property int dockColumn: window.dockPanelColumn(panelId)
                        groupTabs: window.dockGroupTabs(panelId)
                        activeGroupPanel: window.activeDockPanel(panelId)
                        dropIndicator: window.dockDropPanelId === panelId ? window.dockDropZone : ""
                        Layout.column: dockColumn
                        Layout.row: window.dockPanelRow(panelId)
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        onGroupTabSelected: function(id) { window.selectDockPanel(id) }
                        onUngroupRequested: function(id) { window.ungroupDockPanel(id) }
                        onDropRequested: function(gx, gy, dx, dy) { window.handleDockDrop(panelId, gx, gy, dx, dy) }
                        onDragUpdated: function(gx, gy) { window.updateDockDropIndicator(panelId, gx, gy) }
                        onDragFinished: window.clearDockDropIndicator()
                        onFloatingChangedByUser: function(isFloating) { if (isFloating) window.ungroupDockPanel(panelId) }
                        onLayoutChanged: window.scheduleWorkspaceSave()
                        contentComponent: Component { ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 7
                            spacing: 4
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#181e25"; radius: 5
                            Image {
                                anchors.fill: parent; anchors.margins: 8
                                source: "image://layers/composite/"+editor.revision
                                sourceSize.width: 240; sourceSize.height: 105
                                fillMode: Image.PreserveAspectFit; cache: false
                            }
                        }
                    RowLayout {
                        visible: window.height >= 800 && !window.inspectorCollapsed
                        Layout.fillWidth: true
                        Label { text: editor.width+" × "+editor.height+" px"; color: "#9ba7b5"; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Label { text: qsTr("sRGB"); color: "#74828e"; font.pixelSize: 10 }
                    }
                        } }
                    }

                    DockablePanel {
                        id: colorDock
                        objectName: "colorDockPanel"
                        panelId: "color"
                        title: qsTr("Couleur")
                        panelShown: window.dockPanelVisible(panelId)
                        dockedHeight: 118
                        minimumContentHeight: 65
                        property int dockColumn: window.dockPanelColumn(panelId)
                        groupTabs: window.dockGroupTabs(panelId)
                        activeGroupPanel: window.activeDockPanel(panelId)
                        dropIndicator: window.dockDropPanelId === panelId ? window.dockDropZone : ""
                        Layout.column: dockColumn
                        Layout.row: window.dockPanelRow(panelId)
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        onGroupTabSelected: function(id) { window.selectDockPanel(id) }
                        onUngroupRequested: function(id) { window.ungroupDockPanel(id) }
                        onDropRequested: function(gx, gy, dx, dy) { window.handleDockDrop(panelId, gx, gy, dx, dy) }
                        onDragUpdated: function(gx, gy) { window.updateDockDropIndicator(panelId, gx, gy) }
                        onDragFinished: window.clearDockDropIndicator()
                        onFloatingChangedByUser: function(isFloating) { if (isFloating) window.ungroupDockPanel(panelId) }
                        onLayoutChanged: window.scheduleWorkspaceSave()
                        contentComponent: Component { ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 7
                            spacing: 5
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
                        } }
                    }

                    DockablePanel {
                        id: layersDock
                        objectName: "layersDockPanel"
                        panelId: "layers"
                        title: qsTr("Calques")
                        panelShown: window.dockPanelVisible(panelId)
                        dockedHeight: 370
                        minimumContentHeight: 170
                        property int dockColumn: window.dockPanelColumn(panelId)
                        groupTabs: window.dockGroupTabs(panelId)
                        activeGroupPanel: window.activeDockPanel(panelId)
                        dropIndicator: window.dockDropPanelId === panelId ? window.dockDropZone : ""
                        Layout.column: dockColumn
                        Layout.row: window.dockPanelRow(panelId)
                        Layout.fillWidth: true
                        Layout.minimumHeight: floating || collapsed ? implicitHeight : 210
                        Layout.preferredHeight: implicitHeight
                        onGroupTabSelected: function(id) { window.selectDockPanel(id) }
                        onUngroupRequested: function(id) { window.ungroupDockPanel(id) }
                        onDropRequested: function(gx, gy, dx, dy) { window.handleDockDrop(panelId, gx, gy, dx, dy) }
                        onDragUpdated: function(gx, gy) { window.updateDockDropIndicator(panelId, gx, gy) }
                        onDragFinished: window.clearDockDropIndicator()
                        onFloatingChangedByUser: function(isFloating) { if (isFloating) window.ungroupDockPanel(panelId) }
                        onLayoutChanged: window.scheduleWorkspaceSave()
                        contentComponent: Component { ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 7
                            spacing: 5
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
                            id: layerDelegate
                            objectName: "layerDelegate"
                            required property var modelData
                            width: layerList.width; height: 64; radius: 5
                            color: modelData.layerIndex===editor.activeLayer ? "#35443e" : modelData.selected ? "#303b48" : "#292f38"
                            border.width: modelData.selected ? 2 : 1
                            border.color: modelData.layerIndex===editor.activeLayer ? "#78a589" : modelData.selected ? "#66829b" : "#343c46"
                            DropArea {
                                id: maskDropArea
                                anchors.fill: parent
                                z: 8
                                keys: ["photocrap-layer-mask"]
                                property bool validDrop: containsDrag && drag.source &&
                                                         drag.source.sourceLayerIndex !== modelData.layerIndex &&
                                                         modelData.kind !== "group" && !modelData.locked
                                onDropped: function(drop) {
                                    if (!validDrop)
                                        return
                                    editor.copyLayerMask(drop.source.sourceLayerIndex,
                                                         modelData.layerIndex)
                                    drop.acceptProposedAction()
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    visible: maskDropArea.validDrop
                                    color: "#235f4438"
                                    border.width: 3
                                    border.color: "#c5f2cf"
                                    radius: 5
                                }
                            }
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
                                    MouseArea { anchors.fill: parent; onClicked: { editor.activeLayer=modelData.layerIndex; editor.setEditingMask(false); editor.showCompositeView() } }
                                }
                                Rectangle {
                                    id: maskThumbnail
                                    visible: modelData.hasMask
                                    width: visible?34:0; height: 36; color: "#182027"; radius: 3
                                    border.width: modelData.editingMask?2:1; border.color: modelData.editingMask?"#c5f2cf":"#69757f"
                                    Image { anchors.fill: parent; anchors.margins: 2; source: modelData.hasMask?"image://layers/mask/"+modelData.layerIndex+"/"+editor.thumbnailRevision:""; sourceSize.width: 64; sourceSize.height: 64; fillMode: Image.PreserveAspectFit; cache: false }
                                    Label { anchors.centerIn: parent; visible: !modelData.maskEnabled; text: qsTr("×"); color: "#ef6b6b"; font.pixelSize: 34; font.bold: true }
                                    MouseArea {
                                        id: maskDrag
                                        objectName: "maskThumbnailMouse"
                                        anchors.fill: parent
                                        drag.target: maskDragProxy
                                        drag.threshold: 6
                                        onClicked: function(mouse) {
                                            window.activateMaskThumbnail(modelData.layerIndex,
                                                                         mouse.modifiers)
                                        }
                                        onReleased: {
                                            maskDragProxy.Drag.drop()
                                            maskDragProxy.x = 0
                                            maskDragProxy.y = 0
                                        }
                                    }
                                    Item {
                                        id: maskDragProxy
                                        property int sourceLayerIndex: modelData.layerIndex
                                        x: 0; y: 0; width: maskThumbnail.width; height: maskThumbnail.height
                                        z: 20
                                        visible: Drag.active
                                        Drag.active: maskDrag.drag.active
                                        Drag.keys: ["photocrap-layer-mask"]
                                        Drag.source: maskDragProxy
                                        Drag.hotSpot.x: width / 2
                                        Drag.hotSpot.y: height / 2
                                        Drag.supportedActions: Qt.CopyAction
                                        Drag.proposedAction: Qt.CopyAction
                                        Rectangle {
                                            anchors.fill: parent
                                            color: "#263039"
                                            border.width: 2
                                            border.color: "#c5f2cf"
                                            radius: 3
                                            opacity: .94
                                            Image {
                                                anchors.fill: parent
                                                anchors.margins: 2
                                                source: "image://layers/mask/" + modelData.layerIndex +
                                                        "/" + editor.thumbnailRevision
                                                fillMode: Image.PreserveAspectFit
                                                cache: false
                                            }
                                        }
                                    }
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
                        IconButton { objectName: "layerPropertiesButton"; glyph: "ƒ"; hint: qsTr("Propriétés du calque"); enabled: activeInfo.kind!=="group" && (activeInfo.kind!=="pixel" || activeInfo.editingMask); onClicked: layerPropertiesDialog.open() }
                        IconButton { glyph: "↑"; hint: qsTr("Monter le calque"); enabled: activeInfo.kind!=="group" && editor.activeLayer<editor.layers.length-1; onClicked: editor.moveLayer(1) }
                        IconButton { glyph: "↓"; hint: qsTr("Descendre le calque"); enabled: activeInfo.kind!=="group" && editor.activeLayer>0; onClicked: editor.moveLayer(-1) }
                        Item { Layout.fillWidth: true }
                        IconButton { glyph: "×"; hint: qsTr("Supprimer le calque"); enabled: editor.layers.length>1; onClicked: editor.removeLayer() }
                    }
                        } }
                    }

                    DockablePanel {
                        id: channelsDock
                        objectName: "channelsDockPanel"
                        panelId: "channels"
                        title: qsTr("Couches")
                        panelShown: window.dockPanelVisible(panelId)
                        dockedHeight: 260
                        minimumContentHeight: 120
                        property int dockColumn: window.dockPanelColumn(panelId)
                        groupTabs: window.dockGroupTabs(panelId)
                        activeGroupPanel: window.activeDockPanel(panelId)
                        dropIndicator: window.dockDropPanelId === panelId ? window.dockDropZone : ""
                        Layout.column: dockColumn
                        Layout.row: window.dockPanelRow(panelId)
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        onGroupTabSelected: function(id) { window.selectDockPanel(id) }
                        onUngroupRequested: function(id) { window.ungroupDockPanel(id) }
                        onDropRequested: function(gx, gy, dx, dy) { window.handleDockDrop(panelId, gx, gy, dx, dy) }
                        onDragUpdated: function(gx, gy) { window.updateDockDropIndicator(panelId, gx, gy) }
                        onDragFinished: window.clearDockDropIndicator()
                        onFloatingChangedByUser: function(isFloating) { if (isFloating) window.ungroupDockPanel(panelId) }
                        onLayoutChanged: window.scheduleWorkspaceSave()
                        contentComponent: Component { ListView {
                            id: channelList
                            objectName: "channelList"
                            anchors.fill: parent
                            anchors.margins: 7
                            clip: true
                            spacing: 5
                            model: [
                                {name:qsTr("RVB"), description:qsTr("Couleur composite"), color:"#d9dde1"},
                                {name:qsTr("Rouge"), description:qsTr("Canal rouge"), color:"#d84b45"},
                                {name:qsTr("Vert"), description:qsTr("Canal vert"), color:"#42ad61"},
                                {name:qsTr("Bleu"), description:qsTr("Canal bleu"), color:"#4879d5"}
                            ]
                            ScrollBar.vertical: ScrollBar {}
                            delegate: Rectangle {
                                required property var modelData
                                required property int index
                                width: channelList.width; height: 54; radius: 5
                                color: editor.displayChannel === index ? "#35443e" : "#292f38"
                                border.width: editor.displayChannel === index ? 2 : 1
                                border.color: editor.displayChannel === index ? "#78a589" : "#3c4650"
                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 7; spacing: 10
                                    Label {
                                        text: editor.displayChannel === index ? "◉" : "○"
                                        color: editor.displayChannel === index ? "#d8f1df" : "#87939e"
                                        font.pixelSize: 18
                                    }
                                    Rectangle {
                                        width: 38; height: 34; radius: 4; color: "#171c22"
                                        border.width: 1; border.color: "#56616c"
                                        Rectangle {
                                            anchors.centerIn: parent; width: 23; height: 23; radius: index === 0 ? 12 : 4
                                            color: modelData.color
                                            border.width: 1; border.color: "#eef2f5"
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 2
                                        Label { text: modelData.name; color: "#d6e0db"; font.pixelSize: 11 }
                                        Label { text: modelData.description; color: "#80928c"; font.pixelSize: 9 }
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: editor.displayChannel = index
                                }
                            }
                        } }
                    }

                    DockablePanel {
                        id: informationDock
                        objectName: "informationDockPanel"
                        panelId: "information"
                        title: qsTr("Informations")
                        panelShown: window.dockPanelVisible(panelId)
                        dockedHeight: 300
                        minimumContentHeight: 110
                        property int dockColumn: window.dockPanelColumn(panelId)
                        groupTabs: window.dockGroupTabs(panelId)
                        activeGroupPanel: window.activeDockPanel(panelId)
                        dropIndicator: window.dockDropPanelId === panelId ? window.dockDropZone : ""
                        Layout.column: dockColumn
                        Layout.row: window.dockPanelRow(panelId)
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        onGroupTabSelected: function(id) { window.selectDockPanel(id) }
                        onUngroupRequested: function(id) { window.ungroupDockPanel(id) }
                        onDropRequested: function(gx, gy, dx, dy) { window.handleDockDrop(panelId, gx, gy, dx, dy) }
                        onDragUpdated: function(gx, gy) { window.updateDockDropIndicator(panelId, gx, gy) }
                        onDragFinished: window.clearDockDropIndicator()
                        onFloatingChangedByUser: function(isFloating) {
                            window.informationFloating = isFloating
                            if (isFloating) window.ungroupDockPanel(panelId)
                            window.scheduleWorkspaceSave()
                        }
                        hideOnClose: true
                        floatingHeight: 370
                        onHideRequested: window.setDockPanelVisible(panelId, false)
                        onLayoutChanged: window.scheduleWorkspaceSave()
                        contentComponent: Component { InformationPanel {
                            pointerPosition: window.informationPosition
                            pointerColor: window.informationColor
                            pointerValid: window.informationPointerValid
                            samples: window.colorSamples
                            rulerStart: window.rulerStart
                            rulerEnd: window.rulerEnd
                            rulerVisible: window.rulerVisible
                            selection: editor.selection
                            selectionVisible: editor.hasSelection
                            documentWidth: editor.width
                            documentHeight: editor.height
                        } }
                    }

                    DockablePanel {
                        id: historyDock
                        objectName: "historyDockPanel"
                        panelId: "history"
                        title: qsTr("Historique")
                        panelShown: window.dockPanelVisible(panelId)
                        dockedHeight: 118
                        minimumContentHeight: 55
                        property int dockColumn: window.dockPanelColumn(panelId)
                        groupTabs: window.dockGroupTabs(panelId)
                        activeGroupPanel: window.activeDockPanel(panelId)
                        dropIndicator: window.dockDropPanelId === panelId ? window.dockDropZone : ""
                        Layout.column: dockColumn
                        Layout.row: window.dockPanelRow(panelId)
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        onGroupTabSelected: function(id) { window.selectDockPanel(id) }
                        onUngroupRequested: function(id) { window.ungroupDockPanel(id) }
                        onDropRequested: function(gx, gy, dx, dy) { window.handleDockDrop(panelId, gx, gy, dx, dy) }
                        onDragUpdated: function(gx, gy) { window.updateDockDropIndicator(panelId, gx, gy) }
                        onDragFinished: window.clearDockDropIndicator()
                        onFloatingChangedByUser: function(isFloating) { if (isFloating) window.ungroupDockPanel(panelId) }
                        onLayoutChanged: window.scheduleWorkspaceSave()
                        contentComponent: Component { ListView {
                        objectName: "historyStateList"
                        anchors.fill: parent; anchors.margins: 7; clip: true
                        model: editor.history; currentIndex: editor.historyIndex
                        onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Item {
                            required property string modelData
                            required property int index
                            width: ListView.view.width; height: 28
                            RowLayout {
                                anchors.fill: parent; spacing: 5
                                ToolButton {
                                    objectName: "historyBrushSource" + index
                                    implicitWidth: 28; implicitHeight: 28
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Utiliser cet état comme source de la forme d’historique")
                                    onClicked: editor.setHistoryBrushSource(index)
                                    background: Rectangle {
                                        radius: 4
                                        color: editor.historyBrushSourceIndex === index ? "#345344" : parent.hovered ? "#343f49" : "#20262e"
                                        border.color: editor.historyBrushSourceIndex === index ? "#74b393" : "#46515d"
                                    }
                                    contentItem: ToolIcon {
                                        toolName: "historyBrush"
                                        iconColor: editor.historyBrushSourceIndex === index ? "#c0f1d2" : "#a3afbb"
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: (index===editor.historyIndex?"●  ":"    ")+modelData
                                    color: index===editor.historyIndex?"#beeccb":index>editor.historyIndex?"#65707b":"#9ba8b6"
                                    font.pixelSize: 11; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                                }
                            }
                        }
                    } }
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
                Label { text: qsTr("%1 × %2 px   ·   %3 calques   ·   PhotoCrap %4").arg(editor.width).arg(editor.height).arg(editor.layers.length).arg(Qt.application.version); color: "#73818f"; font.pixelSize: 10 }
            }
        }
    }

    DarkMenu {
        id: createLayerMenu
        DarkMenuItem { text: qsTr("Calque de pixels"); onTriggered: editor.addLayer() }
        DarkMenuItem { text: qsTr("Séparation de fréquences…"); onTriggered: { editor.addFrequencySeparation(8); layerPropertiesDialog.open() } }
        MenuSeparator {}
        DarkMenuItem { text: qsTr("Couleur unie…"); onTriggered: addAdjustmentLayer("solidColor") }
        DarkMenuItem { text: qsTr("Lumière"); onTriggered: addAdjustmentLayer("light") }
        DarkMenuItem { text: qsTr("Niveaux"); onTriggered: addAdjustmentLayer("levels") }
        DarkMenuItem { text: qsTr("Courbes"); onTriggered: addAdjustmentLayer("curves") }
        DarkMenuItem { text: qsTr("Couleur"); onTriggered: addAdjustmentLayer("color") }
        DarkMenuItem { text: qsTr("Balance des couleurs"); onTriggered: addAdjustmentLayer("colorBalance") }
        DarkMenuItem { text: qsTr("Noir et blanc"); onTriggered: addAdjustmentLayer("blackWhite") }
        DarkMenuItem { text: qsTr("Filtre photo"); onTriggered: addAdjustmentLayer("photoFilter") }
        DarkMenuItem { text: qsTr("Mélangeur de couches"); onTriggered: addAdjustmentLayer("channelMixer") }
        DarkMenuItem { text: qsTr("Style colorimétrique (LUT)"); onTriggered: addAdjustmentLayer("colorLookup") }
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
    ColorDialog {
        id: colorDialog
        objectName: "colorDialog"
        title: qsTr("Choisir une couleur")
        onAccepted: {
            if (fillingMaskColor) {
                editor.fillMaskWithColor(selectedColor)
                fillingMaskColor = false
            } else if (changingShapeFill) {
                editor.shapeFillColor = selectedColor
                changingShapeFill = false
            } else if (changingShapeStroke) {
                editor.shapeStrokeColor = selectedColor
                changingShapeStroke = false
            } else if (changingPhotoFilterColor) {
                editor.beginLayerPreview()
                editor.previewLayerParameter("color", selectedColor.toString())
                editor.previewLayerParameter("useCustomColor", true)
                editor.finishLayerPreview(qsTr("Modifier la couleur du filtre photo"))
                changingPhotoFilterColor = false
            } else if (changingBlackWhiteTint) {
                editor.beginLayerPreview()
                editor.previewLayerParameter("tintColor", selectedColor.toString())
                editor.finishLayerPreview(qsTr("Modifier la teinte noir et blanc"))
                changingBlackWhiteTint = false
            } else if (changingSolidColor) {
                editor.beginLayerPreview()
                editor.previewLayerParameter("color", selectedColor.toString())
                editor.finishLayerPreview(qsTr("Modifier la couleur du calque"))
                changingSolidColor = false
            } else if (changingSecondary)
                editor.secondaryColor=selectedColor
            else
                editor.color=selectedColor
        }
        onRejected: {
            fillingMaskColor = false
            changingShapeFill = false
            changingShapeStroke = false
            changingSolidColor = false
            changingBlackWhiteTint = false
            changingPhotoFilterColor = false
        }
    }
    Dialog {
        id: newDialog; objectName: "newDialog"; title: qsTr("Nouveau document"); anchors.centerIn: parent; modal: true; width: 410
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
        footer: DialogButtonBox {
            DialogActionButton { objectName: "newDialogCancelButton"; text: qsTr("Annuler"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole; onClicked: newDialog.reject() }
            DialogActionButton { objectName: "newDialogOkButton"; text: qsTr("OK"); defaultButton: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: newDialog.accept() }
        }
        onAccepted: editor.newDocument(newWidth.value,newHeight.value,transparent.checked)
    }
    Dialog {
        id: resizeDialog; title: qsTr("Dimensions de l’image"); anchors.centerIn: parent; modal: true; width: 370
        ColumnLayout {
            anchors.fill: parent; spacing: 16
            RowLayout { Label { text: qsTr("Largeur"); Layout.preferredWidth: 100 } DarkSpinBox { id: resizeWidth; from: 1; to: 16384; suffix: " px" } }
            RowLayout { Label { text: qsTr("Hauteur"); Layout.preferredWidth: 100 } DarkSpinBox { id: resizeHeight; from: 1; to: 16384; suffix: " px" } }
            Label { text: qsTr("Tous les calques sont rééchantillonnés.\nLes proportions sont libres."); color: "#9eabb7" }
        }
        footer: DialogButtonBox {
            DialogActionButton { text: qsTr("Annuler"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole; onClicked: resizeDialog.reject() }
            DialogActionButton { text: qsTr("OK"); defaultButton: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: resizeDialog.accept() }
        }
        onAccepted: editor.resizeImage(resizeWidth.value,resizeHeight.value)
    }
    Dialog {
        id: renameDialog; title: qsTr("Renommer le calque"); anchors.centerIn: parent; modal: true; width: 350
        TextField { id: layerName; width: parent.width; maximumLength: 256; selectByMouse: true; onAccepted: renameDialog.accept() }
        footer: DialogButtonBox {
            DialogActionButton { text: qsTr("Annuler"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole; onClicked: renameDialog.reject() }
            DialogActionButton { text: qsTr("OK"); defaultButton: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: renameDialog.accept() }
        }
        onOpened: { layerName.forceActiveFocus(); layerName.selectAll() }
        onAccepted: editor.renameLayer(editor.activeLayer,layerName.text)
    }
    Dialog {
        id: adjustmentDialog; title: window.adjustmentTitle; anchors.centerIn: parent; modal: true; width: 390
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
        footer: DialogButtonBox {
            DialogActionButton { text: qsTr("Annuler"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole; onClicked: adjustmentDialog.reject() }
            DialogActionButton { text: qsTr("Appliquer"); defaultButton: true; DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole; onClicked: adjustmentDialog.apply() }
        }
        onApplied: { editor.applyFilter(window.adjustment,adjustmentSlider.value); close() }
    }
    Dialog {
        id: brushSettingsDialog
        objectName: "brushSettingsDialog"
        property var initialSettings: ({})
        property bool keepSettings: false
        title: qsTr("Paramètres de pinceau")
        x: Math.max(20,window.width-width-310)
        y: 95
        width: 440
        height: Math.min(650,window.height-130)
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        function captureSettings() {
            initialSettings = {
                brushSize: editor.brushSize,
                brushHardness: editor.brushHardness,
                brushOpacity: editor.brushOpacity,
                brushFlow: editor.brushFlow,
                brushSpacing: editor.brushSpacing,
                brushAngle: editor.brushAngle,
                brushRoundness: editor.brushRoundness,
                brushSmoothing: editor.brushSmoothing,
                brushBlend: editor.brushBlend
            }
        }
        function restoreSettings() {
            if (!initialSettings || initialSettings.brushSize === undefined)
                return
            editor.brushSize = initialSettings.brushSize
            editor.brushHardness = initialSettings.brushHardness
            editor.brushOpacity = initialSettings.brushOpacity
            editor.brushFlow = initialSettings.brushFlow
            editor.brushSpacing = initialSettings.brushSpacing
            editor.brushAngle = initialSettings.brushAngle
            editor.brushRoundness = initialSettings.brushRoundness
            editor.brushSmoothing = initialSettings.brushSmoothing
            editor.brushBlend = initialSettings.brushBlend
        }
        onOpened: {
            keepSettings = false
            captureSettings()
            brushPreview.requestPaint()
        }
        onClosed: {
            if (!keepSettings)
                restoreSettings()
        }

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
                visible: editor.tool !== "mixerBrush" && editor.tool !== "healing" && editor.tool !== "spotHealing"
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
                            visible: editor.tool !== "mixerBrush" || (modelData.key !== "brushOpacity" && modelData.key !== "brushFlow")
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
        footer: DialogButtonBox {
            alignment: Qt.AlignRight
            DialogActionButton {
                objectName: "brushSettingsCancelButton"
                text: qsTr("Annuler")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: brushSettingsDialog.reject()
            }
            DialogActionButton {
                objectName: "brushSettingsOkButton"
                text: qsTr("OK")
                defaultButton: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    brushSettingsDialog.keepSettings = true
                    brushSettingsDialog.accept()
                }
            }
        }
    }
    Dialog {
        id: layerPropertiesDialog
        objectName: "layerPropertiesDialog"
        property bool keepChanges: false
        title: qsTr("Propriétés · ") + activeInfo.name
        x: Math.max(20, window.width-width-310)
        y: 105
        width: 430
        height: Math.min(610, window.height-150)
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        onOpened: {
            keepChanges = false
            editor.beginLayerPropertiesSession()
        }
        onClosed: {
            window.curvePickerMode = ""
            window.colorPickerMode = ""
            window.cancelLayerPreviewQueue()
            editor.cancelLayerPreview()
            if (keepChanges)
                editor.acceptLayerPropertiesSession()
            else
                editor.cancelLayerPropertiesSession()
        }

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
                        ] : (activeInfo.adjustmentType==="curves" || activeInfo.adjustmentType==="levels" || activeInfo.adjustmentType==="light" || activeInfo.adjustmentType==="color" || activeInfo.adjustmentType==="colorBalance" || activeInfo.adjustmentType==="blackWhite" || activeInfo.adjustmentType==="photoFilter" || activeInfo.adjustmentType==="channelMixer" || activeInfo.adjustmentType==="colorLookup" || activeInfo.adjustmentType==="selectiveColor" || activeInfo.adjustmentType==="posterize" || activeInfo.adjustmentType==="threshold") ? []
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
                    RowLayout {
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType === "solidColor"
                        Layout.fillWidth: true
                        spacing: 12
                        Rectangle {
                            width: 52; height: 40; radius: 5
                            color: activeInfo.parameters.color || "#000000"
                            border.width: 1; border.color: "#84909a"
                        }
                        Label { text: qsTr("Couleur du remplissage"); color: "#d5dbe0"; Layout.fillWidth: true }
                        DialogActionButton {
                            objectName: "solidColorPickerButton"
                            text: qsTr("Choisir…")
                            onClicked: {
                                changingSolidColor = true
                                colorDialog.selectedColor = activeInfo.parameters.color || "#000000"
                                colorDialog.open()
                            }
                        }
                    }
                    CurvesEditor {
                        id: curvesEditorControl
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType==="curves"
                        Layout.fillWidth: true
                        histogram: (editor.revision, editor.histogram())
                        sourcePoints: activeInfo.parameters.points || [{"x":0,"y":0},{"x":255,"y":255}]
                        pickerMode: window.curvePickerMode
                        onPickerSelected: function(mode) { window.curvePickerMode = mode }
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(points) {
                            if(layerPreview.checked) window.queueLayerPreview("points",points)
                        }
                        onCommitted: function(points) {
                            window.previewLayerValue("points",points,true)
                        }
                    }
                    LightEditor {
                        objectName: "lightEditor"
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType === "light"
                        Layout.fillWidth: true
                        parameters: activeInfo.parameters
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) {
                            if (layerPreview.checked)
                                window.queueLayerPreview(key,value)
                        }
                        onCommitted: function(key,value) {
                            window.previewLayerValue(key,value,true)
                        }
                    }
                    ColorEditor {
                        id: colorEditorControl
                        objectName: "colorEditor"
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType === "color"
                        Layout.fillWidth: true
                        parameters: activeInfo.parameters
                        pickerMode: window.colorPickerMode
                        onPickerSelected: function(mode) { window.colorPickerMode = mode }
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) { if (layerPreview.checked) window.queueLayerPreview(key,value) }
                        onCommitted: function(key,value) { window.previewLayerValue(key,value,true) }
                    }
                    ColorBalanceEditor {
                        objectName: "colorBalanceEditor"
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType === "colorBalance"
                        Layout.fillWidth: true
                        parameters: activeInfo.parameters
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) { if (layerPreview.checked) window.queueLayerPreview(key,value) }
                        onCommitted: function(key,value) { window.previewLayerValue(key,value,true) }
                    }
                    BlackWhiteEditor {
                        objectName: "blackWhiteEditor"
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType === "blackWhite"
                        Layout.fillWidth: true
                        parameters: activeInfo.parameters
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) { if (layerPreview.checked) window.queueLayerPreview(key,value) }
                        onCommitted: function(key,value) { window.previewLayerValue(key,value,true) }
                        onPresetRequested: function(values) {
                            window.cancelLayerPreviewQueue()
                            editor.beginLayerPreview()
                            const keys = ["reds","yellows","greens","cyans","blues","magentas"]
                            for (let i=0; i<keys.length; ++i)
                                editor.previewLayerParameter(keys[i], values[i])
                            editor.finishLayerPreview(qsTr("Appliquer un style noir et blanc"))
                        }
                        onAutoRequested: {
                            const values = editor.autoBlackWhiteValues()
                            editor.beginLayerPreview()
                            const keys = ["reds","yellows","greens","cyans","blues","magentas"]
                            for (let i=0; i<keys.length; ++i)
                                editor.previewLayerParameter(keys[i], values[i])
                            editor.finishLayerPreview(qsTr("Réglage automatique noir et blanc"))
                        }
                        onTintToggled: function(enabled) {
                            editor.beginLayerPreview()
                            editor.previewLayerParameter("tintEnabled", enabled)
                            editor.finishLayerPreview(qsTr("Modifier la teinte noir et blanc"))
                        }
                        onTintColorRequested: {
                            changingBlackWhiteTint = true
                            colorDialog.selectedColor = activeInfo.parameters.tintColor || "#d6aa78"
                            colorDialog.open()
                        }
                    }
                    PhotoFilterEditor {
                        objectName: "photoFilterEditor"
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType === "photoFilter"
                        Layout.fillWidth: true
                        parameters: activeInfo.parameters
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) {
                            if (layerPreview.checked)
                                window.queueLayerPreview(key,value)
                        }
                        onCommitted: function(key,value) {
                            window.previewLayerValue(key,value,true)
                        }
                        onPresetRequested: function(index,color) {
                            window.cancelLayerPreviewQueue()
                            editor.beginLayerPreview()
                            editor.previewLayerParameter("preset", index)
                            editor.previewLayerParameter("color", color)
                            editor.previewLayerParameter("useCustomColor", false)
                            editor.finishLayerPreview(qsTr("Modifier le filtre photo"))
                        }
                        onColorRequested: {
                            changingPhotoFilterColor = true
                            colorDialog.selectedColor = activeInfo.parameters.color || "#ec8a00"
                            colorDialog.open()
                        }
                    }
                    ChannelMixerEditor {
                        objectName: "channelMixerEditor"
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType === "channelMixer"
                        Layout.fillWidth: true
                        parameters: activeInfo.parameters
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) {
                            if (layerPreview.checked)
                                window.queueLayerPreview(key,value)
                        }
                        onCommitted: function(key,value) {
                            window.previewLayerValue(key,value,true)
                        }
                        onPresetRequested: function(values) {
                            window.cancelLayerPreviewQueue()
                            editor.beginLayerPreview()
                            for (const key in values)
                                editor.previewLayerParameter(key, values[key])
                            editor.finishLayerPreview(qsTr("Appliquer un style de mélangeur de couches"))
                        }
                    }
                    ColorLookupEditor {
                        objectName: "colorLookupEditor"
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType === "colorLookup"
                        Layout.fillWidth: true
                        parameters: activeInfo.parameters
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) {
                            if (layerPreview.checked)
                                window.queueLayerPreview(key,value)
                        }
                        onCommitted: function(key,value) {
                            window.previewLayerValue(key,value,true)
                        }
                        onPresetRequested: function(index) {
                            window.cancelLayerPreviewQueue()
                            editor.beginLayerPreview()
                            editor.previewLayerParameter("preset", index)
                            editor.finishLayerPreview(qsTr("Modifier le style colorimétrique"))
                        }
                    }
                    SelectiveColorEditor {
                        objectName: "selectiveColorEditor"
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType === "selectiveColor"
                        Layout.fillWidth: true
                        parameters: activeInfo.parameters
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) {
                            if (layerPreview.checked)
                                window.queueLayerPreview(key,value)
                        }
                        onCommitted: function(key,value) {
                            window.previewLayerValue(key,value,true)
                        }
                    }
                    PosterizeEditor {
                        objectName: "posterizeEditor"
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType === "posterize"
                        Layout.fillWidth: true
                        parameters: activeInfo.parameters
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) {
                            if (layerPreview.checked)
                                window.queueLayerPreview(key,value)
                        }
                        onCommitted: function(key,value) {
                            window.previewLayerValue(key,value,true)
                        }
                    }
                    ThresholdEditor {
                        objectName: "thresholdEditor"
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType === "threshold"
                        Layout.fillWidth: true
                        histogram: (editor.revision, editor.histogram())
                        parameters: activeInfo.parameters
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) {
                            if (layerPreview.checked)
                                window.queueLayerPreview(key,value)
                        }
                        onCommitted: function(key,value) {
                            window.previewLayerValue(key,value,true)
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
                    GradientMapEditor {
                        objectName: "gradientMapEditor"
                        visible: !activeInfo.editingMask && activeInfo.adjustmentType==="gradientMap"
                        Layout.fillWidth: true
                        shadowColor: activeInfo.parameters.shadowColor || "#101828"
                        highlightColor: activeInfo.parameters.highlightColor || "#f5d9a8"
                        onEditStarted: editor.beginLayerPreview()
                        onPreviewed: function(key,value) {
                            if(layerPreview.checked)window.queueLayerPreview(key,value)
                        }
                        onCommitted: function(key,value) {
                            window.previewLayerValue(key,value,true)
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
        footer: Item {
            implicitHeight: 48
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8
                LayerPropertiesActions {
                    objectName: "layerPropertiesActions"
                    visible: !activeInfo.editingMask && activeInfo.kind === "adjustment"
                    layerVisible: activeInfo.visible
                    clippingMask: activeInfo.clippingMask
                    canClip: activeInfo.clippingMask || editor.activeLayer > 0
                    canDelete: editor.layers.length > 1 && !activeInfo.locked
                    onVisibilityToggled: editor.setLayerVisible(editor.activeLayer,
                                                                 !activeInfo.visible)
                    onClippingToggled: editor.setClippingMask(!activeInfo.clippingMask)
                    onResetRequested: {
                        window.curvePickerMode = ""
                        window.colorPickerMode = ""
                        window.cancelLayerPreviewQueue()
                        editor.cancelLayerPreview()
                        editor.resetAdjustmentLayer()
                    }
                    onDeleteRequested: {
                        layerPropertiesDialog.keepChanges = true
                        editor.removeLayer()
                        layerPropertiesDialog.accept()
                    }
                }
                Item { Layout.fillWidth: true }
                DialogActionButton {
                    objectName: "layerPropertiesCancelButton"
                    text: qsTr("Annuler")
                    onClicked: layerPropertiesDialog.reject()
                }
                DialogActionButton {
                    objectName: "layerPropertiesCloseButton"
                    text: qsTr("Fermer")
                    defaultButton: true
                    onClicked: {
                        window.flushLayerPreview()
                        editor.finishLayerPreview(qsTr("Modifier %1").arg(activeInfo.name))
                        layerPropertiesDialog.keepChanges = true
                        layerPropertiesDialog.accept()
                    }
                }
            }
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
                    ItemDelegate {
                        Layout.fillWidth: true
                        highlighted: window.preferencesPage===2
                        text: qsTr("▦   Barre d’outils")
                        Accessible.name: qsTr("Barre d’outils")
                        onClicked: window.preferencesPage=2
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
                ColumnLayout {
                    spacing: 10
                    Label { text: qsTr("Barre d’outils"); color: "#eef2f5"; font.pixelSize: 20; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Choisissez les outils visibles et leur ordre. Les outils d’un même cadre partagent un emplacement et un raccourci ; un clic droit sur cet emplacement ouvre le groupe.")
                        color: "#95a2af"; wrapMode: Text.WordWrap
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#39424c" }
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        Column {
                            id: toolSettingsList
                            objectName: "toolSettingsList"
                            width: parent.width
                            spacing: 8
                            Repeater {
                                model: (window.toolbarRevision, window.toolLayout)
                                delegate: Rectangle {
                                    id: groupCard
                                    required property var modelData
                                    required property int index
                                    property var groupData: modelData
                                    width: toolSettingsList.width
                                    height: groupHeader.height + toolRows.height + 14
                                    radius: 6
                                    color: "#242b33"
                                    border.color: "#39434e"
                                    Column {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.margins: 7
                                        spacing: 4
                                        RowLayout {
                                            id: groupHeader
                                            width: parent.width
                                            height: 32
                                            Label {
                                                Layout.fillWidth: true
                                                text: window.toolGroupLabel(groupCard.groupData)
                                                color: "#dce3e9"; font.bold: true; elide: Text.ElideRight
                                            }
                                            Label {
                                                text: window.toolGroupShortcut(groupCard.groupData)
                                                color: "#88a394"; font.bold: true
                                            }
                                            ToolButton {
                                                text: "↑"; enabled: groupCard.index > 0
                                                Accessible.name: qsTr("Monter le groupe")
                                                onClicked: window.moveToolGroup(groupCard.index, -1)
                                            }
                                            ToolButton {
                                                text: "↓"; enabled: groupCard.index + 1 < window.toolLayout.length
                                                Accessible.name: qsTr("Descendre le groupe")
                                                onClicked: window.moveToolGroup(groupCard.index, 1)
                                            }
                                        }
                                        Column {
                                            id: toolRows
                                            width: parent.width
                                            Repeater {
                                                model: groupCard.groupData.tools
                                                delegate: RowLayout {
                                                    required property string modelData
                                                    required property int index
                                                    readonly property var itemTool: window.toolData(modelData)
                                                    width: toolRows.width
                                                    height: 34
                                                    CheckBox {
                                                        enabled: itemTool && itemTool.implemented
                                                        checked: enabled && groupCard.groupData.hidden.indexOf(modelData) < 0
                                                        Accessible.name: itemTool ? qsTr("Afficher %1").arg(itemTool.label) : ""
                                                        onToggled: window.setToolVisible(groupCard.groupData.id, modelData, checked)
                                                    }
                                                    ToolIcon {
                                                        Layout.preferredWidth: 25
                                                        Layout.preferredHeight: 25
                                                        toolName: itemTool ? itemTool.name : ""
                                                        fallback: itemTool ? itemTool.icon : "?"
                                                        fallbackPixelSize: 17
                                                        iconColor: itemTool && itemTool.implemented ? "#c9d0d8" : "#66717d"
                                                    }
                                                    Label {
                                                        Layout.fillWidth: true
                                                        text: itemTool ? itemTool.label + (itemTool.implemented ? "" : "  (NYI)") : modelData
                                                        color: itemTool && itemTool.implemented ? "#cbd3da" : "#6f7a85"
                                                    }
                                                    ToolButton {
                                                        text: "↑"; enabled: itemTool && itemTool.implemented && index > 0
                                                        Accessible.name: qsTr("Monter l’outil")
                                                        onClicked: window.moveToolInGroup(groupCard.groupData.id, index, -1)
                                                    }
                                                    ToolButton {
                                                        text: "↓"; enabled: itemTool && itemTool.implemented && index + 1 < groupCard.groupData.tools.length
                                                        Accessible.name: qsTr("Descendre l’outil")
                                                        onClicked: window.moveToolInGroup(groupCard.groupData.id, index, 1)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        footer: DialogButtonBox {
            DialogActionButton {
                text: qsTr("Valeurs par défaut")
                DialogButtonBox.buttonRole: DialogButtonBox.ResetRole
                onClicked: {
                    if (window.preferencesPage === 0) shortcuts.resetDefaults()
                    else if (window.preferencesPage === 2) window.resetToolLayout()
                }
            }
            DialogActionButton {
                text: qsTr("Fermer")
                defaultButton: true
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
            DialogActionButton { text: qsTr("Annuler"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole; onClicked: unsavedDialog.close() }
            DialogActionButton { text: qsTr("Abandonner"); DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole; onClicked: { unsavedDialog.close(); window.perform(window.pendingAction) } }
            DialogActionButton { text: qsTr("Enregistrer"); defaultButton: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: { unsavedDialog.close(); window.resumeAfterSave=true; window.save(false) } }
        }
    }
    Dialog {
        id: errorDialog; title: qsTr("Impossible de terminer l’opération"); anchors.centerIn: parent; modal: true; width: 460
        Label { id: errorLabel; width: parent.width; wrapMode: Text.WordWrap }
        footer: DialogButtonBox { DialogActionButton { text: qsTr("OK"); defaultButton: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: errorDialog.accept() } }
    }
    Dialog {
        id: aboutDialog; title: qsTr("PhotoCrap %1").arg(Qt.application.version); anchors.centerIn: parent; modal: true; width: 480
        Label {
            width: parent.width; wrapMode: Text.WordWrap; lineHeight: 1.4
            text: qsTr("Un atelier de création d’images en C++ et Qt Quick.\n\nCalques, dessin, sélection, transformations et retouche, sur macOS, Linux et Windows.\n\nCette première version utilise des calques raster en sRGB 8 bits. Elle ne lit pas les PSD et n’inclut pas les objets dynamiques, les masques de calque, le RAW ou les fonctions IA.\n\nLogiciel indépendant, sans affiliation avec Adobe.")
        }
        footer: DialogButtonBox { DialogActionButton { text: qsTr("OK"); defaultButton: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole; onClicked: aboutDialog.accept() } }
    }
}
