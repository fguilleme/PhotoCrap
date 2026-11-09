import QtQuick

DropArea {
    id: root
    required property var document
    property bool validDrop: false
    signal fileDropped(url file)
    keys: ["text/uri-list"]

    function accepts(drag) {
        // There is one document slot: do not replace it repeatedly for a batch.
        return drag.hasUrls && drag.urls.length === 1 && document.canImportLayerFiles(drag.urls)
    }
    onEntered: function(drag) {
        validDrop = accepts(drag)
        drag.accepted = validDrop
    }
    onExited: validDrop = false
    onDropped: function(drop) {
        if (!validDrop || !accepts(drop)) return
        drop.accept(Qt.CopyAction)
        validDrop = false
        fileDropped(drop.urls[0])
    }
    Rectangle {
        objectName: "documentImageDropIndicator"
        anchors.fill: parent
        anchors.margins: 3
        visible: root.validDrop && root.containsDrag
        color: "#183c7254"
        border.width: 2
        border.color: "#bce8c8"
        radius: 6
        Rectangle {
            anchors.centerIn: parent
            width: label.implicitWidth + 32
            height: label.implicitHeight + 24
            radius: 6
            color: "#26372f"
            Text {
                id: label
                anchors.centerIn: parent
                text: qsTr("Déposer pour ouvrir l’image")
                color: "#e4f6e8"
                font.pixelSize: 14
            }
        }
    }
}
