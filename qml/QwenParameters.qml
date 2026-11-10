import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

ColumnLayout {
    id: root
    readonly property var adapters: ["Multiple-Angles", "Photo-to-Anime", "Anime-V2", "Light-Migration", "Upscaler", "Style-Transfer", "Manga-Tone", "Anything2Real", "Fal-Multiple-Angles", "Polaroid-Photo", "Unblur-Anything", "Midnight-Noir-Eyes-Spotlight", "Hyper-Realistic-Portrait", "Ultra-Realistic-Portrait", "Pixar-Inspired-3D", "Noir-Comic-Book", "Any-light", "Studio-DeLight", "Cinematic-FlatLog"]
    readonly property bool valid: lora.editText.trim().length > 0 && (randomize.checked || (seed.text.trim().length > 0 && Number.isSafeInteger(Number(seed.text)) && Number(seed.text) >= 0))
    spacing: 6
    function load(text) {
        let value = ({})
        try { value = JSON.parse(text || "{}") } catch (e) {}
        const adapter = value.lora_adapter === undefined ? "Photo-to-Anime" : value.lora_adapter
        lora.currentIndex = adapters.indexOf(adapter)
        lora.editText = adapter
        seed.text = String(value.seed === undefined ? 42 : value.seed)
        randomize.checked = value.randomize_seed !== false
        guidance.value = Math.round(Number(value.guidance_scale === undefined ? 1 : value.guidance_scale) * 100)
        steps.value = Number(value.steps || 4)
    }
    function options(base) {
        let value = ({})
        try { value = JSON.parse(base || "{}") } catch (e) {}
        value.lora_adapter = lora.editText.trim()
        value.seed = Number.isSafeInteger(Number(seed.text)) && Number(seed.text) >= 0 ? Number(seed.text) : 42
        value.randomize_seed = randomize.checked
        value.guidance_scale = guidance.value / 100
        value.steps = steps.value
        return value
    }
    GridLayout {
        columns: 2
        columnSpacing: 12
        Layout.fillWidth: true
        Basic.Label { text: qsTr("Adaptateur LoRA"); color: "#aab6c3" }
        Basic.ComboBox {
            id: lora
            objectName: "qwenLora"
            Layout.fillWidth: true
            editable: true
            model: root.adapters
            currentIndex: 1
            implicitHeight: 32
            palette.buttonText: "#e0e7ee"; palette.button: "#2d3641"
            palette.text: "#e0e7ee"; palette.base: "#202730"
        }
        Basic.Label { text: qsTr("Graine"); color: "#aab6c3" }
        RowLayout {
            Layout.fillWidth: true
            AiTextField {
                id: seed
                objectName: "qwenSeed"
                Layout.fillWidth: true
                enabled: !randomize.checked
                text: "42"
                maximumLength: 16
                inputMethodHints: Qt.ImhDigitsOnly
                validator: RegularExpressionValidator { regularExpression: /[0-9]{1,16}/ }
            }
            Basic.CheckBox {
                id: randomize
                objectName: "qwenRandomizeSeed"
                text: qsTr("Aléatoire")
                checked: true
                palette.windowText: "#dce4ed"
            }
        }
        Basic.Label { text: qsTr("Guidance"); color: "#aab6c3" }
        RowLayout {
            Layout.fillWidth: true
            DarkSpinBox {
                id: guidance
                objectName: "qwenGuidance"
                from: 0; to: 10000; value: 100; stepSize: 10
                Layout.fillWidth: true
                textFromValue: function(value, locale) { return Number(value / 100).toLocaleString(locale, "f", 2) }
                valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text.trim()) * 100) }
            }
            Basic.Label { text: qsTr("Étapes"); color: "#aab6c3" }
            DarkSpinBox {
                id: steps
                objectName: "qwenSteps"
                from: 1; to: 100; value: 4
                Layout.fillWidth: true
            }
        }
    }
    Basic.Label {
        visible: !root.valid
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: lora.editText.trim().length === 0 ? qsTr("Choisissez un adaptateur LoRA.") : qsTr("Indiquez une graine entière entre 0 et 9 007 199 254 740 991.")
        color: "#eea39c"
    }
}
