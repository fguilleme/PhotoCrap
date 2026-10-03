import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string shadowColor: "#101828"
    property string highlightColor: "#f5d9a8"
    property int activeHandle: 0
    property bool dragging: false
    property bool syncing: false
    property real shadowHue: 220
    property real shadowSaturation: .68
    property real shadowValue: .16
    property real highlightHue: 37
    property real highlightSaturation: .31
    property real highlightValue: .96

    signal editStarted()
    signal previewed(string key, string value)
    signal committed(string key, string value)

    spacing: 9

    function componentHex(value) {
        return Math.max(0,Math.min(255,Math.round(value*255))).toString(16).padStart(2,"0")
    }
    function hsvHex(h,s,v) {
        const sector=((h%360)+360)%360/60
        const chroma=v*s
        const x=chroma*(1-Math.abs(sector%2-1))
        let r=0,g=0,b=0
        if(sector<1){r=chroma;g=x}else if(sector<2){r=x;g=chroma}else if(sector<3){g=chroma;b=x}
        else if(sector<4){g=x;b=chroma}else if(sector<5){r=x;b=chroma}else{r=chroma;b=x}
        const m=v-chroma
        return "#"+componentHex(r+m)+componentHex(g+m)+componentHex(b+m)
    }
    function hexHsv(value) {
        const text=String(value || "#000000").replace("#","")
        const expanded=text.length===3 ? text[0]+text[0]+text[1]+text[1]+text[2]+text[2] : text
        const r=parseInt(expanded.slice(0,2),16)/255
        const g=parseInt(expanded.slice(2,4),16)/255
        const b=parseInt(expanded.slice(4,6),16)/255
        const maximum=Math.max(r,g,b),minimum=Math.min(r,g,b),delta=maximum-minimum
        let hue=0
        if(delta>0) {
            if(maximum===r)hue=60*(((g-b)/delta)%6)
            else if(maximum===g)hue=60*((b-r)/delta+2)
            else hue=60*((r-g)/delta+4)
        }
        if(hue<0)hue+=360
        return {h:hue,s:maximum===0?0:delta/maximum,v:maximum}
    }
    function syncColors() {
        if(dragging)return
        syncing=true
        const shadow=hexHsv(shadowColor),highlight=hexHsv(highlightColor)
        shadowHue=shadow.h;shadowSaturation=shadow.s;shadowValue=shadow.v
        highlightHue=highlight.h;highlightSaturation=highlight.s;highlightValue=highlight.v
        syncing=false
        wheel.requestPaint()
        valueOverlay.requestPaint()
    }
    function selectedKey() { return activeHandle===0 ? "shadowColor" : "highlightColor" }
    function selectedColor() {
        return activeHandle===0 ? hsvHex(shadowHue,shadowSaturation,shadowValue)
                                : hsvHex(highlightHue,highlightSaturation,highlightValue)
    }
    function selectedValue() { return activeHandle===0 ? shadowValue : highlightValue }
    function setSelectedValue(value,preview) {
        if(activeHandle===0)shadowValue=value
        else highlightValue=value
        valueOverlay.requestPaint()
        if(preview)root.previewed(selectedKey(),selectedColor())
    }
    function setSelectedPosition(hue,saturation,preview) {
        if(activeHandle===0){shadowHue=hue;shadowSaturation=saturation}
        else{highlightHue=hue;highlightSaturation=saturation}
        if(preview)root.previewed(selectedKey(),selectedColor())
    }
    function selectHandle(index) {
        activeHandle=index
        valueOverlay.requestPaint()
    }

    onShadowColorChanged: syncColors()
    onHighlightColorChanged: syncColors()
    Component.onCompleted: syncColors()

    Label {
        text: qsTr("Couleurs du dégradé")
        color: "#c4cdd5"
        Layout.fillWidth: true
    }

    Item {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 230
        Layout.preferredHeight: 230

        Canvas {
            id: wheel
            objectName: "gradientMapWheel"
            anchors.fill: parent
            readonly property real center: width/2
            readonly property real radius: Math.min(width,height)/2-9
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c=getContext("2d")
                c.reset();c.clearRect(0,0,width,height)
                for(let degree=0;degree<360;degree+=2) {
                    c.beginPath();c.moveTo(center,center)
                    c.arc(center,center,radius,-(degree+2)*Math.PI/180,-degree*Math.PI/180)
                    c.closePath();c.fillStyle=root.hsvHex(degree,1,1);c.fill()
                }
                const white=c.createRadialGradient(center,center,0,center,center,radius)
                white.addColorStop(0,"rgba(255,255,255,1)");white.addColorStop(1,"rgba(255,255,255,0)")
                c.beginPath();c.arc(center,center,radius,0,Math.PI*2);c.fillStyle=white;c.fill()
                c.strokeStyle="#687480";c.lineWidth=1;c.stroke()
            }
        }

        Canvas {
            id: valueOverlay
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c=getContext("2d")
                c.reset();c.clearRect(0,0,width,height)
                c.beginPath();c.arc(wheel.center,wheel.center,wheel.radius,0,Math.PI*2)
                c.fillStyle="rgba(0,0,0,"+(1-root.selectedValue())+")";c.fill()
            }
        }

        Rectangle {
            width: 16;height:16;radius:8
            x: wheel.center+Math.cos(root.shadowHue*Math.PI/180)*root.shadowSaturation*wheel.radius-width/2
            y: wheel.center-Math.sin(root.shadowHue*Math.PI/180)*root.shadowSaturation*wheel.radius-height/2
            color: root.hsvHex(root.shadowHue,root.shadowSaturation,root.shadowValue)
            border.width: root.activeHandle===0?3:2
            border.color: root.activeHandle===0?"#ffffff":"#11161b"
        }
        Rectangle {
            width: 16;height:16;radius:8
            x: wheel.center+Math.cos(root.highlightHue*Math.PI/180)*root.highlightSaturation*wheel.radius-width/2
            y: wheel.center-Math.sin(root.highlightHue*Math.PI/180)*root.highlightSaturation*wheel.radius-height/2
            color: root.hsvHex(root.highlightHue,root.highlightSaturation,root.highlightValue)
            border.width: root.activeHandle===1?3:2
            border.color: root.activeHandle===1?"#ffffff":"#11161b"
        }
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            cursorShape: Qt.CrossCursor
            function update(mouse,preview) {
                const dx=mouse.x-wheel.center,dy=wheel.center-mouse.y
                const saturation=Math.min(1,Math.sqrt(dx*dx+dy*dy)/wheel.radius)
                let hue=Math.atan2(dy,dx)*180/Math.PI
                if(hue<0)hue+=360
                root.setSelectedPosition(hue,saturation,preview)
            }
            onPressed: function(mouse) {
                const shadowX=wheel.center+Math.cos(root.shadowHue*Math.PI/180)*root.shadowSaturation*wheel.radius
                const shadowY=wheel.center-Math.sin(root.shadowHue*Math.PI/180)*root.shadowSaturation*wheel.radius
                const highlightX=wheel.center+Math.cos(root.highlightHue*Math.PI/180)*root.highlightSaturation*wheel.radius
                const highlightY=wheel.center-Math.sin(root.highlightHue*Math.PI/180)*root.highlightSaturation*wheel.radius
                const shadowDistance=Math.hypot(mouse.x-shadowX,mouse.y-shadowY)
                const highlightDistance=Math.hypot(mouse.x-highlightX,mouse.y-highlightY)
                if(Math.min(shadowDistance,highlightDistance)<22)
                    root.selectHandle(shadowDistance<=highlightDistance?0:1)
                root.dragging=true;root.editStarted();update(mouse,true)
            }
            onPositionChanged: function(mouse) { if(pressed)update(mouse,true) }
            onReleased: {
                root.dragging=false
                root.committed(root.selectedKey(),root.selectedColor())
            }
            onCanceled: root.dragging=false
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Luminosité"); color: "#c4cdd5" }
        DarkSlider {
            id: brightness
            objectName: "gradientMapBrightness"
            Layout.fillWidth: true
            from: 0;to: 100;stepSize: 1
            value: root.selectedValue()*100
            onPressedChanged: {
                if(pressed)root.editStarted()
                else root.committed(root.selectedKey(),root.selectedColor())
            }
            onMoved: root.setSelectedValue(value/100,true)
        }
        Label { text: Math.round(brightness.value)+" %"; color: "#91a0ad"; Layout.preferredWidth: 45 }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 30
        radius: 5
        border.color: "#59636e"
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: root.shadowColor }
            GradientStop { position: 1; color: root.highlightColor }
        }
    }
}
