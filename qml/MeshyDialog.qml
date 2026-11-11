import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "meshyDialog"
    required property var controller
    required property var settings
    required property bool hasSelection
    readonly property string keySource: (settings.revision, settings.keySource("meshy"))
    anchors.centerIn: parent
    width: Math.min(510, parent.width - 32)
    title: qsTr("Générer un calque 3D avec Meshy")
    modal: true

    onOpened: {
        source.currentIndex = hasSelection ? 2 : 0
        description.text = ""
        key.clear()
        remember.checked = true
        ultra.checked = false
        if (source.currentIndex === 0)
            description.forceActiveFocus()
    }
    onClosed: key.clear()

    contentItem: ColumnLayout {
        spacing: 10
        Label { text: qsTr("Source") }
        ComboBox {
            id: source
            objectName: "meshySource"
            Layout.fillWidth: true
            model: dialog.hasSelection
                   ? [qsTr("Description"), qsTr("Image du document"), qsTr("Sélection")]
                   : [qsTr("Description"), qsTr("Image du document")]
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            visible: source.currentIndex === 0
            TextArea {
                id: description
                objectName: "meshyPrompt"
                placeholderText: qsTr("Décrivez l’objet à générer")
                wrapMode: TextEdit.Wrap
                selectByMouse: true
            }
        }
        Label {
            Layout.fillWidth: true
            color: "#a5b2c0"
            wrapMode: Text.WordWrap
            text: source.currentIndex === 0
                  ? qsTr("La description sera envoyée à Meshy pour générer puis texturer le modèle. Ces étapes utilisent des crédits Meshy.")
                  : qsTr("L’image choisie sera envoyée à Meshy pour générer et texturer le modèle. Cette opération utilise des crédits Meshy.")
        }
        CheckBox {
            id: ultra
            text: qsTr("Géométrie Ultra 4K (+5 crédits Meshy)")
        }
        Label { text: qsTr("Clé API Meshy") }
        TextField {
            id: key
            objectName: "meshyApiKey"
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: dialog.keySource === "environment" ? qsTr("Clé fournie par MESHY_API_KEY")
                            : dialog.keySource === "stored"
                            ? qsTr("Clé du trousseau · saisir pour la remplacer")
                            : qsTr("Saisissez votre clé API")
        }
        CheckBox {
            id: remember
            visible: key.text.length > 0
            text: qsTr("Enregistrer la clé dans le trousseau")
        }
    }

    footer: DialogButtonBox {
        DialogActionButton {
            text: qsTr("Annuler")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: dialog.reject()
        }
        DialogActionButton {
            objectName: "meshyGenerate"
            text: qsTr("Générer")
            defaultButton: !description.activeFocus
            enabled: !dialog.controller.busy
                     && (key.text.trim().length > 0 || dialog.keySource === "environment" || dialog.keySource === "stored")
                     && (source.currentIndex !== 0 || description.text.trim().length > 0
                         && description.text.trim().length <= 800)
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: {
                if (dialog.controller.generate(source.currentIndex, description.text, key.text, remember.checked, ultra.checked))
                    dialog.close()
            }
        }
    }
}
