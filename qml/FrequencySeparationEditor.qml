import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "frequencySeparationEditor"
    property var parameters: ({})
    property bool busy: false
    property bool locked: false
    property bool prepared: false
    readonly property real radius: Number(prepared ? parameters._frequencyRadius : (parameters.radius === undefined ? 8 : parameters.radius))
    signal editStarted()
    signal previewed(string key, real value)
    signal committed(string key, real value)
    signal retouchRequested()
    signal radiusRestoreRequested()
    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Rayon du flou gaussien"); color: "#d5dbe0"; Layout.fillWidth: true }
        DarkSpinBox {
            objectName: "frequencyRadiusValue"
            from: 1; to: 100
            suffix: " px"
            value: root.radius
            enabled: !root.prepared && !root.busy && !root.locked
            onValueModified: {
                root.editStarted()
                root.committed("radius", value)
            }
        }
    }
    DarkSlider {
        objectName: "frequencyRadiusSlider"
        Layout.fillWidth: true
        from: 1; to: 100; stepSize: 1
        value: root.radius
        visible: !root.prepared
        enabled: !root.prepared && !root.busy && !root.locked
        onPressedChanged: {
            if (pressed) root.editStarted()
            else root.committed("radius", value)
        }
        onMoved: root.previewed("radius", value)
    }
    Label {
        Layout.fillWidth: true
        text: root.prepared ? qsTr("Les pixels sont prêts à être retouchés. Revenir au réglage du rayon restaure les fréquences d’origine et abandonne leurs retouches. Cette opération est annulable.") : qsTr("Choisissez le rayon, puis préparez les pixels pour retoucher chaque fréquence. Le calcul se fait en arrière-plan. Le rayon sera figé ; Édition > Annuler restaure la paire dynamique.")
        wrapMode: Text.WordWrap
        color: "#a8b4bf"
    }
    DialogActionButton {
        objectName: "frequencyRetouchButton"
        text: qsTr("Préparer la retouche")
        visible: !root.prepared
        Layout.alignment: Qt.AlignRight
        enabled: !root.busy && !root.locked
        onClicked: root.retouchRequested()
    }
    DialogActionButton {
        objectName: "frequencyRestoreRadiusButton"
        text: qsTr("Revenir au réglage du rayon…")
        visible: root.prepared
        Layout.alignment: Qt.AlignRight
        enabled: !root.busy && !root.locked && Boolean(root.parameters._frequencyPairId)
        onClicked: root.radiusRestoreRequested()
    }
    Label {
        Layout.fillWidth: true
        text: qsTr("Basses fréquences : couleurs et volumes, avec le pinceau ou le flou. Hautes fréquences : texture, avec le tampon de duplication en échantillonnant le calque actif. L’image d’origine reste intacte.")
        wrapMode: Text.WordWrap
        color: "#91a0ad"
    }
}
