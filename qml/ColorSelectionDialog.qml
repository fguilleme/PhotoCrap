import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PhotoCrap.Native

Dialog {
    id: root
    objectName: "colorSelectionDialog"
    required property var document
    title: qsTr("Plage de couleurs")
    modal: false
    closePolicy: Popup.NoAutoClose
    parent: Overlay.overlay
    x: Math.max(0,Math.min(parent.width-width-16,parent.width*.5-width*.25))
    y: Math.max(8,(parent.height-height)/2)
    width: Math.min(760, parent.width - 32)
    property int sampleMode: 0
    property real previewZoom: 0
    property real previewCenterX: .5
    property real previewCenterY: .5
    property var previewControl: null
    readonly property bool canApply: document.colorSelectionReady && !refresh.running
    function sampleAt(x,y,modifiers) {
        if(x<0 || y<0 || x>=document.width || y>=document.height)return
        family.currentIndex=0
        refresh.stop()
        document.sampleSelectionColor(x,y,(modifiers & Qt.AltModifier)?2:(modifiers & Qt.ShiftModifier)?1:sampleMode,options())
    }
    function zoomPreview(factor) {previewZoom=Math.max(.02,Math.min(16,(previewControl?previewControl.scaleFactor:1)*factor))}
    function fitPreview() {previewZoom=0;previewCenterX=.5;previewCenterY=.5}
    readonly property var types: ["sampled","reds","yellows","greens","cyans","blues","magentas","highlights","midtones","shadows"]
    function options() { return {type:types[family.currentIndex],fuzziness:fuzz.value,localized:localized.checked,contiguous:contiguous.checked,range:range.value,invert:inverse.checked} }
    function recalculate() { if(opened) refresh.restart() }
    function start() { fitPreview();family.currentIndex=0;contiguous.checked=false;fuzz.value=40;localized.checked=false;range.value=100;inverse.checked=false;sampleMode=0;document.beginColorSelection();open() }
    Shortcut { sequence: "Escape"; enabled: root.visible; onActivated: root.reject() }
    Timer { id: refresh; interval: 50; onTriggered: root.document.previewColorSelection(root.options()) }
    onRejected: {refresh.stop();document.cancelColorSelection()}
    onAccepted: {refresh.stop();document.acceptColorSelection()}
    header: Item {
        implicitHeight: 36
        Label { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: root.title; color: "#dbe1e8"; font.bold: true }
        MouseArea {
            objectName: "colorSelectionDragHandle"; anchors.fill: parent; cursorShape: Qt.SizeAllCursor
            property point pressPoint
            property real initialX: 0
            property real initialY: 0
            onPressed: function(mouse) {pressPoint=mapToItem(root.parent,mouse.x,mouse.y);initialX=root.x;initialY=root.y}
            onPositionChanged: function(mouse) {
                if(!pressed)return
                const point=mapToItem(root.parent,mouse.x,mouse.y)
                root.x=Math.max(0,Math.min(root.parent.width-root.width,initialX+point.x-pressPoint.x))
                root.y=Math.max(0,Math.min(Math.max(0,root.parent.height-root.height),initialY+point.y-pressPoint.y))
            }
        }
    }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Label { text: qsTr("Sélectionner :"); color: "#dbe1e8" }
            ComboBox {
                id: family; objectName: "colorSelectionFamily"; Layout.fillWidth: true
                model: [qsTr("Couleurs prélevées"),qsTr("Rouges"),qsTr("Jaunes"),qsTr("Verts"),qsTr("Cyans"),qsTr("Bleus"),qsTr("Magentas"),qsTr("Hautes lumières"),qsTr("Tons moyens"),qsTr("Ombres")]
                onActivated: root.recalculate()
            }
        }
        Label { text: qsTr("Cliquez sur l’aperçu ou sur le canevas. Maj ajoute une couleur, Option/Alt la retire."); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#aab6c4" }
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            Repeater {
                model: 2
                ColumnLayout {
                    required property int index
                    Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 1
                    Label { Layout.fillWidth: true; elide: Text.ElideRight; text: index===0 ? qsTr("Image — pipette") : qsTr("Sélection — blanc : sélectionné"); color: "#dbe1e8" }
                    ColorSelectionPreview {
                        document: root.document; controls: root; mask: index===1
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(240,Math.max(130,root.parent.height-540))
                    }
                }
            }
        }
        RowLayout {
            Label { text: qsTr("Zoom :"); color: "#aab6c4" }
            Button { objectName: "colorSelectionZoomOut"; text: "−"; implicitWidth: 36; onClicked: root.zoomPreview(.8) }
            Label { objectName: "colorSelectionZoomLabel"; text: Math.round((root.previewControl?root.previewControl.scaleFactor:1)*100)+" %"; color: "#dbe1e8"; Layout.preferredWidth: 55 }
            Button { objectName: "colorSelectionZoomIn"; text: "+"; implicitWidth: 36; onClicked: root.zoomPreview(1.25) }
            Button { text: "100 %"; onClicked: root.previewZoom=1 }
            Button { text: qsTr("Adapter"); onClicked: root.fitPreview() }
            Item { Layout.fillWidth: true }
            Label { text: qsTr("Molette : zoom · Glisser : déplacer"); color: "#aab6c4" }
        }
        RowLayout {
            visible: family.currentIndex===0
            Label { text: qsTr("Pipette :"); color: "#dbe1e8" }
            Repeater {
                model: [qsTr("Remplacer"),qsTr("Ajouter (+)"),qsTr("Retirer (−)")]
                Button { required property int index; required property string modelData; text: modelData; checkable: true; checked: root.sampleMode===index; onClicked: root.sampleMode=index }
            }
            Item { Layout.fillWidth: true }
            Label { text: qsTr("%1 échantillon(s)").arg(root.document.colorSelectionSampleCount); color: "#aab6c4" }
        }
        RowLayout {
            Label { text: qsTr("Tolérance :"); color: "#dbe1e8" }
            Slider { enabled: family.currentIndex<7; id: fuzz; objectName: "colorSelectionFuzziness"; from: 0; to: 200; value: 40; stepSize: 1; Layout.fillWidth: true; onMoved: root.recalculate() }
            DarkSpinBox { enabled: family.currentIndex<7; from: 0; to: 200; value: fuzz.value; editable: true; onValueModified: {fuzz.value=value;root.recalculate()} }
        }
        RowLayout {
            CheckBox { id: localized; text: qsTr("Groupes de couleurs localisés"); enabled: family.currentIndex===0; onToggled: root.recalculate() }
            Label { text: qsTr("Portée :"); color: "#aab6c4" }
            Slider { id: range; from: 1; to: 100; value: 100; stepSize: 1; enabled: localized.checked; Layout.fillWidth: true; onMoved: root.recalculate() }
            Label { text: Math.round(range.value)+" %"; color: "#aab6c4" }
        }
        RowLayout {
            CheckBox { id: contiguous; objectName: "colorSelectionContiguous"; text: qsTr("Zones contiguës"); onToggled: root.recalculate(); ToolTip.visible: hovered; ToolTip.text: qsTr("Limiter aux zones reliées par leurs bords aux points prélevés") }
            CheckBox { id: inverse; text: qsTr("Inverser"); onToggled: root.recalculate() }
            Item { Layout.fillWidth: true }
            BusyIndicator { running: root.document.selectionBusy; visible: running; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
            Label { text: root.document.selectionBusy ? qsTr("Calcul en arrière-plan…") : qsTr("Les gris sont partiellement sélectionnés."); color: "#aab6c4" }
        }
    }
    footer: Item {
        implicitHeight: 44
        RowLayout {
            anchors.right: parent.right; anchors.bottom: parent.bottom
            anchors.rightMargin: 12; anchors.bottomMargin: 8; spacing: 8
            DialogActionButton { text: qsTr("Annuler"); onClicked: root.reject() }
            DialogActionButton { objectName: "colorSelectionApply"; text: qsTr("OK"); defaultButton: true; enabled: root.canApply; onClicked: root.accept() }
        }
    }
}
