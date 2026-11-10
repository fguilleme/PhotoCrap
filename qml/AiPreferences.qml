import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import QtQuick.Dialogs

ColumnLayout {
    id: panel
    objectName: "aiPreferencesPanel"
    required property var settings
    signal providerSaved(string providerId)
    property string providerId: ""
    property string providerKind: "openai"
    readonly property bool basicAuth: providerKind === "gradio" && authMode.currentIndex === 0
    property string promptId: ""
    property string notice: ""
    readonly property bool topaz: providerKind === "fal" && modelName.text.trim() === "topaz/upscale/image/precision"
    readonly property bool esrgan: providerKind === "fal" && modelName.text.trim() === "fal-ai/esrgan"
    readonly property bool upscaling: topaz || esrgan
    spacing: 12
    function selectProvider(id) {
        const value = settings.providers.find(function (entry) {
            return entry.id === id;
        });
        if (!value)
            return;
        providerId = value.id;
        providerKind = value.kind;
        providerList.selectedId = value.id;
        providerName.text = value.name || "";
        serverUrl.text = value.url || "";
        modelName.text = value.model || "";
        environmentName.text = value.keyEnv || "";
        options.text = value.options || "{}";
        qwenParameters.load(options.text);
        username.text = value.username || "";
        authMode.currentIndex = value.authMode === "none" ? 1 : 0;
        workflow.text = value.workflow || "";
        imageField.text = value.imageField || "image_urls";
        imageArray.checked = value.imageArray !== false;
        promptMode.currentIndex = Math.max(0, ["required", "optional", "none"].indexOf(value.promptMode || "required"));
        nativeInput.checked = value.nativeInput === true;
        maxEdge.value = value.maxEdge || 1536;
        apiKey.text = "";
        notice = "";
    }
    function createProvider(kind) {
        const entry = settings.newProvider(kind);
        const base = entry.name;
        let suffix = 2;
        while (settings.providers.some(function (item) {
            return item.name === entry.name;
        }))
            entry.name = base + " " + suffix++;
        const saved = settings.saveProvider(entry);
        if (saved) {
            selectProvider(saved);
            notice = qsTr("Fournisseur créé. Complétez sa configuration puis enregistrez.");
        }
    }
    function saveProvider() {
        if (providerKind === "gradio" && !qwenParameters.valid) { notice = qsTr("Vérifiez l’adaptateur LoRA et la graine."); return; }
        const saved = settings.saveProvider({
            id: providerId,
            name: providerName.text,
            kind: providerKind,
            url: serverUrl.text.trim(),
            model: modelName.text.trim(),
            keyEnv: environmentName.text.trim(),
            maxEdge: maxEdge.value,
            options: providerKind === "gradio" ? JSON.stringify(qwenParameters.options(options.text)) : options.text,
            username: username.text.trim(),
            authMode: authMode.currentIndex === 1 ? "none" : "basic",
            workflow: workflow.text,
            imageField: imageField.text,
            imageArray: imageArray.checked,
            promptMode: ["required", "optional", "none"][promptMode.currentIndex],
            nativeInput: nativeInput.checked
        });
        if (!saved) {
            notice = "";
            return;
        }
        providerId = saved;
        if (panel.basicAuth ? apiKey.text.length : apiKey.text.trim().length)
            settings.saveKey(saved, apiKey.text);
        else {
            notice = qsTr("Configuration enregistrée.");
            providerSaved(saved);
        }
    }
    function loadPrompt(value) {
        promptId = value.id || "";
        promptName.text = value.name || "";
        promptBody.text = value.text || "";
    }
    Component.onCompleted: {
        if (settings.providers.length)
            selectProvider(settings.providers[0].id);
        if (settings.prompts.length)
            loadPrompt(settings.prompts[0]);
    }
    Shortcut {
        sequences: [StandardKey.InsertParagraphSeparator]
        context: Qt.WindowShortcut
        enabled: panel.visible && tabs.currentIndex === 0 && panel.providerId.length > 0 && !panel.settings.credentialsBusy && !(panel.Window.window && panel.Window.window.activeFocusItem instanceof TextEdit)
        onActivated: panel.saveProvider()
    }
    Connections {
        target: panel.settings
        function onKeySaved(id, success) {
            if (id === panel.providerId && success) {
                apiKey.text = "";
                panel.notice = qsTr("Configuration et identifiant secret enregistrés.");
                panel.providerSaved(id);
            }
        }
    }
    Label {
        text: qsTr("Intelligence artificielle")
        font.pixelSize: 20
        font.bold: true
        color: "#eef2f5"
    }
    RowLayout {
        spacing: 6
        AiActionButton {
            text: qsTr("Fournisseurs")
            primary: tabs.currentIndex === 0
            onClicked: tabs.currentIndex = 0
        }
        AiActionButton {
            text: qsTr("Prompts")
            primary: tabs.currentIndex === 1
            onClicked: tabs.currentIndex = 1
        }
        Item {
            Layout.fillWidth: true
        }
    }
    StackLayout {
        id: tabs
        currentIndex: 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        RowLayout {
            spacing: 16
            ColumnLayout {
                Layout.preferredWidth: 195
                Layout.fillHeight: true
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Fournisseurs")
                        color: "#c5cfd8"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    AiActionButton {
                        objectName: "aiAddProvider"
                        enabled: !panel.settings.credentialsBusy
                        text: "+"
                        implicitWidth: 32
                        onClicked: addMenu.popup()
                    }
                }
                AiProviderList {
                    id: providerList
                    enabled: !panel.settings.credentialsBusy
                    objectName: "aiProviderList"
                    settings: panel.settings
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onActivated: function (id) {
                        panel.selectProvider(id);
                    }
                }
                AiActionButton {
                    text: qsTr("Supprimer")
                    Layout.fillWidth: true
                    enabled: panel.providerId.length > 0 && !panel.settings.credentialsBusy
                    onClicked: {
                        panel.settings.removeProvider(panel.providerId);
                        panel.providerId = "";
                        if (panel.settings.providers.length)
                            panel.selectProvider(panel.settings.providers[0].id);
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
                    text: panel.providerId ? qsTr("Configuration du fournisseur") : qsTr("Ajoutez un fournisseur avec le bouton +")
                    color: "#e6ebf0"
                    font.pixelSize: 15
                    font.bold: true
                }
                ScrollView {
                    id: form
                    enabled: !panel.settings.credentialsBusy
                    objectName: "aiProviderForm"
                    visible: panel.providerId.length > 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true
                    ColumnLayout {
                        width: form.availableWidth
                        spacing: 8
                        Label {
                            text: qsTr("Nom")
                            color: "#aab6c3"
                            font.pixelSize: 12
                        }
                        AiTextField {
                            id: providerName
                            objectName: "aiProviderName"
                            Layout.fillWidth: true
                            placeholderText: qsTr("Nom de cette configuration")
                        }
                        Label {
                            text: panel.providerKind === "gradio" ? "Qwen Image Edit · Gradio (JSON v2)" : panel.providerKind === "comfy" ? "ComfyUI · Local" : panel.providerKind === "fal" ? "fal.ai" : panel.providerKind === "openai" ? "OpenAI · GPT Image" : qsTr("API d’images compatible OpenAI")
                            color: "#899aa9"
                            font.pixelSize: 11
                        }
                        Label {
                            text: qsTr("Adresse du serveur")
                            color: "#aab6c3"
                            font.pixelSize: 12
                        }
                        AiTextField {
                            id: serverUrl
                            objectName: "aiProviderAddress"
                            Layout.fillWidth: true
                            placeholderText: "https://api.openai.com/v1"
                        }
                        Label {
                            text: panel.providerKind === "gradio" ? qsTr("Nom de l’API") : panel.providerKind === "comfy" ? qsTr("Checkpoint installé dans ComfyUI") : qsTr("Modèle")
                            color: "#aab6c3"
                            font.pixelSize: 12
                        }
                        AiTextField {
                            id: modelName
                            objectName: "aiProviderModel"
                            Layout.fillWidth: true
                            onEditingFinished: {
                                if (panel.upscaling) {
                                    promptMode.currentIndex = 2
                                    imageField.text = "image_url"
                                    imageArray.checked = false
                                    nativeInput.checked = true
                                }
                            }
                        }
                        Label {
                            visible: panel.providerKind === "gradio"
                            text: qsTr("Authentification")
                            color: "#aab6c3"
                        }
                        Basic.ComboBox {
                            id: authMode
                            objectName: "aiGradioAuthMode"
                            visible: panel.providerKind === "gradio"
                            Layout.fillWidth: true
                            model: [qsTr("HTTP Basic"), qsTr("Sans authentification")]
                            palette.buttonText: "#e0e7ee"; palette.button: "#2d3641"
                        }
                        Label { visible: panel.basicAuth; text: qsTr("Utilisateur HTTP Basic"); color: "#aab6c3" }
                        AiTextField {
                            id: username
                            objectName: "aiGradioUsername"
                            visible: panel.basicAuth
                            Layout.fillWidth: true
                            maximumLength: 256
                        }
                        QwenParameters {
                            id: qwenParameters
                            objectName: "aiQwenProviderParameters"
                            visible: panel.providerKind === "gradio"
                            Layout.fillWidth: true
                        }
                        Label {
                            visible: panel.providerKind !== "gradio" || panel.basicAuth
                            text: panel.basicAuth ? qsTr("Mot de passe HTTP Basic") : qsTr("Clé API")
                            color: "#aab6c3"
                            font.pixelSize: 12
                        }
                        AiTextField {
                            id: apiKey
                            visible: panel.providerKind !== "gradio" || panel.basicAuth
                            objectName: "aiApiKey"
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: {
                                const revision = panel.settings.revision;
                                if (panel.basicAuth) return panel.settings.hasKey(panel.providerId) ? qsTr("Mot de passe disponible · saisir pour le remplacer") : qsTr("Mot de passe du serveur Qwen")
                                return panel.settings.hasKey(panel.providerId) ? qsTr("Clé disponible · saisir pour la remplacer") : panel.settings.keySource(panel.providerId) === "unloaded" ? qsTr("Saisir une clé pour l’ajouter ou la remplacer") : qsTr("Coller la clé API ici");
                            }
                        }
                        Label {
                            objectName: "aiApiKeyStatus"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 11
                            color: "#91ad9c"
                            text: {
                                const revision = panel.settings.revision;
                                const source = panel.settings.keySource(panel.providerId);
                                if (panel.providerKind === "gradio") {
                                    if (!panel.basicAuth) return qsTr("Aucun identifiant n’est envoyé à ce serveur.");
                                    if (apiKey.text.length) return qsTr("Mot de passe saisi, pas encore enregistré.");
                                    return source === "stored" ? qsTr("Mot de passe enregistré dans le trousseau sécurisé.") : qsTr("Le mot de passe reste dans le trousseau, hors du JSON et des projets.");
                                }
                                if (apiKey.text.trim().length)
                                    return panel.settings.credentialsBusy ? qsTr("Enregistrement de la clé…") : qsTr("Clé saisie, pas encore enregistrée. Cliquez sur Enregistrer.");
                                if (source === "shared")
                                    return qsTr("Clé du même service : « %1 ».").arg(panel.settings.sharedKeyName(panel.providerId));
                                return source === "stored" ? qsTr("Clé enregistrée dans le trousseau sécurisé.") : source === "environment" ? qsTr("Clé fournie par la variable d’environnement.") : source === "unloaded" ? qsTr("Une clé déjà enregistrée sera chargée uniquement à la génération.") : panel.providerKind === "comfy" || panel.providerKind === "compatible" ? qsTr("Facultative pour un serveur local sans authentification.") : qsTr("La clé sera conservée dans le trousseau sécurisé du système.");
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: !nativeInput.checked
                            Label {
                                text: qsTr("Taille maximale d’envoi")
                                color: "#aab6c3"
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                            DarkSpinBox {
                                id: maxEdge
                                from: 256
                                to: 4096
                                value: 1536
                                stepSize: 256
                                suffix: " px"
                                Layout.preferredWidth: 130
                                implicitHeight: 32
                            }
                        }
                        Label {
                            visible: panel.providerKind === "fal" || panel.providerKind === "comfy"
                            text: qsTr("Prompt")
                            color: "#aab6c3"
                            font.pixelSize: 12
                        }
                        Basic.ComboBox {
                            id: promptMode
                            objectName: "aiProviderPromptMode"
                            visible: panel.providerKind === "fal" || panel.providerKind === "comfy"
                            enabled: !panel.upscaling
                            Layout.fillWidth: true
                            model: [qsTr("Obligatoire"), qsTr("Facultatif"), qsTr("Sans prompt")]
                            palette.buttonText: "#e0e7ee"
                            palette.button: "#2d3641"
                        }
                        Basic.CheckBox {
                            id: nativeInput
                            objectName: "aiProviderNativeInput"
                            text: qsTr("Envoyer à la résolution native")
                            palette.windowText: "#dce4ed"
                        }
                        AiActionButton {
                            text: advanced.visible ? qsTr("Masquer les options avancées") : qsTr("Options avancées…")
                            onClicked: advanced.visible = !advanced.visible
                        }
                        ColumnLayout {
                            id: advanced
                            visible: false
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                text: qsTr("Variable d’environnement (facultative)")
                                color: "#aab6c3"
                                font.pixelSize: 12
                            }
                            AiTextField {
                                id: environmentName
                                Layout.fillWidth: true
                                placeholderText: "OPENAI_API_KEY, FAL_KEY…"
                            }
                            Label {
                                visible: panel.providerKind !== "gradio"
                                text: qsTr("Paramètres du modèle (JSON)")
                                color: "#aab6c3"
                                font.pixelSize: 12
                            }
                            Basic.TextArea {
                                id: options
                                visible: panel.providerKind !== "gradio"
                                Layout.fillWidth: true
                                Layout.preferredHeight: 76
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                color: "#dde5ed"
                                font.pixelSize: 12
                                background: Rectangle {
                                    color: "#181e25"
                                    radius: 5
                                    border.color: "#3b4551"
                                }
                            }
                            RowLayout {
                                visible: panel.providerKind === "fal"
                                Layout.fillWidth: true
                                Label {
                                    text: qsTr("Champ image fal.ai")
                                    color: "#aab6c3"
                                    font.pixelSize: 12
                                }
                                AiTextField {
                                    id: imageField
                                    enabled: !panel.upscaling
                                    Layout.fillWidth: true
                                    text: "image_urls"
                                }
                                Basic.CheckBox {
                                    id: imageArray
                                    enabled: !panel.upscaling
                                    text: qsTr("Liste")
                                    checked: true
                                    palette.windowText: "#dce4ed"
                                }
                            }
                            Label {
                                visible: panel.providerKind === "comfy"
                                text: qsTr("Workflow ComfyUI au format API")
                                color: "#aab6c3"
                                font.pixelSize: 12
                            }
                            RowLayout {
                                visible: panel.providerKind === "comfy"
                                AiActionButton {
                                    text: qsTr("Charger…")
                                    onClicked: workflowFile.open()
                                }
                                AiActionButton {
                                    text: qsTr("Workflow par défaut")
                                    onClicked: workflow.text = panel.settings.newProvider("comfy").workflow
                                }
                            }
                            Basic.TextArea {
                                id: workflow
                                visible: panel.providerKind === "comfy"
                                Layout.fillWidth: true
                                Layout.preferredHeight: 180
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                color: "#dde5ed"
                                font.pixelSize: 12
                                background: Rectangle {
                                    color: "#181e25"
                                    radius: 5
                                    border.color: "#3b4551"
                                }
                            }
                            AiActionButton {
                                text: qsTr("Supprimer la clé enregistrée")
                                enabled: !panel.settings.credentialsBusy
                                onClicked: {
                                    panel.settings.removeKey(panel.providerId);
                                    apiKey.text = "";
                                }
                            }
                        }
                    }
                }
                Label {
                    objectName: "aiProviderFeedback"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    visible: text.length > 0
                    color: panel.settings.error ? "#eea39c" : "#a6c5af"
                    font.pixelSize: 12
                    text: panel.settings.error || panel.notice
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: panel.settings.credentialsBusy ? qsTr("Accès au trousseau…") : ""
                        color: "#92a2b1"
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                    AiActionButton {
                        objectName: "aiSaveProvider"
                        primary: true
                        text: qsTr("Enregistrer")
                        enabled: panel.providerId.length > 0 && !panel.settings.credentialsBusy
                        onClicked: panel.saveProvider()
                    }
                }
            }
        }
        RowLayout {
            spacing: 16
            ColumnLayout {
                Layout.preferredWidth: 195
                Layout.fillHeight: true
                RowLayout {
                    Label {
                        text: qsTr("Bibliothèque")
                        color: "#c5cfd8"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    AiActionButton {
                        text: "+"
                        implicitWidth: 32
                        onClicked: {
                            promptList.currentIndex = -1;
                            panel.loadPrompt({});
                        }
                    }
                }
                ListView {
                    id: promptList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: panel.settings.prompts
                    delegate: Basic.ItemDelegate {
                        required property var modelData
                        required property int index
                        width: promptList.width
                        height: 46
                        text: modelData.name
                        highlighted: modelData.id === panel.promptId
                        font.pixelSize: 13
                        palette.text: "#e5ebf1"
                        background: Rectangle {
                            radius: 5
                            color: parent.highlighted ? "#304137" : "#202730"
                            border.color: parent.highlighted ? "#8eb39a" : "#35404b"
                        }
                        onClicked: {
                            promptList.currentIndex = index;
                            panel.loadPrompt(modelData);
                        }
                    }
                }
                AiActionButton {
                    text: qsTr("Supprimer")
                    Layout.fillWidth: true
                    enabled: panel.promptId.length > 0
                    onClicked: {
                        panel.settings.removePrompt(panel.promptId);
                        panel.loadPrompt(panel.settings.prompts.length ? panel.settings.prompts[0] : ({}));
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
                Label {
                    text: qsTr("Nom du prompt")
                    color: "#aab6c3"
                    font.pixelSize: 12
                }
                AiTextField {
                    id: promptName
                    Layout.fillWidth: true
                    placeholderText: qsTr("Nom du prompt")
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Basic.TextArea {
                        id: promptBody
                        objectName: "aiPromptEditor"
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        color: "#dde5ed"
                        padding: 10
                        font.pixelSize: 13
                        placeholderText: qsTr("Décrivez les modifications à effectuer et ce qu’il faut conserver.")
                        background: Rectangle {
                            color: "#181e25"
                            radius: 5
                            border.color: "#3b4551"
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    color: "#eea39c"
                    text: panel.settings.error
                    visible: text.length > 0
                    wrapMode: Text.WordWrap
                }
                AiActionButton {
                    text: qsTr("Enregistrer le prompt")
                    primary: true
                    Layout.alignment: Qt.AlignRight
                    onClicked: {
                        const id = panel.settings.savePrompt({
                            id: panel.promptId,
                            name: promptName.text,
                            text: promptBody.text
                        });
                        if (id)
                            panel.promptId = id;
                    }
                }
            }
        }
    }
    DarkMenu {
        id: addMenu
        DarkMenuItem {
            text: "OpenAI · GPT Image"
            onTriggered: panel.createProvider("openai")
        }
        DarkMenuItem {
            text: "fal.ai"
            onTriggered: panel.createProvider("fal")
        }
        DarkMenuItem {
            objectName: "aiAddTopazProvider"
            text: "fal.ai · Topaz Upscale Precision"
            onTriggered: panel.createProvider("falTopaz")
        }
        DarkMenuItem {
            objectName: "aiAddEsrganProvider"
            text: "fal.ai · Real-ESRGAN"
            onTriggered: panel.createProvider("falEsrgan")
        }
        DarkMenuItem {
            objectName: "aiAddQwenProvider"
            text: "Qwen Edit 2.1 · Gradio"
            onTriggered: panel.createProvider("gradio")
        }
        DarkMenuItem {
            text: "ComfyUI · Local"
            onTriggered: panel.createProvider("comfy")
        }
        DarkMenuItem {
            text: qsTr("API compatible OpenAI")
            onTriggered: panel.createProvider("compatible")
        }
    }
    FileDialog {
        id: workflowFile
        title: qsTr("Workflow ComfyUI")
        nameFilters: ["JSON (*.json)"]
        onAccepted: {
            const text = panel.settings.readWorkflow(selectedFile);
            if (text.length)
                workflow.text = text;
        }
    }
}
