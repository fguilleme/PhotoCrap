import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var document
    signal editCommitted()
    readonly property var options: document.transformOptions
    readonly property bool warping: options.mode === "warp"
    implicitHeight: 46
    color: "#242a32"

    component TransformField: RowLayout {
        id: field
        required property string label
        required property string parameter
        property string unit: ""
        spacing: 4
        Label { text: field.label; color: "#aeb9c5" }
        TextField {
            id: entry
            objectName: "transformField_" + field.parameter
            Layout.preferredWidth: 66
            selectByMouse: true
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: DoubleValidator {
                bottom: -1000000; top: 1000000; decimals: 3
                notation: DoubleValidator.StandardNotation
            }
            Binding on text {
                when: !entry.activeFocus
                value: Number(root.options[field.parameter] || 0).toLocaleString(Qt.locale(), "f", 2)
                restoreMode: Binding.RestoreNone
            }
            onAccepted: {
                const number = Number.fromLocaleString(Qt.locale(), text)
                root.document.setTransformParameter(field.parameter, number)
                root.editCommitted()
            }
        }
        Label { visible: field.unit.length > 0; text: field.unit; color: "#aeb9c5" }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            contentWidth: controls.implicitWidth
            contentHeight: availableHeight
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
            ScrollBar.horizontal.policy: ScrollBar.AsNeeded
            clip: true

            RowLayout {
                id: controls
                height: Math.max(0, root.height - 5)
                spacing: 7
                ComboBox {
                    objectName: "transformMode"
                    Layout.preferredWidth: 160
                    readonly property var modes: [
                        { label: qsTr("Transformation manuelle"), value: "free" },
                        { label: qsTr("Échelle"), value: "scale" },
                        { label: qsTr("Rotation"), value: "rotate" },
                        { label: qsTr("Inclinaison"), value: "skew" }
                    ].concat(root.options.allowProjective ? [
                        { label: qsTr("Distorsion"), value: "distort" },
                        { label: qsTr("Perspective"), value: "perspective" }
                    ] : []).concat(root.options.allowWarp ? [
                        { label: qsTr("Déformation"), value: "warp" }
                    ] : [])
                    model: modes
                    textRole: "label"
                    currentIndex: modes.findIndex(function(mode) { return mode.value === root.options.mode })
                    onActivated: {
                        root.document.setTransformMode(modes[currentIndex].value)
                        root.editCommitted()
                    }
                }
                CheckBox {
                    visible: !root.warping
                    text: qsTr("Pivot")
                    checked: root.options.showReference === true
                    onToggled: root.document.setTransformParameter("showReference", checked)
                }
                Grid {
                    visible: !root.warping && root.options.showReference === true
                    columns: 3
                    spacing: 2
                    Repeater {
                        model: 9
                        delegate: Rectangle {
                            required property int index
                            readonly property point position: Qt.point((index % 3) / 2, Math.floor(index / 3) / 2)
                            width: 9; height: 9; radius: 1
                            color: root.options.reference && root.options.reference.x === position.x
                                   && root.options.reference.y === position.y ? "#b6dfff" : "#454f5d"
                            TapHandler { onTapped: root.document.setTransformParameter("reference", parent.position) }
                        }
                    }
                }
                TransformField { visible: !root.warping; label: "X"; parameter: "x"; unit: "px" }
                TransformField { visible: !root.warping; label: "Y"; parameter: "y"; unit: "px" }
                TransformField { visible: !root.warping; label: qsTr("L"); parameter: "width"; unit: "%" }
                ToolButton {
                    visible: !root.warping
                    text: "⛓"
                    checkable: true
                    checked: root.options.linked === true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Conserver les proportions")
                    Accessible.name: ToolTip.text
                    onToggled: root.document.setTransformParameter("linked", checked)
                }
                TransformField { visible: !root.warping; label: qsTr("H"); parameter: "height"; unit: "%" }
                TransformField { visible: !root.warping; label: "∠"; parameter: "angle"; unit: "°" }
                ComboBox {
                    visible: root.warping
                    Layout.preferredWidth: 140
                    readonly property var presets: [
                        { label: qsTr("Personnalisée"), value: "custom" },
                        { label: qsTr("Arc"), value: "arc" },
                        { label: qsTr("Arc inférieur"), value: "arcLower" },
                        { label: qsTr("Arc supérieur"), value: "arcUpper" },
                        { label: qsTr("Voûte"), value: "arch" },
                        { label: qsTr("Renflement"), value: "bulge" },
                        { label: qsTr("Drapeau"), value: "flag" },
                        { label: qsTr("Onde"), value: "wave" }
                    ]
                    model: presets
                    textRole: "label"
                    currentIndex: presets.findIndex(function(preset) { return preset.value === root.options.preset })
                    onActivated: {
                        root.document.setTransformParameter("preset", presets[currentIndex].value)
                        root.editCommitted()
                    }
                }
                ComboBox {
                    visible: root.warping
                    Layout.preferredWidth: 135
                    readonly property var sizes: [1, 3, 4, 5]
                    model: [qsTr("Grille par défaut"), "3 × 3", "4 × 4", "5 × 5", qsTr("Personnalisée…")]
                    currentIndex: root.options.columns === root.options.rows && sizes.includes(root.options.columns)
                                  ? sizes.indexOf(root.options.columns) : 4
                    onActivated: {
                        if (currentIndex === 4) gridDialog.open()
                        else {
                            const size = sizes[currentIndex]
                            root.document.setTransformParameter("grid", Qt.point(size, size))
                            root.editCommitted()
                        }
                    }
                }
                TransformField { visible: root.warping && root.options.preset !== "custom"; label: qsTr("Inflexion"); parameter: "bend"; unit: "%" }
                CheckBox {
                    visible: root.warping && root.options.preset !== "custom"
                    text: qsTr("Verticale")
                    checked: root.options.vertical === true
                    onToggled: root.document.setTransformParameter("vertical", checked)
                }
                Button {
                    visible: root.warping
                    text: qsTr("Scinder")
                    onClicked: splitMenu.popup()
                    DarkMenu {
                        id: splitMenu
                        DarkMenuItem { text: qsTr("Horizontalement"); onTriggered: root.document.setTransformParameter("split", "horizontal") }
                        DarkMenuItem { text: qsTr("Verticalement"); onTriggered: root.document.setTransformParameter("split", "vertical") }
                        DarkMenuItem { text: qsTr("En croix"); onTriggered: root.document.setTransformParameter("split", "cross") }
                    }
                }
            }
        }
        ToolButton {
            text: "×"; objectName: "cancelTransformButton"
            ToolTip.visible: hovered; ToolTip.text: qsTr("Annuler la transformation (Échap)")
            Accessible.name: ToolTip.text
            onClicked: { root.document.cancelTransform(); root.editCommitted() }
        }
        ToolButton {
            text: "✓"; objectName: "applyTransformButton"
            ToolTip.visible: hovered; ToolTip.text: qsTr("Appliquer la transformation (Entrée)")
            Accessible.name: ToolTip.text
            onClicked: { root.document.applyTransform(); root.editCommitted() }
        }
    }

    Dialog {
        id: gridDialog
        title: qsTr("Grille de déformation")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay
        onOpened: {
            gridColumns.value = Math.max(1, root.options.columns)
            gridRows.value = Math.max(1, root.options.rows)
        }
        onAccepted: {
            root.document.setTransformParameter("grid", Qt.point(gridColumns.value, gridRows.value))
            root.editCommitted()
        }
        contentItem: GridLayout {
            columns: 2
            Label { text: qsTr("Colonnes") }
            SpinBox { id: gridColumns; from: 1; to: 12; editable: true }
            Label { text: qsTr("Lignes") }
            SpinBox { id: gridRows; from: 1; to: 12; editable: true }
        }
    }
}
