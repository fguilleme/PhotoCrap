import QtQuick

DropArea {
    id: root
    required property var document
    required property var documentManager
    property bool validDrop: false
    signal filesDropped(var files)
    keys: ["text/uri-list"]

    function accepts(drag) {
        return drag.hasUrls && documentManager.canOpenFiles(drag.urls)
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
        filesDropped(drop.urls)
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
