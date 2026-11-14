import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Rectangle {
    id: panel
    required property var controller
    property bool notice: false
    property bool wasRunning: false
    readonly property bool active: controller.busy || controller.pendingResult || controller.surfacePicking
    readonly property bool succeeded: !active && !controller.error.length && controller.completedVariants === controller.variantCount
    objectName: "aiProgressPanel"
    visible: active || notice
    implicitHeight: content.implicitHeight + 32
    height: implicitHeight
    radius: 9
    color: "#25312d"
    border.color: controller.error.length ? "#df9a80" : "#a1c8ae"
    border.width: 2
    onControllerChanged: {
        notice = false;
        wasRunning = false;
        closeTimer.stop();
    }
    // The card intercepts clicks only inside its own bounds. The canvas stays editable.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        cursorShape: Qt.ArrowCursor
    }
    Timer {
        id: closeTimer
        interval: 8000
        onTriggered: panel.notice = false
    }
    Connections {
        target: panel.controller
        function onChanged() {
            if (panel.active) {
                panel.wasRunning = true;
                panel.notice = false;
                closeTimer.stop();
            } else if (panel.wasRunning) {
                panel.wasRunning = false;
                panel.notice = true;
                if (!panel.controller.error.length)
                    closeTimer.restart();
            }
        }
    }
    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                width: 36
                height: 36
                radius: 7
                color: "#a1c8ae"
                Text {
                    anchors.centerIn: parent
                    text: qsTr("IA")
                    color: "#20362a"
                    font.pixelSize: 14
                    font.bold: true
                }
            }
            ColumnLayout {
                spacing: 3
                Layout.fillWidth: true
                Basic.Label {
                    objectName: "aiProgressTitle"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: panel.controller.surfaceAnalysis ? qsTr("Peinture sur surface")
                          : panel.controller.busy ? qsTr("Création IA en cours")
                          : panel.controller.pendingResult ? qsTr("Résultat IA prêt à ajouter")
                          : panel.controller.error.length ? qsTr("La génération IA s’est arrêtée")
                          : panel.succeeded ? qsTr("Création IA terminée") : qsTr("Génération IA interrompue")
                    color: "#f2f8f3"
                    font.pixelSize: 17
                    font.bold: true
                }
                Basic.Label {
                    objectName: "aiProgressProvider"
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAnywhere
                    text: panel.controller.providerName + " · " + panel.controller.modelName
                    color: "#c7d8cc"
                    font.pixelSize: 12
                }
            }
            Basic.Label {
                objectName: "aiProgressElapsed"
                readonly property int seconds: panel.controller.elapsedSeconds
                text: qsTr("Écoulé %1:%2").arg(Math.floor(seconds / 60).toString().padStart(2, "0")).arg((seconds % 60).toString().padStart(2, "0"))
                color: "#dceadf"
                font.pixelSize: 13
            }
        }
        Basic.Label {
            objectName: "aiProgressStage"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: panel.controller.error || panel.controller.status
            color: panel.controller.error.length ? "#ffc9b9" : "#eef5f0"
            font.pixelSize: 14
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
        Item {
            objectName: "aiProgressActivity"
            Layout.fillWidth: true
            implicitHeight: 7
            visible: panel.controller.busy
            clip: true
            Rectangle {
                anchors.fill: parent
                radius: 3
                color: "#46564c"
            }
            Rectangle {
                id: pulse
                width: parent.width * 0.32
                height: parent.height
                radius: 3
                color: "#b5dfc3"
                NumberAnimation on x {
                    from: -pulse.width
                    to: pulse.parent.width
                    duration: 1400
                    loops: Animation.Infinite
                    running: panel.visible && panel.controller.busy
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            RowLayout {
                visible: !panel.controller.surfaceAnalysis && panel.width >= 580
                spacing: 4
                Repeater {
                    model: panel.controller.variantCount
                    Rectangle {
                        required property int index
                        width: 18
                        height: 6
                        radius: 2
                        color: index < panel.controller.completedVariants ? "#b5dfc3" : "#4b5c51"
                    }
                }
            }
            Basic.Label {
                text: panel.controller.surfaceAnalysis ? "" : qsTr("%1 / %2 variante(s) créée(s)").arg(panel.controller.completedVariants).arg(panel.controller.variantCount)
                color: "#c7d8cc"
                font.pixelSize: 12
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            AiActionButton {
                objectName: "aiProgressAddResult"
                primary: true
                text: qsTr("Ajouter le résultat")
                visible: panel.controller.pendingResult && !panel.controller.busy
                onClicked: panel.controller.retryInsertion()
            }
            AiActionButton {
                objectName: "aiProgressCancel"
                text: panel.active ? qsTr("Annuler") : qsTr("Fermer")
                onClicked: {
                    if (panel.active)
                        panel.controller.cancel();
                    else
                        panel.notice = false;
                }
            }
        }
    }
}
