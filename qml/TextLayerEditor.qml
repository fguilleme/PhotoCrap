import QtQuick
import QtQuick.Shapes
import PhotoCrap.Native 1.0

Item {
    id: root
    required property var document
    property real zoom: 1
    property int geometryRevision: 0
    readonly property var state: (geometryRevision, document.textEditState)
    readonly property var surfaceFrame: (state.surfaceOutline || []).map(p => Qt.point(p.x * zoom, p.y * zoom))
    readonly property bool hasSurfaceFrame: surfaceFrame.length > 3
    property bool loading: false
    property bool ready: false
    property int revision: 0
    property int focusRequest: 0
    property bool primaryPressed: false
    readonly property bool moveCursor: document.textEditing && (primaryPressed || textMove.moving || textMove.onFrame)
    readonly property bool pointerInside: visible && textMove.enabled && textMove.containsMouse
    readonly property var style: (revision, formatter.format(input.cursorPosition))
    property alias input: input
    property alias formatter: formatter
    visible: document.textEditing || (state.html !== undefined && !document.editingMask
                                      && !document.editingVectorMask && !document.transformActive)
    onVisibleChanged: if(!visible)primaryPressed=false
    width: Math.max(32,state.viewWidth || 32)
    height: Math.max(32,state.viewHeight || 64)
    function apply(values) {
        formatter.apply(values,input.selectionStart,input.selectionEnd)
        revision++
        sync()
        input.forceActiveFocus()
    }
    function sync() {
        if(loading || !ready || !document.textEditing || document.textGesture)return
        document.updateTextContent(input.text,formatter.snapshot())
    }
    function load(focus, point) {
        if(!document.textEditing)return
        ready=false
        loading=true
        input.text=state.html || ""
        formatter.restoreFormatting(state)
        loading=false
        ready=true
        revision++
        const request = ++focusRequest
        if(focus)Qt.callLater(function(){
            if(request !== root.focusRequest || !root.ready || !root.document.textEditing)return
            input.forceActiveFocus()
            input.cursorPosition=point!==undefined ? input.positionAt(point.x,point.y) : input.length
        })
    }
    function isOnSurfaceFrame(x, y) {
        for (let i = 1; i < surfaceFrame.length; ++i) {
            const a = surfaceFrame[i - 1], b = surfaceFrame[i]
            const dx = b.x - a.x, dy = b.y - a.y
            const lengthSquared = dx * dx + dy * dy
            const t = lengthSquared > 0
                      ? Math.max(0, Math.min(1, ((x - a.x) * dx + (y - a.y) * dy) / lengthSquared)) : 0
            const offsetX = x - a.x - t * dx, offsetY = y - a.y - t * dy
            if (offsetX * offsetX + offsetY * offsetY <= 25) return true
        }
        return false
    }
    function localPoint(point) {
        if (hasSurfaceFrame)
            return document.surfaceTextLocalPoint(Qt.point(point.x / zoom, point.y / zoom))
        return mapFromItem(parent, point.x, point.y)
    }
    function projectRect(rect) {
        return document.surfaceTextRect(rect).map(point => Qt.point(point.x * zoom, point.y * zoom))
    }
    transform: Matrix4x4 {
        matrix: {
            const m=root.state.transform || [1,0,0,0,1,0,0,0,1],z=root.zoom
            return Qt.matrix4x4(m[0]*z,m[3]*z,0,m[6]*z,
                               m[1]*z,m[4]*z,0,m[7]*z,
                               0,0,1,0,
                               m[2],m[5],0,m[8])
        }
    }
    Rectangle { anchors.fill: parent; visible: !root.hasSurfaceFrame; color: "transparent"; border.color: "#75b2f5"; border.width: 1/root.zoom }
    Shape {
        objectName: "surfaceTextOutline"
        parent: root.parent
        anchors.fill: parent
        z: root.z + 1
        visible: root.visible && root.hasSurfaceFrame
        ShapePath {
            fillColor: "transparent"
            strokeColor: "#75b2f5"
            strokeWidth: 1
            PathPolyline { path: root.surfaceFrame }
        }
    }
    Item {
        anchors.fill: parent
        visible:root.ready && root.document.textEditing
        clip: true
        Repeater {
            model: root.hasSurfaceFrame ? [] : (root.revision,root.formatter.selectionRects(input.selectionStart,input.selectionEnd))
            Rectangle {
                required property var modelData
                x:modelData.x;y:modelData.y;width:modelData.width;height:modelData.height
                color:"#70468cdf"
            }
        }
        Rectangle {
            x:input.cursorRectangle.x;y:input.cursorRectangle.y
            width:1/root.zoom;height:input.cursorRectangle.height
            color:"#5faaff";visible:!root.hasSurfaceFrame && input.activeFocus && input.selectionStart===input.selectionEnd && root.blink
        }
        TextEdit {
            id: input
            objectName: "canvasTextEditor"
            anchors.fill: parent
            // The native composite supplies the visible glyphs, including masks, opacity and blend.
            // This invisible Qt editor supplies selection, keyboard, clipboard and IME input.
            opacity: 0
            enabled: root.ready && root.document.textEditing && !root.document.textGesture
            textFormat: TextEdit.RichText
            selectByMouse: true
            persistentSelection: true
            color: root.state.color || "black"
            font.family: root.state.fontFamily || "sans-serif"
            font.pixelSize: root.state.fontSize || 64
            wrapMode: root.state.paragraph ? TextEdit.Wrap : TextEdit.NoWrap
            onTextChanged: {root.revision++;root.sync()}
            onCursorPositionChanged: {root.revision++;root.blink=true}
            onSelectionStartChanged: root.revision++
            onSelectionEndChanged: root.revision++
            Keys.onPressed: function(event) {
                root.primaryPressed=Boolean(event.modifiers & Qt.ControlModifier) || event.key===Qt.Key_Control
                const primary=Boolean(event.modifiers & (Qt.ControlModifier|Qt.MetaModifier))
                if(event.key===Qt.Key_Escape){root.document.finishTextEdit(true);event.accepted=true}
                else if(primary && (event.key===Qt.Key_Return || event.key===Qt.Key_Enter)) {
                    root.sync();root.document.finishTextEdit();event.accepted=true
                } else if(primary && event.key===Qt.Key_B){root.apply({bold:!root.style.bold});event.accepted=true}
                else if(primary && event.key===Qt.Key_I){root.apply({italic:!root.style.italic});event.accepted=true}
                else if(primary && event.key===Qt.Key_U){root.apply({underline:!root.style.underline});event.accepted=true}
            }
            Keys.onReleased: function(event) {
                root.primaryPressed=event.key!==Qt.Key_Control && Boolean(event.modifiers & Qt.ControlModifier)
            }
        }
    }
    Item {
        parent: root.parent
        anchors.fill: parent
        z: root.z + 1
        visible: root.hasSurfaceFrame && root.ready && root.document.textEditing
        Repeater {
            model: parent.visible ? (root.revision, root.formatter.selectionRects(input.selectionStart, input.selectionEnd)) : []
            Shape {
                required property var modelData
                anchors.fill: parent
                ShapePath {
                    strokeWidth: -1
                    fillColor: "#70468cdf"
                    PathPolyline { path: root.hasSurfaceFrame ? (root.state, root.zoom, root.projectRect(modelData)) : [] }
                }
            }
        }
        Shape {
            anchors.fill: parent
            visible: parent.visible && input.activeFocus && input.selectionStart === input.selectionEnd && root.blink
            ShapePath {
                strokeWidth: -1
                fillColor: "#5faaff"
                PathPolyline {
                    path: root.hasSurfaceFrame ? (root.state, root.zoom, root.projectRect(
                        Qt.rect(input.cursorRectangle.x, input.cursorRectangle.y,
                                1 / root.zoom, input.cursorRectangle.height))) : []
                }
            }
        }
    }
    MouseArea {
        id: textMove
        objectName: "textMoveMouse"
        parent: root.hasSurfaceFrame ? root.parent : root
        anchors.fill: parent
        anchors.margins: root.hasSurfaceFrame ? 0 : -4/root.zoom
        z: root.hasSurfaceFrame ? root.z + 2 : 1
        containmentMask: root.hasSurfaceFrame ? surfaceFrameHit : null
        visible: root.visible
        enabled: root.document.textEditing && root.ready && !root.document.textGesture
        hoverEnabled: true
        preventStealing: true
        acceptedButtons: Qt.LeftButton
        cursorShape: root.moveCursor ? Qt.SizeAllCursor : Qt.IBeamCursor
        property bool moving: false
        property bool onFrame: false
        property bool selecting: false
        property int selectionAnchor
        property point previous
        property point travelled: Qt.point(0,0)
        function updateFrame(mouse) {
            const px=mouse.x+x,py=mouse.y+y
            onFrame=root.hasSurfaceFrame ? root.isOnSurfaceFrame(px, py)
                                        : px<0 || py<0 || px>root.width || py>root.height
        }
        function moveTo(mouse) {
            if(!moving)return
            const p=mapToItem(root.parent,mouse.x,mouse.y)
            const dx=(p.x-previous.x)/root.zoom,dy=(p.y-previous.y)/root.zoom
            root.document.moveTextBy(dx,dy)
            travelled=Qt.point(travelled.x+dx,travelled.y+dy)
            previous=p
        }
        function positionAt(mouse) {
            const p = root.localPoint(mapToItem(root.parent, mouse.x, mouse.y))
            return input.positionAt(p.x, p.y)
        }
        function selectTo(position) {
            if (position === selectionAnchor) {
                input.deselect()
                input.cursorPosition = position
            } else {
                input.select(selectionAnchor, position)
            }
        }
        onPressed: function(mouse) {
            updateFrame(mouse)
            if (!(mouse.modifiers & Qt.ControlModifier) && !onFrame) {
                if (!root.hasSurfaceFrame) { mouse.accepted = false; return }
                const position = positionAt(mouse)
                selectionAnchor = mouse.modifiers & Qt.ShiftModifier
                    ? (input.cursorPosition === input.selectionStart ? input.selectionEnd : input.selectionStart)
                    : position
                selecting = true
                root.focusRequest++
                input.forceActiveFocus()
                selectTo(position)
                return
            }
            moving=true
            travelled=Qt.point(0,0)
            previous=mapToItem(root.parent,mouse.x,mouse.y)
        }
        onPositionChanged: function(mouse) {
            root.primaryPressed=Boolean(mouse.modifiers & Qt.ControlModifier)
            updateFrame(mouse)
            if (selecting) selectTo(positionAt(mouse))
            else moveTo(mouse)
        }
        onReleased: function(mouse) {
            if (selecting) selectTo(positionAt(mouse))
            else moveTo(mouse)
            selecting=false
            moving=false
            updateFrame(mouse)
            input.forceActiveFocus()
        }
        onDoubleClicked: {
            if (root.hasSurfaceFrame && !onFrame && !moving) {
                selecting = false
                input.selectWord()
            }
        }
        onCanceled: {
            if(moving)root.document.moveTextBy(-travelled.x,-travelled.y)
            moving=false
            selecting=false
        }
        onExited: onFrame=false
    }
    QtObject {
        id: surfaceFrameHit
        function contains(point: point): bool {
            if (root.isOnSurfaceFrame(point.x, point.y)) return true
            const p = root.localPoint(point)
            return p.x >= 0 && p.y >= 0 && p.x <= root.width && p.y <= root.height
        }
    }
    Rectangle {
        parent: root.hasSurfaceFrame ? root.parent : root
        visible: root.document.textEditing && Boolean(root.state.paragraph)
        width: root.hasSurfaceFrame ? 9 : 9/root.zoom
        height: width; color:"white";border.color:"#327ac6"
        anchors.right: root.hasSurfaceFrame ? undefined : parent.right
        anchors.bottom: root.hasSurfaceFrame ? undefined : parent.bottom
        x: root.hasSurfaceFrame ? root.state.surfaceCorner.x * root.zoom - width / 2 : 0
        y: root.hasSurfaceFrame ? root.state.surfaceCorner.y * root.zoom - height / 2 : 0
        z: root.hasSurfaceFrame ? root.z + 3 : 2
        MouseArea {
            anchors.fill:parent;anchors.margins:-5/root.zoom;cursorShape:Qt.SizeFDiagCursor
            property point initial
            property real initialWidth
            property real initialHeight
            onPressed: function(mouse){initial=root.localPoint(mapToItem(root.parent,mouse.x,mouse.y));initialWidth=root.width;initialHeight=root.height}
            onPositionChanged: function(mouse) {
                if(!pressed)return
                const p=root.localPoint(mapToItem(root.parent,mouse.x,mouse.y))
                root.document.setTextBox(true,Math.max(16,initialWidth+p.x-initial.x),Math.max(16,initialHeight+p.y-initial.y))
            }
        }
    }
    Text {
        visible: root.document.textEditing && Boolean(root.state.paragraph) && input.contentHeight>root.height
        anchors.right:parent.right;anchors.top:parent.bottom;text:"+";color:"#ff675f";font.pixelSize:14/root.zoom
    }
    property bool blink: true
    Timer {interval:500;running:root.visible && input.activeFocus;repeat:true;onTriggered:root.blink=!root.blink}
    TextFormatting { id:formatter; objectName:"textFormatter"; document:input.textDocument }
    Connections {
        target:root.document
        function onLayersChanged(){root.geometryRevision++}
        function onTextEditingChanged(){root.ready=false;root.primaryPressed=false}
        function onTextEditRequested(){root.load(true)}
        function onTextEditRequestedAt(x,y){root.load(true,Qt.point(x,y))}
    }
}
