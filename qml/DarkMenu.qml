import QtQuick
import QtQuick.Controls
import "Studio.js" as Studio

Menu {
    id: control

    padding: 5
    delegate: DarkMenuItem { }
    palette.window: Studio.panel
    palette.windowText: Studio.ink
    palette.base: Studio.panel
    palette.text: Studio.ink
    palette.button: Studio.panel
    palette.buttonText: Studio.ink
    palette.highlight: Studio.selected
    palette.highlightedText: Studio.ink

    background: Rectangle {
        implicitWidth: 220
        color: Studio.panel
        border.color: Studio.line
        border.width: 1
        radius: 7
    }
}
