import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ScrollView {
    id: panel
    required property var document
    required property var settings
    property var usage: ({})
    property var planeUsage: ({})
    contentWidth: availableWidth
    clip: true
    function refresh() {
        const current = document.scratchCacheUsage()
        if (!current.busy) {
            usage = current
            planeUsage = document.memoryUsage()
        }
    }
    onVisibleChanged: if (visible) refresh()
    Component.onCompleted: refresh()
    Timer { interval: 1000; repeat: true; running: panel.visible; onTriggered: panel.refresh() }
    Connections { target: panel.settings; function onScratchCacheChanged() { panel.refresh() } }

    ColumnLayout {
    width: panel.availableWidth
    spacing: 14
    Label { text: qsTr("Cache de travail"); color: "#eef2f5"; font.pixelSize: 20; font.bold: true }
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Cache graphique"); color: "#d3dbe2"; Layout.fillWidth: true }
        DarkSpinBox {
            objectName: "textureCacheBudget"
            from: 0; to: 1024; value: panel.settings.textureCacheMiB; suffix: qsTr("Mio")
            Accessible.name: qsTr("Budget du cache graphique")
            onValueModified: panel.settings.textureCacheMiB = value
        }
    }
    Label {
        Layout.fillWidth: true
        text: qsTr("Conserve les zones récemment affichées pour accélérer les déplacements. 0 désactive cette réserve. La zone visible reste prioritaire et peut dépasser le budget.")
        color: "#a8b5c1"; wrapMode: Text.WordWrap
    }
    Label {
        Layout.fillWidth: true
        text: qsTr("Conserve les rendus de fréquences et les tuiles des calques, masques et états d’historique sur disque. Les opérations qui utilisent encore une image complète réservent un tampon temporaire.")
        color: "#a8b5c1"; wrapMode: Text.WordWrap
    }
    Rectangle { Layout.fillWidth: true; height: 1; color: "#39424c" }
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Budget disque"); color: "#d3dbe2"; Layout.fillWidth: true }
        DarkSpinBox {
            objectName: "scratchDiskBudget"
            from: 0; to: 64; value: panel.settings.scratchDiskGiB; suffix: qsTr("Gio")
            Accessible.name: qsTr("Budget du cache disque")
            onValueModified: panel.settings.scratchDiskGiB = value
        }
    }
    Label {
        Layout.fillWidth: true
        text: qsTr("0 désactive le cache. Une réserve de 512 Mio d’espace libre est conservée. Le cache RAM de fréquences reste limité à 128 Mio.")
        color: "#95a2af"; wrapMode: Text.WordWrap
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Tuiles en mémoire"); color: "#d3dbe2"; Layout.fillWidth: true }
        DarkSpinBox {
            objectName: "planeRamBudget"
            from: 0; to: 4096; value: panel.settings.planeRamMiB; suffix: qsTr("Mio")
            Accessible.name: qsTr("Budget RAM des tuiles")
            onValueModified: panel.settings.planeRamMiB = value
        }
    }
    Label {
        Layout.fillWidth: true
        text: qsTr("Objectif pour les tuiles hors des opérations en cours. Le déchargement se fait après une pause ; un disque indisponible conserve les données en mémoire. Ce budget ne couvre pas tous les tampons de l’application.")
        color: "#95a2af"; wrapMode: Text.WordWrap
    }
    Label {
        objectName: "tileRamStatistics"
        Layout.fillWidth: true
        text: qsTr("Tuiles RAM : %1 Mio · objectif %2 Mio").arg(
                  ((panel.planeUsage.tileHeapBytes || 0) / 1048576).toFixed(1)).arg(panel.settings.planeRamMiB)
        color: "#c9d4dc"; wrapMode: Text.WordWrap
    }
    Label { text: qsTr("Dossier de travail"); color: "#d3dbe2" }
    Label {
        Layout.fillWidth: true
        text: panel.usage.directory || qsTr("Dossier de cache système")
        color: "#b9c6d1"; wrapMode: Text.WrapAnywhere
        objectName: "scratchCacheDirectory"
    }
    RowLayout {
        DialogActionButton { text: qsTr("Choisir…"); onClicked: folder.open() }
        DialogActionButton {
            text: qsTr("Dossier par défaut")
            enabled: panel.settings.scratchDirectory.length > 0
            onClicked: panel.settings.scratchDirectory = ""
        }
    }
    Label {
        Layout.fillWidth: true
        objectName: "scratchCacheStatistics"
        text: qsTr("Utilisé : %1 Mio · %2 blocs · %3 réutilisations").arg(
                  ((panel.usage.diskBytes || 0) / 1048576).toFixed(1)).arg(
                  panel.usage.entries || 0).arg(panel.usage.hits || 0)
        color: "#c9d4dc"; wrapMode: Text.WordWrap
    }
    DialogActionButton {
        objectName: "clearScratchCacheButton"
        text: qsTr("Vider le cache")
        onClicked: { panel.document.clearScratchCache(); panel.refresh() }
    }
    Label {
        Layout.fillWidth: true
        text: qsTr("Les blocs encore utilisés par un rendu restent disponibles jusqu’à sa fin. Ce cache temporaire n’est pas une sauvegarde ; les projets .pcr restent indépendants.")
        color: "#95a2af"; wrapMode: Text.WordWrap
    }
    Label {
        Layout.fillWidth: true
        visible: text.length > 0
        text: panel.usage.error || ""
        color: "#e9b27b"; wrapMode: Text.WordWrap
    }
    }
    FolderDialog {
        id: folder
        title: qsTr("Choisir le dossier du cache de travail")
        onAccepted: panel.settings.setScratchDirectoryUrl(selectedFolder)
    }
}
