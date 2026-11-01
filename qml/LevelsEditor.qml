import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "levelsEditorRoot"
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
    }
    onParametersChanged: sync()
    Component.onCompleted: sync()
    Label { text: qsTr("Niveaux d’entrée"); color: "#c4cdd5" }
    LevelsChart {
        id: inputCanvas
        objectName: "levelsEditor"
        Layout.fillWidth: true; Layout.preferredHeight: 205
        histogram: root.histogram
        black: root.inputBlack; white: root.inputWhite; gamma: root.gamma
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
                root.previewed(root.activeKey,emitted)
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
    LevelsChart {
        id: outputCanvas
        objectName: "levelsOutputChart"
        Layout.fillWidth: true; Layout.preferredHeight: 48
        output: true
        black: root.outputBlack; white: root.outputWhite
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            onPressed: function(mouse){root.activeKey=Math.abs(mouse.x-outputCanvas.px(root.outputBlack))<Math.abs(mouse.x-outputCanvas.px(root.outputWhite))?"outputBlack":"outputWhite";root.editStarted();updateValue(mouse.x)}
            function updateValue(x){const value=outputCanvas.valueX(x);if(root.activeKey==="outputBlack")root.outputBlack=Math.min(root.outputWhite,value);else root.outputWhite=Math.max(root.outputBlack,value);const emitted=root.activeKey==="outputBlack"?root.outputBlack:root.outputWhite;root.previewed(root.activeKey,emitted)}
            onPositionChanged:function(mouse){if(pressed)updateValue(mouse.x)}
            onReleased:{const key=root.activeKey,value=key==="outputBlack"?root.outputBlack:root.outputWhite;root.activeKey="";root.committed(key,value)}
            onCanceled:root.activeKey=""
        }
    }
    RowLayout { Layout.fillWidth: true; Label { text: Math.round(root.outputBlack); color: "#9da8b2" } Item { Layout.fillWidth: true } Label { text: Math.round(root.outputWhite); color: "#9da8b2" } }
}
