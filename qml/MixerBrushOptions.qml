import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    signal colorRequested()
    signal brushSettingsRequested()
    color: "#252c35"
    implicitHeight: 44
    component PaintToggle: IconButton {
        required property bool clean
        contentItem: Item {
            ToolIcon {anchors.centerIn:parent;width:22;height:22;toolName:"mixerBrush";iconColor:parent.parent.selected?"#c5f2cf":"#cbd4dc"}
            Text {anchors.right:parent.right;anchors.bottom:parent.bottom;text:parent.parent.clean?"×":"+";color:"#edf3f7";font.bold:true;font.pixelSize:14}
        }
        implicitWidth:32;implicitHeight:30
    }
    component PercentControl: Item {
        id: percentControl
        required property string setting
        property alias contentItem: number
        property int value: Math.round(root.document[setting]*100)
        readonly property int from: 0
        readonly property int to: 100
        readonly property int stepSize: 1
        signal valueModified()
        onValueModified: root.document[setting] = value / 100
        implicitWidth:72;implicitHeight:30
        function commit() {
            const value=Number.fromLocaleString(Qt.locale(),number.text.trim())
            if(isFinite(value) && value>=0 && value<=100) root.document[setting]=value/100
        }
        RowLayout {
            anchors.fill:parent;spacing:1
            TextField {
                id:number;Layout.fillWidth:true;implicitHeight:28
                text:String(percentControl.value);selectByMouse:true
                validator:IntValidator {bottom:0;top:100}
                color:enabled?"#e8edf2":"#687581"
                background:Rectangle {color:"#1b2128";radius:3;border.color:number.activeFocus?"#78a589":"#48535f"}
                horizontalAlignment:Text.AlignHCenter
                onEditingFinished:percentControl.commit()
                onAccepted:{percentControl.commit();focus=false}
            }
            Label {text:"%";color:"#aeb8c2"}
            ToolButton {text:"⌄";implicitWidth:18;implicitHeight:28;onClicked:popup.open()
                Popup {
                    id:popup;y:parent.height;x:-110;width:200;height:54
                    background:Rectangle {color:"#303741";radius:4;border.color:"#56616c"}
                    Slider {anchors.fill:parent;from:0;to:100;stepSize:1;value:percentControl.value;onMoved:root.document[percentControl.setting]=value/100}
                }
            }
        }
        Binding {target:number;property:"text";value:String(percentControl.value);when:!number.activeFocus}
    }
    ScrollView {
        anchors.fill: parent;clip:true
        contentWidth:row.implicitWidth+24;contentHeight:40
        ScrollBar.vertical.policy:ScrollBar.AlwaysOff
        RowLayout {
            id:row
            x:12;height:40;spacing:7
            DarkSpinBox {
                objectName:"mixerSize"
                from:1;to:500;value:Math.round(root.document.brushSize);suffix:" px";implicitWidth:108
                onValueModified:root.document.brushSize=value
                ToolTip.text:qsTr("Diamètre du pinceau");ToolTip.visible:hovered
            }
            IconButton {objectName:"mixerBrushSettings";toolName:"mixerBrush";hint:qsTr("Paramètres de la pointe");implicitWidth:30;implicitHeight:30;onClicked:root.brushSettingsRequested()}
            RowLayout {
                spacing:0
                Button {
                    id:loadSwatch
                    objectName:"mixerLoadSwatch"
                    implicitWidth:42;implicitHeight:30
                    Accessible.name:qsTr("Couleur de la charge")
                    onClicked:root.colorRequested()
                    ToolTip.text:qsTr("Choisir une couleur ; Option/Alt-clic sur l’image pour charger les couleurs de la pointe");ToolTip.visible:hovered
                    contentItem:Item {
                        Rectangle {anchors.fill:parent;color:"#d9dde1"}
                        Image {anchors.fill:parent;source:root.document.mixerLoadPreview;fillMode:Image.Stretch;cache:false;visible:root.document.mixerLoaded}
                        Text {anchors.centerIn:parent;visible:!root.document.mixerLoaded;text:"∅";color:"#414a54";font.pixelSize:18}
                        Rectangle {anchors.fill:parent;color:"transparent";border.color:"#a6b1bc"}
                    }
                }
                ToolButton {
                    objectName:"mixerLoadMenuButton"
                    text:"⌄";implicitWidth:22;implicitHeight:30
                    onClicked:loadMenu.popup(this,0,height)
                    ToolTip.text:qsTr("Charger, nettoyer ou prélever une couleur unie");ToolTip.visible:hovered
                }
            }
            PaintToggle {
                objectName:"mixerAutoLoad";clean:false
                selected:root.document.mixerAutoLoad
                hint:qsTr("Charger le pinceau après chaque trait")
                onClicked:root.document.mixerAutoLoad=!root.document.mixerAutoLoad
            }
            PaintToggle {
                objectName:"mixerAutoClean";clean:true
                selected:root.document.mixerAutoClean
                hint:qsTr("Nettoyer le pinceau après chaque trait")
                onClicked:root.document.mixerAutoClean=!root.document.mixerAutoClean
            }
            ComboBox {
                id:presets
                objectName:"mixerPreset"
                model:root.document.mixerPresets.map(function(preset) {
                    return {id:preset.id,label:qsTranslate("Document",preset.source)}
                })
                textRole:"label";valueRole:"id"
                currentIndex:(count,indexOfValue(root.document.mixerPreset))
                implicitWidth:210;implicitHeight:30
                onActivated:root.document.applyMixerPreset(currentValue)
                ToolTip.text:qsTr("Combinaisons d’humidité, de charge et de mélange ; le débit reste indépendant");ToolTip.visible:hovered
            }
            Repeater {
                model:[
                    {key:"mixerWet",label:qsTr("Humidité"),hint:qsTr("Prélèvement des couleurs du canevas ; plus humide, les couleurs sont entraînées plus longtemps")},
                    {key:"mixerLoad",label:qsTr("Charge"),hint:qsTr("Quantité de peinture disponible ; une faible charge s’épuise plus vite")},
                    {key:"mixerMix",label:qsTr("Mélange"),hint:qsTr("0 % : charge du pinceau ; 100 % : peinture du canevas. Inactif à sec")},
                    {key:"mixerFlow",label:qsTr("Débit"),hint:qsTr("Quantité déposée par empreinte, indépendante du préréglage")}
                ]
                delegate:RowLayout {
                    required property var modelData
                    spacing:4
                    ScrubLabel {control:percent;text:modelData.label;color:"#cbd4dc"}
                    PercentControl {
                        id:percent
                        objectName:modelData.key;setting:modelData.key
                        enabled:modelData.key!=="mixerMix" || root.document.mixerWet>0
                        ToolTip.text:modelData.hint
                        ToolTip.visible:percentHover.hovered
                        HoverHandler {id:percentHover}
                    }
                }
            }
            IconButton {objectName:"mixerAirbrush";glyph:"◌";hint:qsTr("Aérographe : accumuler la peinture sans déplacer la souris");selected:root.document.mixerAirbrush;implicitWidth:30;implicitHeight:30;onClicked:root.document.mixerAirbrush=!root.document.mixerAirbrush}
            Label {text:"◐";color:"#cbd4dc"}
            PercentControl {objectName:"mixerSmoothing";setting:"brushSmoothing";ToolTip.text:qsTr("Lissage du trait");ToolTip.visible:smoothHover.hovered;HoverHandler{id:smoothHover}}
            ToolButton {text:"⚙";implicitWidth:30;implicitHeight:30;onClicked:settings.popup(this,0,height);ToolTip.text:qsTr("Options du pinceau mélangeur");ToolTip.visible:hovered}
            ScrubLabel { control: mixerAngle;text:qsTr("Angle");color:"#cbd4dc"}
            DarkSpinBox { id: mixerAngle;objectName:"mixerAngle";from:-180;to:180;value:Math.round(root.document.brushAngle);suffix:"°";implicitWidth:105;onValueModified:root.document.brushAngle=value}
            CheckBox {
                objectName:"mixerSampleAllLayers"
                text:qsTr("Tous les calques");enabled:!root.document.editingMask
                checked:root.document.mixerSampleAllLayers
                onToggled:root.document.mixerSampleAllLayers=checked
            }
        }
    }
    DarkMenu {
        id:loadMenu
        objectName:"mixerLoadMenu"
        DarkMenuItem {objectName:"mixerLoadAction";text:qsTr("Charger le pinceau");onTriggered:root.document.loadMixerBrush()}
        DarkMenuItem {objectName:"mixerCleanAction";text:qsTr("Nettoyer le pinceau");onTriggered:root.document.cleanMixerBrush()}
        MenuSeparator {}
        DarkMenuItem {objectName:"mixerSolidOnly";text:qsTr("Charger les couleurs unies seulement");checkable:true;checked:root.document.mixerSolidOnly;onTriggered:root.document.mixerSolidOnly=checked}
    }
    DarkMenu {
        id:settings
        DarkMenuItem {text:qsTr("Paramètres de la pointe…");onTriggered:root.brushSettingsRequested()}
        MenuSeparator {}
        DarkMenuItem {text:qsTr("Pointes à soies et simulation physique (NYI)");enabled:false}
        DarkMenuItem {text:qsTr("Pression et inclinaison du stylet (NYI)");enabled:false}
    }
}
