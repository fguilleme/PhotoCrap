import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    property var samples: []
    readonly property bool sampler: document.tool === "colorSampler"
    signal clearSamples()
    color: "#20262e"
    implicitHeight: 44
    ColumnLayout {
        anchors.fill: parent; anchors.leftMargin:18; anchors.rightMargin:18
        spacing:0
        RowLayout {
            Layout.preferredHeight:44; Layout.fillWidth:true
            spacing:8
            ToolIcon { toolName: root.document.tool; Layout.preferredWidth:24; Layout.preferredHeight:24 }
            Label { text:qsTr("Taille de l’échantillon");color:"#aebbc8" }
            ComboBox {
                objectName:"eyedropperSampleSize"
                readonly property var sizes:[1,3,5,11,31,51,101]
                model:[qsTr("Échantillon ponctuel"),qsTr("Moyenne 3 × 3"),qsTr("Moyenne 5 × 5"),qsTr("Moyenne 11 × 11"),qsTr("Moyenne 31 × 31"),qsTr("Moyenne 51 × 51"),qsTr("Moyenne 101 × 101")]
                currentIndex:sizes.indexOf(root.document.eyedropperSize)
                implicitWidth:185;implicitHeight:30
                onActivated:root.document.eyedropperSize=sizes[currentIndex]
            }
            Label { text:qsTr("Échantillon");color:"#aebbc8" }
            ComboBox {
                objectName:"eyedropperSampleSource"
                model:[qsTr("Tous les calques"),qsTr("Calque actif")]
                currentIndex:root.document.eyedropperSource
                implicitWidth:160;implicitHeight:30
                onActivated:root.document.eyedropperSource=currentIndex
            }
            CheckBox {
                objectName:"eyedropperShowRing"
                visible:!root.sampler
                text:qsTr("Afficher l’anneau de prélèvement")
                checked:root.document.eyedropperRing
                onToggled:root.document.eyedropperRing=checked
            }
            Button {
                objectName:"clearColorSamplers"
                visible:root.sampler
                enabled:root.samples.length>0
                text:qsTr("Tout effacer");implicitHeight:30
                onClicked:root.clearSamples()
            }
            Label {
                text:root.sampler ? qsTr("%1 / 4 échantillons").arg(root.samples.length)
                                 : qsTr("Option / Alt : couleur d’arrière-plan · Maj : ajouter un échantillon")
                color:"#87929e";Layout.fillWidth:true;elide:Text.ElideRight
            }
        }
    }
}
