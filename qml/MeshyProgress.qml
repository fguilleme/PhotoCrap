import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel
    objectName: "meshyProgress"
    required property var controller
    readonly property var result: controller.resultInfo
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
                objectName: "meshyResume"
                visible: panel.controller.canResume
                text: qsTr("Reprendre le suivi")
                onClicked: panel.controller.resume()
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
        Label {
            Layout.fillWidth: true
            visible: !!panel.result.triangles
            wrapMode: Text.WordWrap
            color: "#dce9e2"
            text: visible ? qsTr("%1 triangles · %2 sommets · GLB source %3")
                .arg(Number(panel.result.triangles).toLocaleString(Qt.locale(), "f", 0))
                .arg(Number(panel.result.vertices).toLocaleString(Qt.locale(), "f", 0))
                .arg(panel.result.fileSize) : ""
        }
        Label {
            Layout.fillWidth: true
            visible: !!panel.result.decodedSize
            wrapMode: Text.WordWrap
            color: "#b9ccc0"
            text: visible ? qsTr("Modèle décodé %1").arg(panel.result.decodedSize) : ""
        }
        Label {
            Layout.fillWidth: true
            visible: panel.result.triangleTarget > 0 && panel.result.sourceTriangles > panel.result.triangleTarget
            wrapMode: Text.WordWrap
            color: "#b9ccc0"
            text: visible ? qsTr("Source %1 triangles · objectif %2")
                .arg(Number(panel.result.sourceTriangles).toLocaleString(Qt.locale(), "f", 0))
                .arg(Number(panel.result.triangleTarget).toLocaleString(Qt.locale(), "f", 0)) : ""
        }
        ListView {
            id: textureList
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(count * 20, 80)
            visible: count > 0
            clip: true
            model: panel.result.textures || []
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Label {
                required property var modelData
                width: textureList.width - 12
                height: 20
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                color: "#b9ccc0"
                text: modelData.width === modelData.loadedWidth && modelData.height === modelData.loadedHeight
                    ? qsTr("Texture %1 × %2 px · %3").arg(modelData.width).arg(modelData.height).arg(modelData.size)
                    : qsTr("Texture %1 × %2 px · %3 · chargée en %4 × %5 px")
                        .arg(modelData.width).arg(modelData.height).arg(modelData.size)
                        .arg(modelData.loadedWidth).arg(modelData.loadedHeight)
            }
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
