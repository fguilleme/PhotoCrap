import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    signal editCommitted()
    color: "#20262e"
    implicitHeight: 44
    enabled: !document.cropBusy
    readonly property bool perspective: document.tool === "perspectiveCrop"
    property bool showGrid: true
    function ratioLabel(ratio) {
        if(ratio===0) return qsTr("Libre")
        for(const pair of [[1,1],[4,5],[5,7],[2,3],[16,9]]) {
            if(Math.abs(ratio-pair[0]/pair[1])<.000001) return pair[0]+" : "+pair[1]
            if(Math.abs(ratio-pair[1]/pair[0])<.000001) return pair[1]+" : "+pair[0]
        }
        return ratio.toFixed(3)+" : 1"
    }
    readonly property var ratios: [0, -1, 1, 4/5, 5/7, 2/3, 16/9]
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18; anchors.rightMargin: 18
        spacing: 8
        ToolIcon { toolName: root.document.tool; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
        ComboBox {
            id: cropPreset
            objectName: "cropRatioPreset"
            visible: !root.perspective
            implicitWidth: 160; implicitHeight: 30
            displayText: root.ratioLabel(root.document.cropRatio)
            model: [qsTr("Libre"), qsTr("Ratio d’origine"), "1 : 1", "4 : 5", "5 : 7", "2 : 3", "16 : 9"]
            onActivated: root.document.cropRatio = index === 1 ? root.document.width/root.document.height : root.ratios[index]
            Connections { target: root.document; function onCropChanged() { if(root.document.cropRatio===0) cropPreset.currentIndex=0 } }
        }
        Button {
            text: qsTr("Permuter")
            visible: !root.perspective
            enabled: root.document.cropRatio > 0
            implicitHeight: 30
            onClicked: root.document.cropRatio = 1/root.document.cropRatio
        }
        Repeater {
            model: [qsTr("L"), qsTr("H")]
            delegate: RowLayout {
                required property string modelData
                required property int index
                spacing: 4
                Label { text: modelData + ":"; color: "#b4beca" }
                TextField {
                    objectName: index===0 ? "cropOutputWidth" : "cropOutputHeight"
                    implicitWidth: 78; implicitHeight: 30
                    placeholderText: qsTr("Auto")
                    text: (index===0 ? root.document.cropOutputSize.width : root.document.cropOutputSize.height) || ""
                    validator: IntValidator { bottom: 1; top: 32768 }
                    selectByMouse: true
                    onAccepted: root.editCommitted()
                    onEditingFinished: {
                        const size=root.document.cropOutputSize
                        const value=text.length ? Number(text) : 0
                        root.document.cropOutputSize= index===0 ? Qt.size(value,size.height) : Qt.size(size.width,value)
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Dimensions de sortie en pixels ; vide = automatique")
                }
            }
        }
        CheckBox { text: qsTr("Grille"); checked: root.showGrid; onToggled: root.showGrid=checked }
        Button { objectName: "resetCropButton"; text: qsTr("Réinitialiser"); implicitHeight: 30; onClicked: root.document.resetCrop() }
        Item { Layout.fillWidth: true }
        Button { objectName: "cancelCropButton"; text: qsTr("Annuler"); enabled: root.document.cropPoints.length>0; implicitHeight: 30; onClicked: root.document.cancelCrop() }
        Button { objectName: "applyCropButton"; text: qsTr("Recadrer"); enabled: root.document.hasCrop; implicitHeight: 30; onClicked: root.document.applyCrop() }
    }
}
