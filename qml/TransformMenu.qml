import QtQuick
import QtQuick.Controls

DarkMenu {
    id: root
    required property var document
    property Action againAction
    property var startTransform: function(mode) { document.beginTransform(mode) }
    property bool contextMenu: false
    readonly property var options: document.transformOptions

    title: qsTr("Transformation")
    DarkMenuItem { visible: !!root.againAction; action: root.againAction }
    MenuSeparator { visible: !!root.againAction }
    DarkMenuItem { visible: root.contextMenu; text: qsTr("Transformation manuelle"); onTriggered: root.startTransform("free") }
    DarkMenuItem { text: qsTr("Échelle"); onTriggered: root.startTransform("scale") }
    DarkMenuItem { text: qsTr("Rotation"); onTriggered: root.startTransform("rotate") }
    DarkMenuItem { text: qsTr("Inclinaison"); onTriggered: root.startTransform("skew") }
    DarkMenuItem { text: qsTr("Distorsion"); enabled: !root.document.transformActive || root.options.allowProjective === true; onTriggered: root.startTransform("distort") }
    DarkMenuItem { text: qsTr("Perspective"); enabled: !root.document.transformActive || root.options.allowProjective === true; onTriggered: root.startTransform("perspective") }
    MenuSeparator {}
    DarkMenuItem { text: qsTr("Déformation"); enabled: !root.document.transformActive || root.options.allowWarp === true; onTriggered: root.startTransform("warp") }
    DarkMenuItem { text: qsTr("Scinder horizontalement"); enabled: root.options.mode === "warp"; onTriggered: root.document.setTransformParameter("split", "horizontal") }
    DarkMenuItem { text: qsTr("Scinder verticalement"); enabled: root.options.mode === "warp"; onTriggered: root.document.setTransformParameter("split", "vertical") }
    DarkMenuItem { text: qsTr("Scinder en croix"); enabled: root.options.mode === "warp"; onTriggered: root.document.setTransformParameter("split", "cross") }
    DarkMenuItem { text: qsTr("Supprimer la subdivision"); enabled: root.options.mode === "warp" && root.options.node >= 0; onTriggered: root.document.transformCommand("removeSplit") }
    MenuSeparator {}
    DarkMenuItem { text: qsTr("Convertir le point d’ancrage"); enabled: root.options.mode === "warp" && root.options.node >= 0; onTriggered: root.document.transformCommand("corner") }
    DarkMenuItem { text: qsTr("Afficher les repères"); enabled: root.options.mode === "warp"; checkable: true; checked: root.options.showGuides === true; onTriggered: root.document.setTransformParameter("showGuides", checked) }
    MenuSeparator {}
    DarkMenuItem { text: qsTr("Rotation de 180°"); onTriggered: root.document.transformCommand("rotate180") }
    DarkMenuItem { text: qsTr("Rotation de 90° à droite"); onTriggered: root.document.transformCommand("rotateCW") }
    DarkMenuItem { text: qsTr("Rotation de 90° à gauche"); onTriggered: root.document.transformCommand("rotateCCW") }
    MenuSeparator {}
    DarkMenuItem { text: qsTr("Symétrie horizontale"); onTriggered: root.document.transformCommand("flipX") }
    DarkMenuItem { text: qsTr("Symétrie verticale"); onTriggered: root.document.transformCommand("flipY") }
}
