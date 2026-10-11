import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    color: "#20262e"
    implicitHeight: 44
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18; anchors.rightMargin: 18
        spacing: 8
        Label { text: qsTr("Pièce : Normal"); color: "#aebbc8" }
        RadioButton {
            objectName: "patchSource"
            text: qsTr("Source")
            checked: root.document.patchMode === 0
            onClicked: root.document.patchMode = 0
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Sélectionner la zone à réparer, puis glisser vers une texture saine")
        }
        RadioButton {
            objectName: "patchDestination"
            text: qsTr("Destination")
            checked: root.document.patchMode === 1
            onClicked: root.document.patchMode = 1
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Sélectionner une texture saine, puis glisser vers la zone à réparer")
        }
        Label { text: qsTr("Diffusion"); color: "#aebbc8"; Layout.leftMargin: 12 }
        DarkSpinBox {
            objectName: "patchDiffusion"
            from: 1; to: 7
            value: root.document.patchDiffusion
            implicitWidth: 82
            onValueModified: root.document.patchDiffusion = value
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Faible : textures détaillées ; élevée : zones lisses")
        }
        Label {
            text: qsTr("Entourez une zone, puis faites glisser la sélection")
            color: "#87929e"
            Layout.fillWidth: true; Layout.leftMargin: 12; elide: Text.ElideRight
        }
    }
}
