import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    signal editCommitted()
    readonly property bool modalOpen: rename.opened
    implicitHeight: 44
    color: "#252c35"
    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: options.implicitWidth+24
        contentHeight: 40
        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
        RowLayout {
            id: options
            x: 12;height:40;spacing:8
            Label { text:qsTr("Tracé");color:"#c4d0db" }
            ComboBox {
                objectName:"penPathCombo"
                Layout.preferredWidth:160
                model:root.document.penPaths
                textRole:"name"
                currentIndex:root.document.activePenPath
                displayText:currentIndex<0?qsTr("Nouveau tracé"):currentText
                onActivated: {root.document.setActivePenPath(currentIndex);root.editCommitted()}
            }
            ToolButton { text:"+";ToolTip.text:qsTr("Nouveau tracé");ToolTip.visible:hovered;onClicked:{root.document.newPenPath();root.editCommitted()} }
            ToolButton { text:"…";enabled:root.document.activePenPath>=0;ToolTip.text:qsTr("Renommer le tracé");ToolTip.visible:hovered;onClicked:{pathName.text=root.document.penPaths[root.document.activePenPath].name;rename.open()} }
            ToolButton { enabled:root.document.activePenPath>=0;implicitWidth:32;contentItem:TrashIcon {} ToolTip.text:qsTr("Supprimer le tracé");ToolTip.visible:hovered;onClicked:{root.document.deletePenPath();root.editCommitted()} }
            CheckBox { text:qsTr("Ajout/Suppression auto");checked:root.document.penAutoAddDelete;onToggled:root.document.penAutoAddDelete=checked }
            CheckBox { text:qsTr("Aperçu");checked:root.document.penRubberBand;onToggled:root.document.penRubberBand=checked;ToolTip.text:qsTr("Prévisualiser le prochain segment (élastique)");ToolTip.visible:hovered }
            Button { text:qsTr("Terminer");enabled:root.document.penDrawing;onClicked:{root.document.finishPenPath();root.editCommitted()} }
            Button { text:qsTr("Fermer le tracé");enabled:root.document.activePenPath>=0;onClicked:{root.document.closePenPath();root.editCommitted()} }
            Button { objectName:"penSelectionButton";text:qsTr("Sélection");enabled:root.document.activePenPath>=0;onClicked:{root.document.penPathToSelection();root.editCommitted()} }
            Button { text:qsTr("Remplir");enabled:root.document.activePenPath>=0;ToolTip.text:qsTr("Remplir avec le premier plan et l’opacité de peinture");ToolTip.visible:hovered;onClicked:{root.document.fillPenPath();root.editCommitted()} }
            Button { text:qsTr("Contour");enabled:root.document.activePenPath>=0;ToolTip.text:qsTr("Tracer avec les réglages actuels du pinceau");ToolTip.visible:hovered;onClicked:{root.document.strokePenPath();root.editCommitted()} }
            ToolButton { text:"⚙";onClicked:future.popup() }
        }
    }
    DarkMenu {
        id:future
        ContextMenuItem {text:qsTr("Calque de forme vectorielle (NYI)");enabled:false}
        ContextMenuItem {text:qsTr("Masque vectoriel (NYI)");enabled:false}
        ContextMenuItem {text:qsTr("Opérations booléennes de tracés (NYI)");enabled:false}
        ContextMenuItem {text:qsTr("Contour progressif de sélection (NYI)");enabled:false}
        ContextMenuItem {text:qsTr("Simulation de pression (NYI)");enabled:false}
    }
    Dialog {
        id:rename
        title:qsTr("Renommer le tracé")
        parent:Overlay.overlay;anchors.centerIn:parent
        modal:true
        TextField {id:pathName;selectByMouse:true;maximumLength:256;onAccepted:rename.accept()}
        footer:DialogButtonBox {
            DialogActionButton {text:qsTr("Annuler");DialogButtonBox.buttonRole:DialogButtonBox.RejectRole;onClicked:rename.reject()}
            DialogActionButton {text:qsTr("OK");defaultButton:true;DialogButtonBox.buttonRole:DialogButtonBox.AcceptRole;onClicked:rename.accept()}
        }
        onOpened:{pathName.forceActiveFocus();pathName.selectAll()}
        onAccepted:{root.document.renamePenPath(pathName.text);root.editCommitted()}
    }
}
