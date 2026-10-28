import QtQuick
import QtQuick.Controls
import QtQuick.Window
import PhotoCrap.Native

Rectangle {
    id: root
    objectName: mask?"colorSelectionMaskPreview":"colorSelectionImagePreview"
    required property var document
    required property var controls
    property bool mask: false
    readonly property real viewWidth: Math.max(1,width-4)
    readonly property real viewHeight: Math.max(1,height-4)
    readonly property real fitScale: Math.min(viewWidth/Math.max(1,document.width),viewHeight/Math.max(1,document.height))
    readonly property real scaleFactor: controls.previewZoom>0 ? controls.previewZoom : fitScale
    readonly property int cropWidth: Math.max(1,Math.min(document.width,Math.ceil(viewWidth/scaleFactor)))
    readonly property int cropHeight: Math.max(1,Math.min(document.height,Math.ceil(viewHeight/scaleFactor)))
    readonly property int cropX: Math.max(0,Math.min(document.width-cropWidth,Math.floor(controls.previewCenterX*document.width-cropWidth/2)))
    readonly property int cropY: Math.max(0,Math.min(document.height-cropHeight,Math.floor(controls.previewCenterY*document.height-cropHeight/2)))
    color: "#151a20"
    border.color: "#657383"
    clip: true
    ImageCanvas { id: pointerCursor; width: 0; height: 0 }
    function documentPoint(x,y) {
        return Qt.point(cropX+(x-preview.x)/scaleFactor,cropY+(y-preview.y)/scaleFactor)
    }
    function refreshCursor() {
        pointerCursor.setSystemCursor(sampler.panning?Qt.ClosedHandCursor:mask?Qt.OpenHandCursor:Qt.CrossCursor,
                                      sampler.containsMouse && controls.visible,mask?"hand":"picker",0)
    }
    Connections { target: root.controls; function onVisibleChanged(){root.refreshCursor()} }
    Image {
        id: preview
        objectName: root.mask?"colorSelectionMask":"colorSelectionImage"
        anchors.centerIn: parent
        width: root.cropWidth*root.scaleFactor
        height: root.cropHeight*root.scaleFactor
        sourceSize.width: Math.min(4096,Math.max(1,Math.ceil(width*Screen.devicePixelRatio)))
        sourceSize.height: Math.min(4096,Math.max(1,Math.ceil(height*Screen.devicePixelRatio)))
        fillMode: Image.Stretch
        cache: false
        source: "image://layers/colorSelection/"+(root.mask?"mask/":"image/")+root.document.colorSelectionRevision
                +"/"+root.cropX+"/"+root.cropY+"/"+root.cropWidth+"/"+root.cropHeight
    }
    MouseArea {
        id: sampler
        objectName: root.mask?"colorSelectionMaskSampler":"colorSelectionSampler"
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton|Qt.MiddleButton|Qt.RightButton
        hoverEnabled: true
        preventStealing: true
        cursorShape: panning?Qt.ClosedHandCursor:root.mask?Qt.OpenHandCursor:Qt.CrossCursor
        property bool panning: false
        property point pressPoint
        property real initialCenterX: .5
        property real initialCenterY: .5
        property bool panButton: false
        onContainsMouseChanged: root.refreshCursor()
        onPanningChanged: root.refreshCursor()
        onPressed: function(mouse) {
            root.controls.contentItem.forceActiveFocus()
            pressPoint=Qt.point(mouse.x,mouse.y)
            initialCenterX=(root.cropX+root.cropWidth/2)/root.document.width
            initialCenterY=(root.cropY+root.cropHeight/2)/root.document.height
            panButton=mouse.button!==Qt.LeftButton || root.mask
            panning=panButton
        }
        onPositionChanged: function(mouse) {
            if(!pressed)return
            const dx=mouse.x-pressPoint.x,dy=mouse.y-pressPoint.y
            if(Math.abs(dx)+Math.abs(dy)>4)panning=true
            if(panning) {
                root.controls.previewCenterX=Math.max(0,Math.min(1,initialCenterX-dx/root.scaleFactor/root.document.width))
                root.controls.previewCenterY=Math.max(0,Math.min(1,initialCenterY-dy/root.scaleFactor/root.document.height))
            }
        }
        onReleased: function(mouse) {
            if(!panning && mouse.button===Qt.LeftButton && preview.status===Image.Ready) {
                const point=root.documentPoint(mouse.x,mouse.y)
                root.controls.sampleAt(Math.floor(point.x),Math.floor(point.y),mouse.modifiers)
            }
            panning=false
        }
        onCanceled: panning=false
        onWheel: function(wheel) {
            const point=root.documentPoint(wheel.x,wheel.y)
            const delta=wheel.angleDelta.y!==0?wheel.angleDelta.y/120:wheel.pixelDelta.y/40
            const next=Math.max(.02,Math.min(16,root.scaleFactor*Math.pow(1.2,delta)))
            root.controls.previewCenterX=Math.max(0,Math.min(1,(point.x-(wheel.x-width/2)/next)/root.document.width))
            root.controls.previewCenterY=Math.max(0,Math.min(1,(point.y-(wheel.y-height/2)/next)/root.document.height))
            root.controls.previewZoom=next
            wheel.accepted=true
        }
    }
    Component.onCompleted: if(!mask)controls.previewControl=root
}
