import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    signal exportRequested()
    color: "#20262e"; implicitHeight: 44
    enabled: !document.sliceBusy
    readonly property var selected: document.activeSlice>=0 ? document.slices[document.activeSlice] : null
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18; spacing: 8
        ToolIcon { toolName: root.document.tool; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
        Repeater {
            model: [{label:"X",key:"x"},{label:"Y",key:"y"},{label:qsTr("L"),key:"width"},{label:qsTr("H"),key:"height"}]
            delegate: RowLayout {
                required property var modelData
                spacing: 4
                Label { text:modelData.label+":"; color:"#b4beca" }
                TextField {
                    implicitWidth: 64; implicitHeight: 30
                    enabled: root.selected!==null
                    text: root.selected ? root.selected[modelData.key] : ""
                    validator: IntValidator { bottom: 0; top: 32768 }
                    selectByMouse: true
                    onEditingFinished: {
                        if(!root.selected || !acceptableInput) return
                        const r={x:root.selected.x,y:root.selected.y,width:root.selected.width,height:root.selected.height}
                        r[modelData.key]=Number(text)
                        root.document.setActiveSliceRect(r.x,r.y,r.width,r.height)
                    }
                }
            }
        }
        Button { objectName:"divideSliceButton";text:qsTr("Diviser…");enabled:root.selected!==null;implicitHeight:30;onClicked:dividePopup.open() }
        Button { objectName:"deleteSliceButton";text:qsTr("Supprimer");enabled:root.selected!==null;implicitHeight:30;onClicked:root.document.deleteActiveSlice() }
        CheckBox { text:qsTr("Tranches automatiques");checked:root.document.sliceShowAuto;onToggled:root.document.sliceShowAuto=checked }
        Item { Layout.fillWidth:true }
        Button { objectName:"exportSlicesButton";text:qsTr("Exporter en PNG…");enabled:root.document.slices.some(function(s){return !s.automatic});implicitHeight:30;onClicked:root.exportRequested() }
    }
    Popup {
        id: dividePopup
        anchors.centerIn: Overlay.overlay
        modal: true; focus: true
        contentItem: ColumnLayout {
            Label { text:qsTr("Diviser la tranche sélectionnée");font.bold:true }
            RowLayout {
                Label { text:qsTr("Colonnes") }
                SpinBox { id: columns;objectName:"sliceDivisionColumns";from:1;to:32;value:2;editable:true }
                Label { text:qsTr("Rangées") }
                SpinBox { id: rows;objectName:"sliceDivisionRows";from:1;to:32;value:2;editable:true }
            }
            Label { text:qsTr("256 tranches maximum par document");opacity:.7 }
            RowLayout {
                Item { Layout.fillWidth:true }
                Button { text:qsTr("Annuler");onClicked:dividePopup.close() }
                Button {
                    objectName:"confirmSliceDivision"
                    text:qsTr("Diviser")
                    enabled:root.selected!==null && columns.value*rows.value>1
                            && columns.value<=root.selected.width && rows.value<=root.selected.height
                            && root.document.slices.filter(function(s){return !s.automatic}).length-1+columns.value*rows.value<=256
                    onClicked:{root.document.divideActiveSlice(columns.value,rows.value);dividePopup.close()}
                }
            }
        }
    }
}
