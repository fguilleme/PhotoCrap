import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    readonly property bool marquee: document.tool === "select" || document.tool === "ellipticalSelect"
    readonly property bool singlePixel: document.tool === "singleRowSelect" || document.tool === "singleColumnSelect"
    readonly property bool canAntialias: document.tool === "ellipticalSelect" || document.tool === "lasso" || document.tool === "polygonalLasso"
    signal editCommitted()
    implicitHeight: 44
    color: "#252c35"
    component ModeButton: ToolButton {
        id: button
        required property int mode
        required property string help
        objectName: "selectionMode_" + mode
        implicitWidth: 30; implicitHeight: 30
        readonly property bool selected: root.document.selectionMode === mode
        ToolTip.text: help; ToolTip.visible: hovered
        Accessible.name: help
        onClicked: {root.document.selectionMode=mode;root.editCommitted()}
        background: Rectangle {color: button.selected ? "#d0d7de" : button.hovered ? "#3e4853" : "transparent";radius:4}
        contentItem: Canvas {
            id: icon
            implicitWidth: 24; implicitHeight: 24
            onPaint: {
                const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
                c.fillStyle=button.selected ? "#303741" : "#e5eaf0"
                c.strokeStyle=c.fillStyle;c.lineWidth=1.5
                const x=(width-22)/2,y=(height-22)/2;c.translate(x,y)
                if(button.mode===0)c.fillRect(3,3,16,16)
                else if(button.mode===1){c.fillRect(2,2,12,12);c.fillRect(8,8,12,12)}
                else if(button.mode===2){c.fillRect(2,2,12,12);c.clearRect(8,8,12,12);c.strokeRect(8,8,12,12)}
                else {c.strokeRect(2,2,12,12);c.strokeRect(8,8,12,12);c.fillRect(8,8,6,6)}
            }
            Connections {target:button;function onSelectedChanged(){icon.requestPaint()}}
        }
    }
    ScrollView {
        anchors.fill: parent; clip: true
        contentWidth: row.implicitWidth+24; contentHeight: 40
        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
        RowLayout {
            id: row
            x: 12; height: 40; spacing: 8
            RowLayout {
                spacing: 2
                ModeButton {mode:0;help:qsTr("Nouvelle sélection")}
                ModeButton {mode:1;help:qsTr("Ajouter à la sélection")}
                ModeButton {mode:2;help:qsTr("Soustraire de la sélection")}
                ModeButton {mode:3;help:qsTr("Intersection avec la sélection")}
            }
            Rectangle {Layout.preferredWidth:1;Layout.preferredHeight:24;color:"#48535f"}
            ScrubLabel { control: selectionFeather;text:qsTr("Contour progressif");color:"#cbd4dc"}
            DarkSpinBox { id: selectionFeather;
                objectName: "selectionFeather"
                from:0;to:1000;value:root.document.selectionFeather;suffix:" px";implicitWidth:128
                onValueModified:root.document.selectionFeather=value
                ToolTip.text:qsTr("Adoucir la prochaine sélection, en pixels du document");ToolTip.visible:hovered
            }
            CheckBox {
                objectName:"selectionAntialias"
                text:qsTr("Anticrénelage");enabled:root.canAntialias
                checked:root.document.selectionAntialias
                onToggled:root.document.selectionAntialias=checked
                ToolTip.text:qsTr("Lisser les bords courbes et obliques ; sans effet sur les rectangles et les rangées/colonnes");ToolTip.visible:hovered
            }
            Rectangle {visible:root.marquee;Layout.preferredWidth:1;Layout.preferredHeight:24;color:"#48535f"}
            Label {visible:root.marquee;text:qsTr("Style");color:"#cbd4dc"}
            ComboBox {
                objectName:"marqueeStyle"
                visible:root.marquee;implicitWidth:150;implicitHeight:30
                model:[qsTr("Normal"),qsTr("Rapport fixe"),qsTr("Taille fixe")]
                currentIndex:root.document.marqueeStyle
                onActivated:root.document.marqueeStyle=currentIndex
            }
            ScrubLabel { control: widthNumber;visible:root.marquee;text:qsTr("Largeur");color:root.document.marqueeStyle ? "#cbd4dc" : "#717c87"}
            DarkSpinBox {
                id: widthNumber
                objectName:"marqueeWidth"
                visible:root.marquee;enabled:root.document.marqueeStyle>0
                from:1;to:1638400;value:Math.round(root.document.marqueeWidth*100);implicitWidth:132
                suffix:root.document.marqueeStyle===2?" px":""
                textFromValue:function(value,locale){return Number(value/100).toLocaleString(locale,"f",2)}
                valueFromText:function(text,locale){return Math.round(Number.fromLocaleString(locale,text.trim())*100)}
                validator: DoubleValidator {bottom:.01;top:16384;decimals:2;locale:widthNumber.locale.name}
                onValueModified:root.document.marqueeWidth=value/100
            }
            ToolButton {
                objectName:"swapMarqueeDimensions"
                visible:root.marquee;enabled:root.document.marqueeStyle>0
                text:"⇄";implicitWidth:28;implicitHeight:30
                onClicked:root.document.swapMarqueeDimensions()
                ToolTip.text:qsTr("Permuter largeur et hauteur");ToolTip.visible:hovered
            }
            ScrubLabel { control: heightNumber;visible:root.marquee;text:qsTr("Hauteur");color:root.document.marqueeStyle ? "#cbd4dc" : "#717c87"}
            DarkSpinBox {
                id: heightNumber
                objectName:"marqueeHeight"
                visible:root.marquee;enabled:root.document.marqueeStyle>0
                from:1;to:1638400;value:Math.round(root.document.marqueeHeight*100);implicitWidth:132
                suffix:root.document.marqueeStyle===2?" px":""
                textFromValue:function(value,locale){return Number(value/100).toLocaleString(locale,"f",2)}
                valueFromText:function(text,locale){return Math.round(Number.fromLocaleString(locale,text.trim())*100)}
                validator: DoubleValidator {bottom:.01;top:16384;decimals:2;locale:heightNumber.locale.name}
                onValueModified:root.document.marqueeHeight=value/100
            }
            Button {
                objectName:"selectionRefineOptions"
                text:qsTr("Sélectionner et masquer… (NYI)");implicitHeight:30;enabled:false
            }
        }
    }
}
