import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Popup {
    id: root
    required property var textEditor
    required property var document
    width: 440
    height: Math.min(690,parent.height-140)
    padding: 14
    closePolicy: Popup.CloseOnEscape
    background: Rectangle {color:"#252d36";border.color:"#53606e";radius:6}
    function apply(values){textEditor.apply(values)}
    contentItem: ScrollView {
        clip:true
        ColumnLayout {
            width:root.availableWidth
            spacing:9
            RowLayout {
                Layout.fillWidth:true
                Label {text:qsTr("Caractère et paragraphe");color:"white";font.bold:true;Layout.fillWidth:true}
                Button {text:"×";flat:true;onClicked:root.close()}
            }
            Label {text:qsTr("La mise en forme s’applique à la sélection, sinon à tout le texte.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:"#b9c6d3"}
            ComboBox {
                objectName:"textFontFamily"
                Layout.fillWidth:true
                editable:true
                model:root.textEditor.formatter.fontFamilies()
                currentIndex:model.indexOf(root.textEditor.style.family || "sans-serif")
                editText:root.textEditor.style.family || "sans-serif"
                onActivated:root.apply({family:currentText})
                onAccepted:root.apply({family:editText})
            }
            ComboBox {
                objectName:"textFontStyle"
                Layout.fillWidth:true
                model:root.textEditor.formatter.fontStyles(root.textEditor.style.family || "sans-serif")
                currentIndex:Math.max(0,model.indexOf(root.textEditor.style.style || ""))
                onActivated:root.apply({style:currentText})
            }
            Label {
                visible:!root.textEditor.formatter.fontAvailable(root.textEditor.style.family || "sans-serif")
                text:qsTr("Police absente : un remplacement système est utilisé.")
                color:"#ecc388";wrapMode:Text.WordWrap;Layout.fillWidth:true
            }
            RowLayout {
                Label {text:qsTr("Taille");color:"#cbd4dc"}
                DarkSpinBox {objectName:"textFontSize";from:1;to:1024;suffix:" px";value:Math.round(root.textEditor.style.size || 64);onValueModified:root.apply({size:value})}
                Repeater {
                    model:[{label:"B",key:"bold"},{label:"I",key:"italic"},{label:"U",key:"underline"},{label:"S̶",key:"strike"}]
                    Button {
                        required property var modelData
                        text:modelData.label;checkable:true;checked:Boolean(root.textEditor.style[modelData.key])
                        focusPolicy:Qt.NoFocus;implicitWidth:34
                        onClicked:{const v={};v[modelData.key]=checked;root.apply(v)}
                    }
                }
            }
            Button {text:qsTr("Couleur du texte…");Layout.fillWidth:true;onClicked:{textColor.selectedColor=root.textEditor.style.color || root.textEditor.state.color;textColor.open()}}
            RowLayout {
                Label {text:qsTr("Casse");color:"#cbd4dc"}
                ComboBox {model:[qsTr("Normale"),qsTr("Capitales"),qsTr("Petites capitales")];currentIndex:root.textEditor.style.caps || 0;onActivated:root.apply({caps:currentIndex})}
                ComboBox {model:[qsTr("Normal"),qsTr("Exposant"),qsTr("Indice")];currentIndex:root.textEditor.style.script || 0;onActivated:root.apply({script:currentIndex})}
            }
            Repeater {
                model:[{label:qsTr("Approche (1/1000 em)"),key:"tracking",min:-1000,max:1000},
                       {label:qsTr("Échelle horizontale (%)"),key:"stretch",min:1,max:4000},
                       {label:qsTr("Décalage de ligne (%)"),key:"baseline",min:-1000,max:1000},
                       {label:qsTr("Interligne (0 = auto)"),key:"leading",min:0,max:4096}]
                RowLayout {
                    required property var modelData
                    Layout.fillWidth:true
                    Label {text:modelData.label;color:"#cbd4dc";Layout.fillWidth:true}
                    DarkSpinBox {from:modelData.min;to:modelData.max;value:Math.round(root.textEditor.style[modelData.key] || (modelData.key==="stretch"?100:0));onValueModified:{const v={};v[modelData.key]=value;root.apply(v)}}
                }
            }
            CheckBox {text:qsTr("Crénage métrique");checked:root.textEditor.style.kerning!==false;onToggled:root.apply({kerning:checked})}
            Label {text:qsTr("Paragraphe");font.bold:true;color:"white"}
            ComboBox {
                Layout.fillWidth:true
                model:[qsTr("Aligné à gauche"),qsTr("Centré"),qsTr("Aligné à droite"),qsTr("Justifié")]
                currentIndex:root.textEditor.style.alignment & Qt.AlignJustify ? 3 : root.textEditor.style.alignment & Qt.AlignRight ? 2 : root.textEditor.style.alignment & Qt.AlignHCenter ? 1 : 0
                onActivated:root.apply({alignment:[Qt.AlignLeft,Qt.AlignHCenter,Qt.AlignRight,Qt.AlignJustify][currentIndex]})
            }
            CheckBox {text:qsTr("Direction de droite à gauche");checked:Boolean(root.textEditor.style.rtl);onToggled:root.apply({rtl:checked})}
            Repeater {
                model:[{label:qsTr("Retrait gauche"),key:"left"},{label:qsTr("Retrait droit"),key:"right"},
                       {label:qsTr("Première ligne"),key:"indent"},{label:qsTr("Espace avant"),key:"before"},{label:qsTr("Espace après"),key:"after"}]
                RowLayout {
                    required property var modelData
                    Layout.fillWidth:true
                    Label {text:modelData.label;color:"#cbd4dc";Layout.fillWidth:true}
                    DarkSpinBox {from:modelData.key==="indent"?-4096:0;to:4096;suffix:" px";value:Math.round(root.textEditor.style[modelData.key] || 0);onValueModified:{const v={};v[modelData.key]=value;root.apply(v)}}
                }
            }
            CheckBox {text:qsTr("Bloc de paragraphe");checked:Boolean(root.textEditor.state.paragraph);onToggled:root.document.setTextBox(checked,root.textEditor.state.width || 320,root.textEditor.state.height || 180)}
            RowLayout {
                Label {text:qsTr("L");color:"#cbd4dc"}
                DarkSpinBox {from:16;to:32768;suffix:" px";value:Math.round(root.textEditor.state.width || 320);onValueModified:root.document.setTextBox(true,value,root.textEditor.state.height || 180)}
                Label {text:qsTr("H");color:"#cbd4dc"}
                DarkSpinBox {from:16;to:32768;suffix:" px";value:Math.round(root.textEditor.state.height || 180);onValueModified:root.document.setTextBox(true,root.textEditor.state.width || 320,value)}
            }
            Label {text:qsTr("Fonctions avancées");font.bold:true;color:"white"}
            Repeater {
                model:[qsTr("Texte sur tracé (NYI)"),qsTr("Déformation du texte (NYI)"),qsTr("Texte vertical (NYI)"),
                       qsTr("Masques de texte (NYI)"),qsTr("Options OpenType et glyphes (NYI)"),qsTr("Styles de caractère/paragraphe (NYI)"),qsTr("Modes de lissage Adobe (NYI)"),qsTr("Crénage optique (NYI)")]
                Label {required property string modelData;text:modelData;color:"#798693"}
            }
        }
    }
    ColorDialog {id:textColor;onAccepted:root.apply({color:selectedColor.toString()})}
}
