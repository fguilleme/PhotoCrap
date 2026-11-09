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
    readonly property string promptMode: selectedProvider.promptMode || "required"
    readonly property bool topaz: selectedProvider.kind === "fal" && selectedProvider.model === "topaz/upscale/image/precision"
    readonly property bool esrgan: selectedProvider.kind === "fal" && selectedProvider.model === "fal-ai/esrgan"
    readonly property bool upscaling: topaz || esrgan
    readonly property var originalSource: document.originalAiSource
    readonly property var sources: [{name: qsTr("Composition visible complète"), kind: "full"}]
        .concat(document.hasSelection ? [{name: qsTr("Sélection uniquement"), kind: "selection"}] : [])
        .concat(originalSource.width ? [{name: qsTr("Calque actif · image IA d’origine (%1 × %2)").arg(originalSource.width).arg(originalSource.height), kind: "original"}] : [])
    readonly property string sourceKind: source.currentIndex >= 0 && source.currentIndex < sources.length ? sources[source.currentIndex].kind : "full"
    function refreshSource() {
        source.currentIndex = upscaling && originalSource.width ? sources.findIndex(function(s) { return s.kind === "original" })
                              : document.hasSelection ? 1 : 0
    }
    function refreshTopaz() {
        let options = ({})
        try { options = JSON.parse(selectedProvider.options || "{}") } catch(e) {}
        topazModel.currentIndex = Math.max(0, topazModel.model.indexOf(options.model || (esrgan ? "RealESRGAN_x2plus" : "Standard V2")))
        const factor = Number(options[esrgan ? "scale" : "upscale_factor"] || 2)
        upscaleFactor.value = Number.isFinite(factor) && factor >= 1 && factor <= 4 ? Math.round(factor * 100) : 200
    }
    onSelectedProviderChanged: Qt.callLater(function() { refreshSource(); refreshTopaz() })
    objectName: "aiImageDialog"
    title: qsTr("Créer avec l’IA")
    modal: true
    width: Math.min(850, parent.width - 60)
    height: Math.min(610, parent.height - 60)
    anchors.centerIn: parent
    function start() {
        refreshSource();
        refreshTopaz();
        destination.currentIndex = upscaling ? 1 : 0;
        open();
    }
    function chooseProvider(id) {
        providers.selectedId = id;
    }
    contentItem: ColumnLayout {
        spacing: 14
        Label {
            Layout.fillWidth: true
            text: dialog.promptMode === "none" ? qsTr("Choisissez l’image et les paramètres du traitement. Aucun prompt n’est nécessaire.") : qsTr("Choisissez un fournisseur et décrivez la modification souhaitée.")
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
                    model: dialog.sources.map(function(s) { return s.name })
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                    color: "#899aa9"
                    text: destination.currentIndex === 1 ? qsTr("Le résultat s’ouvre à sa résolution native dans un nouvel onglet. Le document source reste intact.") : dialog.sourceKind === "original" ? qsTr("L’image IA est envoyée à sa résolution d’origine. Le résultat revient sur un nouveau calque, à sa place dans le document, avec son masque.") : dialog.sourceKind === "selection" ? qsTr("Le résultat sera replacé dans la sélection, avec son masque.") : qsTr("Le résultat sera ajouté au-dessus de l’image, sur un nouveau calque.")
                }
                ColumnLayout {
                    visible: dialog.upscaling
                    Layout.fillWidth: true
                    Label { text: dialog.esrgan ? qsTr("Modèle Real-ESRGAN") : qsTr("Modèle Topaz"); color: "#aab6c3"; font.pixelSize: 12 }
                    Basic.ComboBox {
                        id: topazModel
                        objectName: "aiTopazModel"
                        Layout.fillWidth: true
                        model: dialog.esrgan ? ["RealESRGAN_x2plus", "RealESRGAN_x4plus", "RealESRGAN_x4_v3", "RealESRGAN_x4_wdn_v3", "RealESRGAN_x4plus_anime_6B", "RealESRGAN_x4_anime_v3"] : ["Standard V2", "High Fidelity V3", "High Fidelity V2", "Low Resolution V2", "CGI", "Text Refine", "Faces"]
                        palette.buttonText: "#e0e7ee"; palette.button: "#2d3641"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Agrandissement"); color: "#aab6c3"; Layout.fillWidth: true }
                        DarkSpinBox {
                            id: upscaleFactor
                            objectName: "aiUpscaleFactor"
                            from: 100; to: 400; value: 200; stepSize: 25
                            suffix: "×"
                            Layout.preferredWidth: 160
                            textFromValue: function(value, locale) { return Number(value / 100).toLocaleString(locale, "f", 2) }
                            valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text.trim()) * 100) }
                        }
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#899aa9"; font.pixelSize: 11
                        text: destination.currentIndex === 1 ? qsTr("Le résultat s’ouvre dans un nouvel onglet à sa résolution native. Le document source reste intact. La sortie doit rester sous 32 Mpx et 16 384 px par côté.") : qsTr("Le document conserve ses dimensions ; l’agrandissement améliore l’image envoyée avant sa remise à l’échelle sur le calque. La sortie doit rester sous 32 Mpx et 16 384 px par côté.")
                    }
                }
                Label { text: qsTr("Destination"); color: "#aab6c3"; font.pixelSize: 12 }
                Basic.ComboBox {
                    id: destination
                    objectName: "aiResultDestination"
                    Layout.fillWidth: true
                    model: [qsTr("Nouveau calque dans ce document"), qsTr("Nouveau document · résolution native")]
                    palette.buttonText: "#e0e7ee"; palette.button: "#2d3641"
                }
                Label {
                    visible: dialog.promptMode !== "none"
                    text: dialog.promptMode === "optional" ? qsTr("Prompt (facultatif)") : qsTr("Prompt")
                    color: "#aab6c3"
                    font.pixelSize: 12
                }
                Basic.ComboBox {
                    id: prompts
                    visible: dialog.promptMode !== "none"
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
                    visible: dialog.promptMode !== "none"
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
                    visible: dialog.promptMode !== "none"
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
                    visible: !dialog.upscaling
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
                    text: dialog.selectedProvider.nativeInput ? (destination.currentIndex === 1 ? qsTr("Envoi et résultat à la résolution native · nouvel onglet") : qsTr("Envoi à la résolution native · résultat sur un nouveau calque")) : qsTr("%1 génération(s) · grand côté limité à %2 px").arg(variants.value).arg(dialog.selectedProvider.maxEdge || 1536)
                }
                Item { visible: dialog.promptMode === "none"; Layout.fillHeight: true }
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
                text: dialog.upscaling ? qsTr("Agrandir") : dialog.promptMode === "none" ? qsTr("Lancer le traitement") : qsTr("Générer")
                enabled: !dialog.controller.busy && !dialog.controller.pendingResult && !dialog.settings.credentialsBusy && providers.count > 0 && (dialog.promptMode !== "required" || prompt.text.trim().length > 0)
                onClicked: {
                    if (dialog.controller.generate(dialog.selectedProvider.id, dialog.promptMode === "none" ? "" : prompt.text, dialog.sourceKind === "selection", dialog.upscaling ? 1 : variants.value,
                                                   dialog.sourceKind === "original", dialog.esrgan ? {model: topazModel.currentText, scale: upscaleFactor.value / 100} : dialog.topaz ? {model: topazModel.currentText, upscale_factor: upscaleFactor.value / 100} : {}, destination.currentIndex === 1))
                        dialog.close();
                }
            }
        }
    }
}
