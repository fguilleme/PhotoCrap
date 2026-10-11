import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    signal colorRequested()
    color: "#20262e"
    implicitHeight: 44

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 8
        ComboBox {
            objectName: "mixerPreset"
            model: [qsTr("Personnalisé"), qsTr("Sec"), qsTr("Humide"), qsTr("Mouillé"), qsTr("Très mouillé")]
            currentIndex: root.document.mixerPreset
            implicitWidth: 134; implicitHeight: 30
            onActivated: root.document.applyMixerPreset(currentIndex)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Combinaisons d’humidité, de charge et de mélange")
        }
        Button {
            id: loadSwatch
            objectName: "mixerLoadSwatch"
            implicitWidth: 54; implicitHeight: 30
            Accessible.name: qsTr("Charge du pinceau")
            onClicked: loadMenu.popup(loadSwatch, 0, loadSwatch.height)
            contentItem: Row {
                spacing: 5
                Rectangle {
                    width: 24; height: 20
                    color: root.document.mixerLoaded ? root.document.mixerLoadColor : "transparent"
                    border.color: "#b3beca"
                    Text { anchors.centerIn: parent; visible: !root.document.mixerLoaded; text: "∅"; color: "#b3beca" }
                }
                Text { text: "▾"; color: "#b3beca" }
            }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Charge actuelle ; Option/Alt-clic sur l’image pour prélever de la peinture")
        }
        Repeater {
            model: [
                {key:"mixerWet", label:qsTr("Humidité"), hint:qsTr("Quantité de peinture prélevée sur l’image")},
                {key:"mixerLoad", label:qsTr("Charge"), hint:qsTr("Réserve de peinture : une faible charge sèche plus vite")},
                {key:"mixerMix", label:qsTr("Mélange"), hint:qsTr("0 % : peinture chargée ; 100 % : couleurs de l’image")},
                {key:"mixerFlow", label:qsTr("Débit"), hint:qsTr("Quantité de peinture déposée par empreinte")}
            ]
            delegate: RowLayout {
                required property var modelData
                spacing: 5
                Label { text: modelData.label; color: "#aebbc8" }
                DarkSpinBox {
                    objectName: modelData.key
                    from: 0; to: 100
                    value: Math.round(root.document[modelData.key] * 100)
                    suffix: " %"; implicitWidth: 112
                    enabled: modelData.key !== "mixerMix" || root.document.mixerWet > 0
                    onValueModified: root.document[modelData.key] = value / 100
                    ToolTip.visible: hovered
                    ToolTip.text: modelData.hint
                }
            }
        }
        CheckBox {
            objectName: "mixerSampleAllLayers"
            text: qsTr("Tous les calques")
            enabled: !root.document.editingMask
            checked: root.document.mixerSampleAllLayers
            onToggled: root.document.mixerSampleAllLayers = checked
        }
        Item { Layout.fillWidth: true }
    }
    DarkMenu {
        id: loadMenu
        objectName: "mixerLoadMenu"
        DarkMenuItem {
            objectName: "mixerLoadAction"
            text: qsTr("Charger le pinceau")
            onTriggered: root.document.loadMixerBrush()
        }
        DarkMenuItem {
            objectName: "mixerCleanAction"
            text: qsTr("Nettoyer le pinceau")
            onTriggered: root.document.cleanMixerBrush()
        }
        DarkMenuItem {
            text: qsTr("Choisir la couleur…")
            onTriggered: root.colorRequested()
        }
        MenuSeparator {}
        DarkMenuItem {
            objectName: "mixerAutoLoad"
            text: qsTr("Charger après chaque trait")
            checkable: true; checked: root.document.mixerAutoLoad
            onTriggered: root.document.mixerAutoLoad = checked
        }
        DarkMenuItem {
            objectName: "mixerAutoClean"
            text: qsTr("Nettoyer après chaque trait")
            checkable: true; checked: root.document.mixerAutoClean
            onTriggered: root.document.mixerAutoClean = checked
        }
        DarkMenuItem {
            objectName: "mixerSolidOnly"
            text: qsTr("Charger les couleurs unies seulement")
            checkable: true; checked: root.document.mixerSolidOnly
            onTriggered: root.document.mixerSolidOnly = checked
        }
    }
}
