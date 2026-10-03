import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property var histogram: []
    property var parameters: ({})
    property real inputBlack: 0
    property real inputWhite: 255
    property real gamma: 1
    property real outputBlack: 0
    property real outputWhite: 255
    property string activeKey: ""
    signal editStarted()
    signal previewed(string key, real value)
    signal committed(string key, real value)
    spacing: 7

    function sync() {
        if(activeKey.length)return
        inputBlack=Number(parameters.inputBlack===undefined?0:parameters.inputBlack)
        inputWhite=Number(parameters.inputWhite===undefined?255:parameters.inputWhite)
        gamma=Number(parameters.gamma===undefined?1:parameters.gamma)
        outputBlack=Number(parameters.outputBlack===undefined?0:parameters.outputBlack)
        outputWhite=Number(parameters.outputWhite===undefined?255:parameters.outputWhite)
        inputCanvas.requestPaint();outputCanvas.requestPaint()
    }
    onParametersChanged: sync()
    onHistogramChanged: inputCanvas.requestPaint()
    Component.onCompleted: sync()
    Label { text: qsTr("Niveaux d’entrée"); color: "#c4cdd5" }
    Canvas {
        id: inputCanvas
        objectName: "levelsEditor"
        Layout.fillWidth: true; Layout.preferredHeight: 205
        property real leftEdge: 14
        property real rightEdge: width-14
        property real topEdge: 8
        property real bottomEdge: height-28
        function px(value) { return leftEdge+value/255*(rightEdge-leftEdge) }
        function valueX(pixel) { return Math.max(0,Math.min(255,(pixel-leftEdge)/(rightEdge-leftEdge)*255)) }
        function gammaT(value) { return value>=1?.5-(value-1)/4:.5+(1-value)/1.8 }
        function gammaValue(t) { return t<=.5?1+(.5-t)*4:1-(t-.5)*1.8 }
        function gammaX() { return px(root.inputBlack+(root.inputWhite-root.inputBlack)*gammaT(root.gamma)) }
        function triangle(c,x,color) { c.beginPath();c.moveTo(x,bottomEdge+2);c.lineTo(x-7,bottomEdge+15);c.lineTo(x+7,bottomEdge+15);c.closePath();c.fillStyle=color;c.fill();c.strokeStyle="#dbe2e8";c.stroke() }
        onWidthChanged: requestPaint()
        onPaint: {
            const c=getContext("2d");c.reset();c.fillStyle="#151a20";c.fillRect(0,0,width,height)
            c.strokeStyle="#303943";for(let i=0;i<=4;i++){const x=leftEdge+(rightEdge-leftEdge)*i/4;c.beginPath();c.moveTo(x,topEdge);c.lineTo(x,bottomEdge);c.stroke()}
            if(root.histogram && root.histogram.length===256){c.beginPath();c.moveTo(leftEdge,bottomEdge);for(let b=0;b<256;b++)c.lineTo(px(b),bottomEdge-Number(root.histogram[b])*(bottomEdge-topEdge)*.92);c.lineTo(rightEdge,bottomEdge);c.closePath();c.fillStyle="rgba(206,216,225,.42)";c.fill()}
            c.strokeStyle="#66727d";c.strokeRect(leftEdge,topEdge,rightEdge-leftEdge,bottomEdge-topEdge)
            triangle(c,px(root.inputBlack),"#07090b");triangle(c,gammaX(),"#858b91");triangle(c,px(root.inputWhite),"#f4f5f6")
        }
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            onPressed: function(mouse) {
                const positions=[inputCanvas.px(root.inputBlack),inputCanvas.gammaX(),inputCanvas.px(root.inputWhite)]
                let nearest=0;if(Math.abs(mouse.x-positions[1])<Math.abs(mouse.x-positions[nearest]))nearest=1;if(Math.abs(mouse.x-positions[2])<Math.abs(mouse.x-positions[nearest]))nearest=2
                root.activeKey=nearest===0?"inputBlack":nearest===1?"gamma":"inputWhite";root.editStarted();updateValue(mouse.x)
            }
            function updateValue(x) {
                const value=inputCanvas.valueX(x)
                if(root.activeKey==="inputBlack")root.inputBlack=Math.min(root.inputWhite-1,value)
                else if(root.activeKey==="inputWhite")root.inputWhite=Math.max(root.inputBlack+1,value)
                else {const t=Math.max(0,Math.min(1,(value-root.inputBlack)/Math.max(1,root.inputWhite-root.inputBlack)));root.gamma=Math.max(.1,Math.min(3,inputCanvas.gammaValue(t)))}
                const emitted=root.activeKey==="inputBlack"?root.inputBlack:root.activeKey==="inputWhite"?root.inputWhite:root.gamma
                root.previewed(root.activeKey,emitted);inputCanvas.requestPaint()
            }
            onPositionChanged: function(mouse) { if(pressed)updateValue(mouse.x) }
            onReleased: {const key=root.activeKey;const value=key==="inputBlack"?root.inputBlack:key==="inputWhite"?root.inputWhite:root.gamma;root.activeKey="";root.committed(key,value)}
            onCanceled: root.activeKey=""
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: Math.round(root.inputBlack); color: "#9da8b2" }
        Item { Layout.fillWidth: true }
        Label { text: Number(root.gamma).toFixed(2); color: "#9da8b2" }
        Item { Layout.fillWidth: true }
        Label { text: Math.round(root.inputWhite); color: "#9da8b2" }
    }
    Label { text: qsTr("Niveaux de sortie"); color: "#c4cdd5" }
    Canvas {
        id: outputCanvas
        Layout.fillWidth: true; Layout.preferredHeight: 48
        property real leftEdge: 14
        property real rightEdge: width-14
        function px(value) { return leftEdge+value/255*(rightEdge-leftEdge) }
        function valueX(pixel) { return Math.max(0,Math.min(255,(pixel-leftEdge)/(rightEdge-leftEdge)*255)) }
        onWidthChanged: requestPaint()
        onPaint: {
            const c=getContext("2d");c.reset();const gradient=c.createLinearGradient(leftEdge,0,rightEdge,0);gradient.addColorStop(0,"#000");gradient.addColorStop(1,"#fff");c.fillStyle=gradient;c.fillRect(leftEdge,5,rightEdge-leftEdge,14)
            function marker(x,color){c.beginPath();c.moveTo(x,21);c.lineTo(x-7,35);c.lineTo(x+7,35);c.closePath();c.fillStyle=color;c.fill();c.strokeStyle="#dbe2e8";c.stroke()}
            marker(px(root.outputBlack),"#07090b");marker(px(root.outputWhite),"#f4f5f6")
        }
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            onPressed: function(mouse){root.activeKey=Math.abs(mouse.x-outputCanvas.px(root.outputBlack))<Math.abs(mouse.x-outputCanvas.px(root.outputWhite))?"outputBlack":"outputWhite";root.editStarted();updateValue(mouse.x)}
            function updateValue(x){const value=outputCanvas.valueX(x);if(root.activeKey==="outputBlack")root.outputBlack=Math.min(root.outputWhite,value);else root.outputWhite=Math.max(root.outputBlack,value);const emitted=root.activeKey==="outputBlack"?root.outputBlack:root.outputWhite;root.previewed(root.activeKey,emitted);outputCanvas.requestPaint()}
            onPositionChanged:function(mouse){if(pressed)updateValue(mouse.x)}
            onReleased:{const key=root.activeKey,value=key==="outputBlack"?root.outputBlack:root.outputWhite;root.activeKey="";root.committed(key,value)}
            onCanceled:root.activeKey=""
        }
    }
    RowLayout { Layout.fillWidth: true; Label { text: Math.round(root.outputBlack); color: "#9da8b2" } Item { Layout.fillWidth: true } Label { text: Math.round(root.outputWhite); color: "#9da8b2" } }
}
