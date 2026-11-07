import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    required property var document
    required property var blendModes
    signal editCommitted()
    spacing: 8

    ComboBox {
        objectName: "fillSource"
        implicitWidth: 140
        model: [qsTr("Premier plan"), qsTr("Motif")]
        currentIndex: root.document.fillUsePattern ? 1 : 0
        onActivated: {
            root.document.fillUsePattern = currentIndex === 1
            root.editCommitted()
        }
    }
    ComboBox {
        objectName: "fillPattern"
        visible: root.document.fillUsePattern
        implicitWidth: 140
        model: root.document.hasCustomStampPattern
            ? [qsTr("Damier"), qsTr("Points"), qsTr("Rayures"), qsTr("Motif personnalisé")]
            : [qsTr("Damier"), qsTr("Points"), qsTr("Rayures")]
        currentIndex: root.document.fillPattern
        onActivated: {
            root.document.fillPattern = currentIndex
            root.editCommitted()
        }
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Motif de remplissage, aligné sur le document")
    }
    Button {
        visible: root.document.fillUsePattern
        enabled: root.document.hasSelection
        text: qsTr("Définir…")
        onClicked: {
            if (root.document.defineStampPattern())
                root.document.fillPattern = 3
            root.editCommitted()
        }
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Créer un motif à partir de la sélection")
    }
    Label { text: qsTr("Mode"); color: "#c8d0d9" }
    ComboBox {
        objectName: "fillBlend"
        implicitWidth: 160
        model: root.blendModes
        currentIndex: root.document.fillBlend
        onActivated: {
            root.document.fillBlend = currentIndex
            root.editCommitted()
        }
    }
    Label { text: qsTr("Opacité"); color: "#c8d0d9" }
    DarkSpinBox {
        objectName: "fillOpacity"
        from: 0; to: 100
        value: Math.round(root.document.brushOpacity * 100)
        suffix: " %"
        implicitWidth: 96
        onValueModified: root.document.brushOpacity = value / 100
    }
    Label { text: qsTr("Tolérance"); color: "#c8d0d9" }
    DarkSpinBox {
        objectName: "fillTolerance"
        from: 0; to: 255
        value: root.document.fillTolerance
        implicitWidth: 76
        onValueModified: root.document.fillTolerance = value
    }
    CheckBox {
        objectName: "fillAntialias"
        text: qsTr("Anticrénelage")
        checked: root.document.fillAntialias
        onToggled: { root.document.fillAntialias = checked; root.editCommitted() }
    }
    CheckBox {
        objectName: "fillContiguous"
        text: qsTr("Pixels contigus")
        checked: root.document.fillContiguous
        onToggled: { root.document.fillContiguous = checked; root.editCommitted() }
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Décocher pour remplir aussi les zones de couleur similaire séparées")
    }
    CheckBox {
        objectName: "fillSampleAllLayers"
        text: qsTr("Tous les calques")
        enabled: !root.document.editingMask
        checked: root.document.fillSampleAllLayers
        onToggled: { root.document.fillSampleAllLayers = checked; root.editCommitted() }
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Détecter les couleurs sur les calques visibles et remplir le calque actif")
    }
}
