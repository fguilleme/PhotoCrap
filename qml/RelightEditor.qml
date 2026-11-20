import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "Studio.js" as Studio

ColumnLayout {
    id: root
    objectName: "relightEditor"
    required property var photos
    required property int activeLayerIndex
    required property bool canRefresh
    required property var parameters
    required property var lightToCanvas
    readonly property var lights: parameters.lights || []
    property int selected: 0
    readonly property int index: Math.min(selected, lights.length - 1)
    readonly property var lamp: lights[index] || {}
    function canvasPoint(lamp) {
        return lightToCanvas ? lightToCanvas.times(Qt.vector3d(lamp.x, lamp.y, 0)) : Qt.vector3d(0, 0, 0)
    }
    function placeOnCanvas(x, y, finished, target) {
        if (!lightToCanvas) return
        const local = lightToCanvas.inverted().times(Qt.vector3d(x, y, 0))
        if (target) {
            change({targetX: Math.max(0, Math.min(100, local.x)),
                    targetY: Math.max(0, Math.min(100, local.y))}, finished)
            return
        }
        change({x: Math.max(-100, Math.min(200, local.x)), y: Math.max(-100, Math.min(200, local.y))}, finished)
    }
    signal refreshPhoto(int photoIndex)
    signal ambientEdited(real value, bool finished)
    signal editStarted()
    signal edited(var lights, bool finished)
    function change(values, finished) {
        if (index < 0) return
        const list = lights.slice()
        list[index] = Object.assign({}, lamp, values)
        edited(list, finished)
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Photo"); color: Studio.secondary }
        ComboBox {
            objectName: "relightPhoto"
            Layout.fillWidth: true
            Accessible.name: qsTr("Photo à éclairer")
            enabled: root.canRefresh && root.photos.length > 0
            model: root.photos
            textRole: "name"
            currentIndex: root.photos.findIndex(p => p.visible && p.layerIndex < root.activeLayerIndex)
            onActivated: i => root.refreshPhoto(root.photos[i].layerIndex)
        }
        Button {
            objectName: "relightRefresh"
            enabled: root.canRefresh
            text: qsTr("Recalculer le relief")
            onClicked: root.refreshPhoto(-1)
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Ambiance"); color: Studio.secondary; Layout.preferredWidth: 95 }
        DarkSlider {
            id: ambient
            objectName: "relightAmbient"
            Layout.fillWidth: true
            Accessible.name: qsTr("Lumière ambiante")
            from: 0; to: 100; stepSize: 1
            value: root.parameters.ambient ?? 100
            onMoved: root.ambientEdited(value, !pressed)
            onPressedChanged: { if (pressed) root.editStarted(); else root.ambientEdited(value, true) }
        }
        Label { text: Math.round(ambient.value) + " %"; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight }
    }
    RowLayout {
        ComboBox {
            Layout.fillWidth: true
            Accessible.name: qsTr("Source lumineuse")
            model: root.lights.map((_, i) => qsTr("Lumière %1").arg(i + 1))
            currentIndex: root.index
            onActivated: i => root.selected = i
        }
        Button {
            objectName: "relightAdd"
            text: "+"; Accessible.name: qsTr("Ajouter une lumière")
            enabled: root.lights.length < 16
            onClicked: {
                const list = root.lights.slice()
                list.push({x:65,y:35,z:65,color:"#ffffff",intensity:100,softness:45})
                root.edited(list,true)
                root.selected = list.length - 1
            }
        }
        Button {
            objectName: "relightRemove"
            text: "−"; Accessible.name: qsTr("Retirer la lumière")
            enabled: root.index >= 0
            onClicked: {
                const list = root.lights.slice()
                list.splice(root.index, 1)
                root.selected = Math.max(0, root.index - 1)
                root.edited(list,true)
            }
        }
    }
    Button {
        visible: root.index >= 0
        text: qsTr("Couleur de la lumière…")
        leftPadding: 31
        Rectangle { x: 8; anchors.verticalCenter: parent.verticalCenter; width: 15; height: 15; radius: 3; color: root.lamp.color || "white"; border.color: Studio.line }
        onClicked: { picker.selectedColor = root.lamp.color; picker.open() }
    }
    CheckBox {
        objectName: "relightAim"
        visible: root.index >= 0
        text: qsTr("Diriger vers une cible")
        checked: !!root.lamp.aimed
        onClicked: root.change({aimed: checked, targetX: root.lamp.targetX ?? 50,
                                targetY: root.lamp.targetY ?? 50}, true)
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Déplacez le viseur sur la zone à éclairer.")
    }
    ColorDialog {
        id: picker
        onAccepted: root.change({color:selectedColor.toString()},true)
    }
    Repeater {
        model: root.index < 0 ? [] : [
            {key:"x", label:qsTr("Position X"), low:-100, high:200},
            {key:"y", label:qsTr("Position Y"), low:-100, high:200},
            {key:"z", label:qsTr("Profondeur Z"), low:-100, high:200},
            {key:"intensity", label:qsTr("Intensité"), low:0, high:400},
            {key:"softness", label:qsTr("Douceur"), low:0, high:100}
        ]
        delegate: RowLayout {
            required property var modelData
            Layout.fillWidth: true
            Label { text: modelData.label; Layout.preferredWidth: 95; color: Studio.secondary }
            DarkSlider {
                id: slider
                Layout.fillWidth: true
                Accessible.name: modelData.label
                from: modelData.low; to: modelData.high; stepSize: 1
                value: Number(root.lamp[modelData.key] || 0)
                function send(finalValue) {
                    const change = {}; change[modelData.key] = value
                    root.change(change,finalValue)
                }
                onMoved: send(!pressed)
                onPressedChanged: { if (pressed) root.editStarted(); else send(true) }
            }
            Label { text: Math.round(slider.value)+" %"; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight }
        }
    }
}
