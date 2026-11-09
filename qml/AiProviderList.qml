import QtQuick
import QtQuick.Controls.Basic as Basic

ListView {
    id: list
    required property var settings
    property string selectedId: ""
    signal activated(string providerId)
    model: settings.providers
    currentIndex: Math.max(0, model.findIndex(function (entry) {
        return entry.id === list.selectedId;
    }))
    keyNavigationEnabled: false
    spacing: 6
    clip: true
    implicitHeight: 210
    Keys.onDownPressed: if (currentIndex + 1 < count) {
        selectedId = model[currentIndex + 1].id;
        activated(selectedId);
    }
    Keys.onUpPressed: if (currentIndex > 0) {
        selectedId = model[currentIndex - 1].id;
        activated(selectedId);
    }
    delegate: Basic.ItemDelegate {
        required property var modelData
        required property int index
        width: list.width
        implicitHeight: 66
        leftPadding: 12
        rightPadding: 10
        highlighted: index === list.currentIndex
        Accessible.name: modelData.name
        onClicked: {
            list.selectedId = modelData.id;
            list.activated(modelData.id);
        }
        background: Rectangle {
            radius: 6
            color: parent.highlighted ? "#304137" : parent.hovered ? "#2a323d" : "#202730"
            border.color: parent.highlighted ? "#8eb39a" : "#35404b"
        }
        contentItem: Column {
            spacing: 5
            Text {
                width: parent.width
                text: modelData.name
                color: "#edf1f5"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: {
                    const revision = list.settings.revision;
                    if (modelData.kind === "comfy")
                        return qsTr("ComfyUI · Local");
                    const source = list.settings.keySource(modelData.id);
                    return source === "shared" ? qsTr("Clé partagée · %1").arg(list.settings.sharedKeyName(modelData.id)) : source === "stored" ? qsTr("Clé enregistrée") : source === "environment" ? qsTr("Clé via environnement") : modelData.kind === "compatible" ? qsTr("Serveur compatible") : qsTr("Clé à renseigner");
                }
                font.pixelSize: 11
                color: "#a9b8b0"
                elide: Text.ElideRight
            }
        }
    }
    Basic.ScrollBar.vertical: Basic.ScrollBar {}
}
