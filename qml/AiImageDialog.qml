import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var document
    required property var settings
    required property var controller
    signal settingsRequested
    property string selectedPrompt: ""
    property var selectedProvider: providers.currentIndex >= 0 && providers.currentIndex < settings.providers.length ? settings.providers[providers.currentIndex] : ({})
    objectName: "aiImageDialog"
    title: qsTr("Créer avec l’IA")
    modal: true
    width: Math.min(850, parent.width - 60)
    height: Math.min(610, parent.height - 60)
    anchors.centerIn: parent
    function start() {
        source.currentIndex = document.hasSelection ? 1 : 0;
        open();
    }
    function chooseProvider(id) {
        providers.selectedId = id;
    }
    contentItem: ColumnLayout {
        spacing: 14
        Label {
            Layout.fillWidth: true
            text: qsTr("Choisissez un fournisseur et décrivez la modification souhaitée.")
            color: "#aab6c3"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18
            ColumnLayout {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                spacing: 10
                Label {
                    text: qsTr("Fournisseurs")
                    color: "#e2e8ee"
                    font.pixelSize: 14
                    font.bold: true
                }
                AiProviderList {
                    id: providers
                    objectName: "aiGenerationProvider"
                    settings: dialog.settings
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                AiActionButton {
                    text: qsTr("Gérer les fournisseurs…")
                    Layout.fillWidth: true
                    onClicked: {
                        dialog.close();
                        dialog.settingsRequested();
                    }
                }
            }
            Rectangle {
                width: 1
                Layout.fillHeight: true
                color: "#35404b"
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10
                Label {
                    text: qsTr("Source")
                    color: "#aab6c3"
                    font.pixelSize: 12
                }
                Basic.ComboBox {
                    id: source
                    objectName: "aiGenerationSource"
                    Layout.fillWidth: true
                    implicitHeight: 32
                    palette.buttonText: "#e0e7ee"
                    palette.button: "#2d3641"
                    palette.text: "#e0e7ee"
                    palette.base: "#202730"
                    model: dialog.document.hasSelection ? [qsTr("Composition visible complète"), qsTr("Sélection uniquement")] : [qsTr("Composition visible complète")]
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                    color: "#899aa9"
                    text: source.currentIndex === 1 ? qsTr("Le résultat sera replacé dans la sélection, avec son masque.") : qsTr("Le résultat sera ajouté au-dessus de l’image, sur un nouveau calque.")
                }
                Label {
                    text: qsTr("Prompt")
                    color: "#aab6c3"
                    font.pixelSize: 12
                }
                Basic.ComboBox {
                    id: prompts
                    objectName: "aiGenerationPrompt"
                    Layout.fillWidth: true
                    implicitHeight: 32
                    palette.buttonText: "#e0e7ee"
                    palette.button: "#2d3641"
                    palette.text: "#e0e7ee"
                    palette.base: "#202730"
                    model: [qsTr("Prompt libre")].concat(dialog.settings.prompts.map(function (item) {
                        return item.name;
                    }))
                    onActivated: {
                        dialog.selectedPrompt = currentIndex > 0 ? dialog.settings.prompts[currentIndex - 1].id : "";
                        if (currentIndex > 0) {
                            prompt.text = dialog.settings.prompts[currentIndex - 1].text;
                            name.text = dialog.settings.prompts[currentIndex - 1].name;
                        } else
                            name.text = "";
                    }
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Basic.TextArea {
                        id: prompt
                        objectName: "aiGenerationText"
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        padding: 10
                        color: "#e0e7ee"
                        placeholderTextColor: "#7f8b99"
                        font.pixelSize: 13
                        placeholderText: qsTr("Décrivez le résultat souhaité et les éléments à préserver…")
                        background: Rectangle {
                            color: "#181e25"
                            radius: 5
                            border.color: prompt.activeFocus ? "#8eb39a" : "#3b4551"
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    AiTextField {
                        id: name
                        Layout.fillWidth: true
                        placeholderText: qsTr("Nom pour enregistrer ce prompt")
                    }
                    AiActionButton {
                        text: qsTr("Enregistrer")
                        enabled: name.text.trim().length > 0 && prompt.text.trim().length > 0
                        onClicked: {
                            const saved = dialog.settings.savePrompt({
                                id: dialog.selectedPrompt,
                                name: name.text,
                                text: prompt.text
                            });
                            if (saved) {
                                dialog.selectedPrompt = saved;
                                prompts.currentIndex = 1 + dialog.settings.prompts.findIndex(function (item) {
                                    return item.id === saved;
                                });
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Variantes")
                        color: "#aab6c3"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    DarkSpinBox {
                        id: variants
                        objectName: "aiVariantCount"
                        from: 1
                        to: 8
                        value: 1
                        implicitHeight: 32
                        Layout.preferredWidth: 120
                    }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                    color: "#899aa9"
                    text: qsTr("%1 génération(s) · grand côté limité à %2 px").arg(variants.value).arg(dialog.selectedProvider.maxEdge || 1536)
                }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#eea39c"
            font.pixelSize: 12
            visible: text.length > 0
            text: dialog.controller.error || dialog.settings.error
        }
    }
    footer: Item {
        implicitHeight: 52
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#35404b"
        }
        RowLayout {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            spacing: 8
            AiActionButton {
                text: qsTr("Annuler")
                onClicked: dialog.reject()
            }
            AiActionButton {
                objectName: "aiGenerateButton"
                primary: true
                text: qsTr("Générer")
                enabled: !dialog.controller.busy && !dialog.controller.pendingResult && !dialog.settings.credentialsBusy && providers.count > 0 && prompt.text.trim().length > 0
                onClicked: {
                    if (dialog.controller.generate(dialog.selectedProvider.id, prompt.text, source.currentIndex === 1, variants.value))
                        dialog.close();
                }
            }
        }
    }
}
