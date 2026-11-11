import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel
    objectName: "meshyProgress"
    required property var controller
    visible: controller.busy || controller.status.length > 0 || controller.error.length > 0
    height: content.implicitHeight + 24
    radius: 9
    color: "#25312d"
    border.color: controller.error.length ? "#df9a80" : "#a1c8ae"
    border.width: 2

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        cursorShape: Qt.ArrowCursor
    }
    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Meshy · Calque 3D")
                color: "#f2f8f3"
                font.bold: true
                font.pixelSize: 15
            }
            DialogActionButton {
                text: panel.controller.busy ? qsTr("Arrêter le suivi") : qsTr("Fermer")
                ToolTip.visible: hovered && panel.controller.busy
                ToolTip.text: qsTr("La génération peut continuer chez Meshy.")
                onClicked: {
                    if (panel.controller.busy)
                        panel.controller.cancel()
                    else
                        panel.controller.dismiss()
                }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: panel.controller.error || panel.controller.status
            color: panel.controller.error.length ? "#ffc9b9" : "#eef5f0"
        }
        ProgressBar {
            Layout.fillWidth: true
            visible: panel.controller.busy
            to: 100
            value: panel.controller.progress
            indeterminate: value === 0
        }
    }
}
