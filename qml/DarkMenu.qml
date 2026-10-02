import QtQuick
import QtQuick.Controls

Menu {
    id: control

    padding: 5
    delegate: DarkMenuItem { }
    palette.window: "#20252c"
    palette.windowText: "#dbe1e8"
    palette.base: "#20252c"
    palette.text: "#dbe1e8"
    palette.button: "#20252c"
    palette.buttonText: "#dbe1e8"
    palette.highlight: "#2866c8"
    palette.highlightedText: "#ffffff"

    background: Rectangle {
        implicitWidth: 220
        color: "#20252c"
        border.color: "#4a535e"
        border.width: 1
        radius: 7
    }
}
