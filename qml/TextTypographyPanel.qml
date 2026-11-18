import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "Studio.js" as Studio

Popup {
    id: root
    required property var textEditor
    required property var document
    readonly property string family: textEditor.style.family || "sans-serif"
    readonly property color inkColor: textEditor.style.color || textEditor.state.color || "black"
    property string candidate: ""
    // The popup uses the footer button's coordinate system. Track layout offsets
    // up to the window so the scroll limit follows resizes and toolbar changes.
    readonly property real buttonTop: {
        let top = 0
        for (let item = parent; item; item = item.parent)
            top += item.y
        return top
    }
    readonly property int buttonGap: 6
    margins: 8
    width: 374
    height: Math.min(form.implicitHeight + 2 * padding, Math.max(1, buttonTop - margins - buttonGap))
    x: parent ? (parent.width - width) / 2 : 0
    y: -height - buttonGap
    padding: 14
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: Studio.panel; border.color: Studio.line; radius: 6 }

    function apply(values) { textEditor.apply(values) }
    function preview(name) {
        if (!fontFamily.popup.visible || !name || root.candidate === name) return
        root.candidate = name
        previewDelay.restart()
    }
    function endPreview() {
        previewDelay.stop()
        root.candidate = ""
        if (document) document.clearTextFontPreview()
    }
    function choose(name) {
        endPreview()
        if (name.trim().length) apply({family: name.trim()})
    }
    onAboutToShow: {
        advanced.checked = false
        fontFamily.editText = family
    }
    onFamilyChanged: if (visible) fontFamily.editText = family
    onAboutToHide: endPreview()
    Timer {
        id: previewDelay
        interval: 85
        onTriggered: if (root.visible && fontFamily.popup.visible && root.document.textEditing)
            root.document.previewTextFont(root.candidate, root.textEditor.input.selectionStart,
                                         root.textEditor.input.selectionEnd)
    }

    contentItem: ScrollView {
        clip: true
        ScrollBar.vertical.policy: advanced.checked ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        contentWidth: availableWidth
        ColumnLayout {
            id: form
            width: root.availableWidth - (advanced.checked ? 10 : 0)
            spacing: 7
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Caractère et paragraphe"); font.pixelSize: 15; font.weight: Font.DemiBold; color: Studio.ink; Layout.fillWidth: true }
                IconButton { glyph: "×"; hint: qsTr("Fermer"); implicitWidth: 28; implicitHeight: 28; onClicked: root.close() }
            }
            Label { text: root.textEditor.input.selectionStart !== root.textEditor.input.selectionEnd ? qsTr("Portée : sélection") : qsTr("Portée : tout le texte"); color: Studio.muted }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Police"); color: Studio.secondary; Layout.preferredWidth: 72 }
                ComboBox {
                    id: fontFamily
                    objectName: "textFontFamily"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    editable: true
                    hoverEnabled: true
                    implicitHeight: 32
                    Accessible.name: qsTr("Police")
                    model: root.textEditor.formatter.fontFamilies()
                    currentIndex: model.indexOf(root.family)
                    font.family: root.family
                    delegate: ItemDelegate {
                        required property int index
                        required property string modelData
                        objectName: "fontOption_" + index
                        width: ListView.view ? ListView.view.width : fontFamily.width
                        height: 33
                        text: modelData
                        font.family: modelData
                        font.pixelSize: 16
                        hoverEnabled: true
                        highlighted: fontFamily.highlightedIndex === index
                        onHoveredChanged: if (hovered) root.preview(modelData)
                        background: Rectangle { color: parent.highlighted || parent.hovered ? Studio.hover : Studio.field }
                    }
                    onHighlighted: function(index) { if (index >= 0) root.preview(textAt(index)) }
                    onActivated: function(index) { root.choose(textAt(index)) }
                    onAccepted: root.choose(editText)
                    popup: Popup {
                        y: fontFamily.height
                        width: fontFamily.width
                        height: Math.min(270, fontFamily.count * 33 + 2 * padding)
                        padding: 2
                        margins: 8
                        onOpened: if (fontFamily.currentIndex >= 0)
                                      faceList.positionViewAtIndex(fontFamily.currentIndex, ListView.Contain)
                        onAboutToHide: root.endPreview()
                        background: Rectangle { color: Studio.field; border.color: Studio.line; radius: 4 }
                        contentItem: ListView {
                            id: faceList
                            objectName: "fontChoices"
                            clip: true
                            model: fontFamily.popup.visible ? fontFamily.delegateModel : null
                            currentIndex: fontFamily.highlightedIndex
                            highlightMoveDuration: 0
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            HoverHandler { onHoveredChanged: if (!hovered) root.endPreview() }
                        }
                    }
                }
            }
            Label {
                visible: !root.textEditor.formatter.fontAvailable(root.family)
                text: qsTr("Police absente : un remplacement système est utilisé.")
                color: Studio.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                visible: stylePicker.count > 1
                Label { text: qsTr("Style"); color: Studio.secondary; Layout.preferredWidth: 72 }
                ComboBox {
                    id: stylePicker
                    objectName: "textFontStyle"
                    Layout.fillWidth: true
                    implicitHeight: 30
                    Accessible.name: qsTr("Style")
                    model: root.textEditor.formatter.fontStyles(root.family)
                    currentIndex: Math.max(0, model.indexOf(root.textEditor.style.style || ""))
                    onActivated: root.apply({style: currentText})
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ScrubLabel {
                    objectName: "textFontSizeLabel"; control: textFontSize
                    Layout.preferredWidth: 72
                    text: qsTr("Taille"); color: Studio.secondary
                    onScrubFinished: root.textEditor.input.forceActiveFocus()
                }
                DarkSpinBox {
                    id: textFontSize; objectName: "textFontSize"
                    Accessible.name: qsTr("Taille")
                    from: 1; to: 1024; suffix: " px"
                    value: Math.round(root.textEditor.style.size || 64)
                    onValueModified: root.apply({size: value})
                }
                Button {
                    Layout.fillWidth: true
                    implicitHeight: 32
                    Accessible.name: qsTr("Couleur du texte…")
                    contentItem: RowLayout {
                        spacing: 6
                        Rectangle { implicitWidth: 17; implicitHeight: 17; color: root.inkColor; border.color: Studio.muted; radius: 3 }
                        Label { text: qsTr("Couleur…"); color: Studio.ink; Layout.fillWidth: true }
                    }
                    onClicked: { textColor.selectedColor = root.inkColor; textColor.open() }
                }
            }
            RowLayout {
                Layout.leftMargin: 77
                Repeater {
                    model: [{letter:"B",key:"bold",label:qsTr("Gras")}, {letter:"I",key:"italic",label:qsTr("Italique")},
                            {letter:"U",key:"underline",label:qsTr("Souligné")}, {letter:"S",key:"strike",label:qsTr("Barré")}]
                    ToolButton {
                        required property var modelData
                        text: modelData.letter
                        implicitWidth: 33; implicitHeight: 28
                        font.bold: modelData.key === "bold"; font.italic: modelData.key === "italic"
                        font.underline: modelData.key === "underline"; font.strikeout: modelData.key === "strike"
                        Accessible.name: modelData.label
                        ToolTip.visible: hovered; ToolTip.text: modelData.label
                        checkable: true; checked: Boolean(root.textEditor.style[modelData.key])
                        onClicked: { const v = {}; v[modelData.key] = checked; root.apply(v) }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                objectName: "textAlignmentOptions"
                visible: Boolean(root.textEditor.state.paragraph) || root.textEditor.input.lineCount > 1
                Label { text: qsTr("Alignement"); color: Studio.secondary; Layout.preferredWidth: 72 }
                ComboBox {
                    Layout.fillWidth:true
                    implicitHeight: 32
                    Accessible.name: qsTr("Alignement")
                    model:[qsTr("Aligné à gauche"),qsTr("Centré"),qsTr("Aligné à droite"),qsTr("Justifié")]
                    currentIndex:root.textEditor.style.alignment & Qt.AlignJustify ? 3 : root.textEditor.style.alignment & Qt.AlignRight ? 2 : root.textEditor.style.alignment & Qt.AlignHCenter ? 1 : 0
                    onActivated:root.apply({alignment:[Qt.AlignLeft,Qt.AlignHCenter,Qt.AlignRight,Qt.AlignJustify][currentIndex]})
                }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Studio.line }
            ToolButton {
                id: advanced
                objectName: "textAdvancedToggle"
                Layout.fillWidth: true
                implicitHeight: 30
                checkable: true
                text: qsTr("Options avancées")
                contentItem: Label {
                    text: (advanced.checked ? "▾  " : "▸  ") + advanced.text
                    color: advanced.checked || advanced.hovered ? Studio.ink : Studio.secondary
                    verticalAlignment: Text.AlignVCenter
                }
            }
            ColumnLayout {
                objectName: "textAdvancedOptions"
                visible: advanced.checked
                Layout.fillWidth: true
                spacing: 7
                RowLayout {
                    Label {text:qsTr("Casse");color:Studio.secondary}
                    ComboBox {Layout.fillWidth:true; Accessible.name: qsTr("Casse"); model:[qsTr("Normale"),qsTr("Capitales"),qsTr("Petites capitales")];currentIndex:root.textEditor.style.caps || 0;onActivated:root.apply({caps:currentIndex})}
                    ComboBox {Layout.fillWidth:true; Accessible.name: qsTr("Position"); model:[qsTr("Normal"),qsTr("Exposant"),qsTr("Indice")];currentIndex:root.textEditor.style.script || 0;onActivated:root.apply({script:currentIndex})}
                }
                Repeater {
                    model:[{label:qsTr("Approche (1/1000 em)"),key:"tracking",min:-1000,max:1000},
                           {label:qsTr("Échelle horizontale (%)"),key:"stretch",min:1,max:4000},
                           {label:qsTr("Décalage de ligne (%)"),key:"baseline",min:-1000,max:1000},
                           {label:qsTr("Interligne (0 = auto)"),key:"leading",min:0,max:4096}]
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth:true
                        ScrubLabel { onScrubFinished: root.textEditor.input.forceActiveFocus(); control: scrubValue2;text:modelData.label;color:Studio.secondary;Layout.fillWidth:true}
                        DarkSpinBox { id: scrubValue2;from:modelData.min;to:modelData.max;value:Math.round(root.textEditor.style[modelData.key] || (modelData.key==="stretch"?100:0));onValueModified:{const v={};v[modelData.key]=value;root.apply(v)}}
                    }
                }
                CheckBox {text:qsTr("Crénage métrique");checked:root.textEditor.style.kerning!==false;onToggled:root.apply({kerning:checked})}
                Label {text:qsTr("Paragraphe");font.bold:true;color:Studio.ink}
                CheckBox {text:qsTr("Direction de droite à gauche");checked:Boolean(root.textEditor.style.rtl);onToggled:root.apply({rtl:checked})}
                Repeater {
                    model:[{label:qsTr("Retrait gauche"),key:"left"},{label:qsTr("Retrait droit"),key:"right"},
                           {label:qsTr("Première ligne"),key:"indent"},{label:qsTr("Espace avant"),key:"before"},{label:qsTr("Espace après"),key:"after"}]
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth:true
                        ScrubLabel { onScrubFinished: root.textEditor.input.forceActiveFocus(); control: scrubValue3;text:modelData.label;color:Studio.secondary;Layout.fillWidth:true}
                        DarkSpinBox { id: scrubValue3;from:modelData.key==="indent"?-4096:0;to:4096;suffix:" px";value:Math.round(root.textEditor.style[modelData.key] || 0);onValueModified:{const v={};v[modelData.key]=value;root.apply(v)}}
                    }
                }
                CheckBox {text:qsTr("Bloc de paragraphe");checked:Boolean(root.textEditor.state.paragraph);onToggled:root.document.setTextBox(checked,root.textEditor.state.width || 320,root.textEditor.state.height || 180)}
                RowLayout {
                    ScrubLabel { onScrubFinished: root.textEditor.input.forceActiveFocus(); control: scrubValue4;text:qsTr("L");color:Studio.secondary}
                    DarkSpinBox { id: scrubValue4;from:16;to:32768;suffix:" px";value:Math.round(root.textEditor.state.width || 320);onValueModified:root.document.setTextBox(true,value,root.textEditor.state.height || 180)}
                    ScrubLabel { onScrubFinished: root.textEditor.input.forceActiveFocus(); control: scrubValue5;text:qsTr("H");color:Studio.secondary}
                    DarkSpinBox { id: scrubValue5;from:16;to:32768;suffix:" px";value:Math.round(root.textEditor.state.height || 180);onValueModified:root.document.setTextBox(true,root.textEditor.state.width || 320,value)}
                }
            }
        }
    }
    ColorDialog { id: textColor; onAccepted: root.apply({color: selectedColor.toString()}) }
}
