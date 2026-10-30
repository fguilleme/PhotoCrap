import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    signal previewRequested(int index)
    signal previewCancelled()
    signal presetChosen(int index)
    property bool previewActive: false
    property int lastPreviewIndex: -1
    property bool accepting: false

    // Outside clicks dismiss the menu before another control can start editing.
    popup.modal: true
    popup.onAboutToShow: { lastPreviewIndex = -1; accepting = false }
    onHighlighted: function(index) { previewIndex(index) }
    onActivated: function(index) {
        accepting = true
        previewActive = false
        presetChosen(index)
    }
    popup.onClosed: cancelPreview()
    onVisibleChanged: if (!visible) { cancelPreview(); popup.close() }
    onEnabledChanged: if (!enabled) { cancelPreview(); popup.close() }

    delegate: ItemDelegate {
        required property int index
        required property var modelData
        objectName: "presetOption" + index
        width: control.popup.width
        text: control.textRole ? modelData[control.textRole] : modelData
        font: control.font
        palette: control.palette
        highlighted: control.highlightedIndex === index
        hoverEnabled: true
        onHoveredChanged: if (hovered) control.previewIndex(index)
    }

    function previewIndex(index) {
        if (!visible || !enabled || !popup.visible || accepting || index < 0 || index === lastPreviewIndex) return
        lastPreviewIndex = index
        previewActive = true
        previewRequested(index)
    }

    function cancelPreview() {
        if (!previewActive) return
        previewActive = false
        previewCancelled()
    }
}
