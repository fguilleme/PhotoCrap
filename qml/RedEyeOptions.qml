import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    color: "#20262e"
    implicitHeight: 44

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 8
        Label { text: qsTr("Taille de pupille"); color: "#aebbc8" }
        DarkSpinBox {
            objectName: "redEyePupilSize"
            from: 1; to: 100; suffix: " %"; implicitWidth: 112
            value: Math.round(root.document.redEyePupilSize * 100)
            onValueModified: root.document.redEyePupilSize = value / 100
        }
        Label { text: qsTr("Obscurcissement"); color: "#aebbc8"; Layout.leftMargin: 12 }
        DarkSpinBox {
            objectName: "redEyeDarken"
            from: 0; to: 100; suffix: " %"; implicitWidth: 112
            value: Math.round(root.document.redEyeDarken * 100)
            onValueModified: root.document.redEyeDarken = value / 100
        }
        Label {
            text: qsTr("Cliquez sur la pupille rouge ou encadrez un œil")
            color: "#87929e"
            Layout.fillWidth: true
            Layout.leftMargin: 12
            elide: Text.ElideRight
        }
    }
}
