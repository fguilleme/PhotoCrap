import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as Native
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "model3dPanel"
    required property var document
    required property var layerInfo
    readonly property bool hasModel: layerInfo.kind === "model3d" && !layerInfo.editingMask
    readonly property bool poseEditable: hasModel && !layerInfo.locked
    readonly property bool busy: document.pixelEditBusy || document.rawLoading || document.transformActive
    readonly property bool colorPickerOpen: lightPicker.visible
    property bool dragging: false
    property bool canceledDrag: false
    property bool committing: false
    property var loadedDocument
    property string loadedLayerId
    property bool ready: false
    property var initialPose: ({})
    property var initialLight: ({})
    readonly property var poseValues: ({
        x: currentPoseValue("x", positionX.value), y: currentPoseValue("y", positionY.value),
        scale: currentPoseValue("scale", sizeControl.value), pitch: currentPoseValue("pitch", pitch.value),
        yaw: currentPoseValue("yaw", yaw.value), roll: currentPoseValue("roll", roll.value)
    })
    readonly property var lightValues: ({
        azimuth: currentLightValue("azimuth", azimuth.value),
        elevation: currentLightValue("elevation", elevation.value),
        intensity: currentLightValue("intensity", intensity.value),
        ambient: currentLightValue("ambient", ambient.value),
        color: lightColor.text.trim().toLowerCase()
    })
    readonly property bool modified: hasModel && (Object.keys(poseValues).some(key => poseValues[key] !== initialPose[key])
        || Object.keys(lightValues).some(key => lightValues[key] !== initialLight[key]))
    signal closeRequested()

    color: "#242a32"
    clip: true
    Component.onCompleted: {
        ready = true
        refresh()
    }
    onLayerInfoChanged: if (ready) refresh()
    onDocumentChanged: if (ready) refresh()
    onVisibleChanged: if (ready && !visible) cancelGesture()

    Connections {
        target: root.document
        function onOperationChanged() {
            if (root.committing && !root.document.operationBusy) {
                root.committing = false
                root.document.cancelModel3DPreview()
                root.refresh(true)
            }
        }
    }

    function currentPoseValue(key, value) {
        const original = initialPose[key]
        return poseEditable && value !== Math.round(original) ? value : original
    }

    function currentLightValue(key, value) {
        const original = initialLight[key]
        return value !== Math.round(original) ? value : original
    }

    function refresh(force) {
        if (loadedDocument && loadedDocument !== document)
            loadedDocument.cancelModel3DPreview()
        if (layerInfo.kind !== "model3d" || layerInfo.editingMask) {
            if (loadedLayerId.length)
                document.cancelModel3DPreview()
            loadedDocument = document
            loadedLayerId = ""
            return
        }
        const layerId = document.layerIdentity(layerInfo.layerIndex)
        const pose = layerInfo.parameters.pose3d
        const light = layerInfo.parameters.light3d
        if (!force && loadedDocument === document && loadedLayerId === layerId
                && JSON.stringify(initialPose) === JSON.stringify(pose)
                && JSON.stringify(initialLight) === JSON.stringify(light))
            return

        if (loadedLayerId !== layerId && dragging) {
            document.cancelModel3DPreview()
            canceledDrag = true
        }
        loadedDocument = document
        loadedLayerId = layerId
        initialPose = pose
        initialLight = light
        positionX.value = Math.round(pose.x)
        positionY.value = Math.round(pose.y)
        sizeControl.value = Math.round(pose.scale)
        pitch.value = Math.round(pose.pitch)
        yaw.value = Math.round(pose.yaw)
        roll.value = Math.round(pose.roll)
        azimuth.value = Math.round(light.azimuth)
        elevation.value = Math.round(light.elevation)
        intensity.value = Math.round(light.intensity)
        ambient.value = Math.round(light.ambient)
        lightColor.text = light.color
    }

    function canChange() {
        return modified && !busy && lightColor.acceptableInput && loadedDocument === document
            && loadedLayerId === document.layerIdentity(layerInfo.layerIndex)
    }

    function previewChanges() {
        if (!busy && hasModel && lightColor.acceptableInput && loadedDocument === document
                && loadedLayerId === document.layerIdentity(layerInfo.layerIndex))
            document.previewModel3D(layerInfo.layerIndex, poseValues, lightValues)
    }

    function commitChanges() {
        if (busy || committing)
            return
        if (canChange()) {
            committing = true
            if (document.editModel3D(layerInfo.layerIndex, poseValues, lightValues)
                    && document.operationBusy)
                return
            committing = false
        }
        document.cancelModel3DPreview()
        refresh(true)
    }

    function cancelGesture() {
        if (!dragging)
            return false
        canceledDrag = true
        document.cancelModel3DPreview()
        refresh(true)
        return true
    }

    component ModelSlider: RowLayout {
        id: row
        required property string caption
        property alias from: slider.from
        property alias to: slider.to
        property alias value: slider.value
        property alias controlName: slider.objectName
        Layout.fillWidth: true
        spacing: 5

        Label {
            text: row.caption
            Layout.preferredWidth: Math.min(106, root.width * 0.38)
            elide: Text.ElideRight
        }
        DarkSlider {
            id: slider
            Layout.fillWidth: true
            Layout.minimumWidth: 40
            Layout.preferredHeight: 30
            Accessible.name: row.caption
            stepSize: 1
            snapMode: Slider.SnapAlways
            onMoved: {
                if (pressed) {
                    if (!root.canceledDrag)
                        root.previewChanges()
                } else {
                    root.commitChanges()
                }
            }
            onPressedChanged: {
                if (pressed) {
                    root.canceledDrag = false
                    root.dragging = true
                } else {
                    root.dragging = false
                    if (root.canceledDrag)
                        root.refresh(true)
                    else
                        root.commitChanges()
                }
            }
        }
        Label {
            text: Number(slider.value).toLocaleString(Qt.locale(), "f", 0)
            Layout.preferredWidth: Math.max(40, implicitWidth)
            horizontalAlignment: Text.AlignRight
        }
    }

    component SectionTab: TabButton {
        id: tab
        required property int page
        readonly property bool selected: tabs.currentIndex === page
        background: Rectangle {
            color: tab.selected ? "#465463" : tab.hovered ? "#343e49" : "#252c34"
            border.color: tab.selected ? "#8194a8" : "#3a444f"
            radius: 4
        }
    }

    Native.ColorDialog {
        id: lightPicker
        title: qsTr("Couleur de la lumière")
        onAccepted: {
            lightColor.text = selectedColor.toString()
            root.commitChanges()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 5

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: "#2c343e"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 4
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Calque 3D · %1").arg(root.layerInfo.name || "")
                    elide: Text.ElideRight
                    font.bold: true
                }
                ToolButton {
                    objectName: "model3dCloseButton"
                    text: "×"
                    implicitWidth: 28
                    implicitHeight: 28
                    Accessible.name: qsTr("Fermer les propriétés 3D")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.refresh(true)
                        root.closeRequested()
                    }
                }
            }
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            SectionTab { page: 0; text: qsTr("Pose") }
            SectionTab { page: 1; objectName: "model3dLightingTab"; text: qsTr("Éclairage") }
        }

        Flickable {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            clip: true
            contentWidth: width
            contentHeight: controls.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            interactive: !root.dragging
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: controls
                width: scroll.width - (scroll.contentHeight > scroll.height ? 8 : 0)
                enabled: root.hasModel && !root.busy

                ColumnLayout {
                    visible: tabs.currentIndex === 0
                    Layout.fillWidth: true
                    spacing: 4
                    enabled: root.poseEditable
                    ModelSlider {
                        id: positionX
                        controlName: "model3dX"
                        caption: qsTr("Centre X (px)")
                        from: Math.min(-Math.round(root.document.width / 2), Math.floor(root.initialPose.x || 0))
                        to: Math.max(Math.round(root.document.width * 1.5), Math.ceil(root.initialPose.x || 0))
                    }
                    ModelSlider {
                        id: positionY
                        caption: qsTr("Centre Y (px)")
                        from: Math.min(-Math.round(root.document.height / 2), Math.floor(root.initialPose.y || 0))
                        to: Math.max(Math.round(root.document.height * 1.5), Math.ceil(root.initialPose.y || 0))
                    }
                    ModelSlider { id: sizeControl; caption: qsTr("Échelle (%)"); from: 1; to: 1000 }
                    ModelSlider { id: pitch; caption: qsTr("Rotation X (°)"); from: -180; to: 180 }
                    ModelSlider { id: yaw; controlName: "model3dYaw"; caption: qsTr("Rotation Y (°)"); from: -180; to: 180 }
                    ModelSlider { id: roll; caption: qsTr("Rotation Z (°)"); from: -180; to: 180 }
                }

                ColumnLayout {
                    visible: tabs.currentIndex === 1
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: qsTr("Éclairage commun"); font.bold: true }
                    ModelSlider { id: azimuth; caption: qsTr("Azimut (°)"); from: -180; to: 180 }
                    ModelSlider { id: elevation; caption: qsTr("Élévation (°)"); from: -90; to: 90 }
                    ModelSlider { id: intensity; controlName: "model3dIntensity"; caption: qsTr("Intensité (%)"); from: 0; to: 200 }
                    ModelSlider { id: ambient; caption: qsTr("Ambiante (%)"); from: 0; to: 100 }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Couleur"); Layout.fillWidth: true }
                        Button {
                            implicitWidth: 35
                            Accessible.name: qsTr("Choisir la couleur de la lumière")
                            onClicked: {
                                lightPicker.selectedColor = lightColor.acceptableInput ? lightColor.text : "#ffffff"
                                lightPicker.open()
                            }
                            contentItem: Rectangle {
                                color: lightColor.acceptableInput ? lightColor.text : "transparent"
                                radius: 2
                                border.color: "#74808b"
                            }
                        }
                        TextField {
                            id: lightColor
                            Layout.preferredWidth: 82
                            Layout.minimumWidth: 0
                            maximumLength: 7
                            placeholderText: "#ffffff"
                            onEditingFinished: {
                                if (acceptableInput)
                                    root.commitChanges()
                                else
                                    text = root.initialLight.color
                            }
                            validator: RegularExpressionValidator { regularExpression: /^#[0-9a-fA-F]{6}$/ }
                        }
                    }
                }
            }
        }

    }
}
