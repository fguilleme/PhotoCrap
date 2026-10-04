import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "selectiveColorEditorRoot"
    property var parameters: ({})
    property int selectedFamily: 0
    readonly property string activePrefix: families[selectedFamily].key
    readonly property int controlCount: controls.count
    signal editStarted()
    signal previewed(string key, var value)
    signal committed(string key, var value)
    spacing: 11

    property var families: [
        {key:"reds", name:qsTr("Rouges"), color:"#d94c47"},
        {key:"yellows", name:qsTr("Jaunes"), color:"#d7bc3d"},
        {key:"greens", name:qsTr("Verts"), color:"#43aa5b"},
        {key:"cyans", name:qsTr("Cyans"), color:"#35b5c2"},
        {key:"blues", name:qsTr("Bleus"), color:"#4976cc"},
        {key:"magentas", name:qsTr("Magentas"), color:"#c653a6"},
        {key:"whites", name:qsTr("Blancs"), color:"#dedede"},
        {key:"neutrals", name:qsTr("Neutres"), color:"#888b8e"},
        {key:"blacks", name:qsTr("Noirs"), color:"#25282c"}
    ]

    function parameterKey(suffix) {
        return activePrefix + suffix
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("Choisissez les couleurs de l’image à corriger.")
        color: "#9eabb6"
        wrapMode: Text.WordWrap
    }

    GridLayout {
        objectName: "selectiveColorFamilyGrid"
        Layout.fillWidth: true
        columns: 3
        columnSpacing: 6
        rowSpacing: 6
        Repeater {
            model: root.families
            delegate: Rectangle {
                required property var modelData
                required property int index
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: 5
                color: root.selectedFamily === index ? "#35433e" : "#272e36"
                border.width: root.selectedFamily === index ? 2 : 1
                border.color: root.selectedFamily === index ? "#8fc9a4" : "#46515c"
                RowLayout {
                    anchors.fill: parent; anchors.margins: 6; spacing: 6
                    Rectangle {
                        width: 14; height: 14; radius: 7; color: modelData.color
                        border.width: 1; border.color: "#c7cdd1"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData.name
                        color: "#dce3e8"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.selectedFamily = index
                }
            }
        }
    }

    Repeater {
        id: controls
        model: [
            {suffix:"CyanRed", label:qsTr("Cyan ↔ Rouge"), left:"#21b9c8", right:"#db4c46"},
            {suffix:"MagentaGreen", label:qsTr("Magenta ↔ Vert"), left:"#cd4ca8", right:"#48ad5d"},
            {suffix:"YellowBlue", label:qsTr("Jaune ↔ Bleu"), left:"#d8bf3d", right:"#4478ce"},
            {suffix:"Black", label:qsTr("Blanc ↔ Noir"), left:"#e4e4e4", right:"#24282d"}
        ]
        delegate: ColumnLayout {
            required property var modelData
            Layout.fillWidth: true; spacing: 2
            RowLayout {
                Layout.fillWidth: true
                Label { text: modelData.label; color: "#d5dbe0"; Layout.fillWidth: true }
                Label {
                    text: slider.value > 0 ? "+" + Math.round(slider.value) : Math.round(slider.value)
                    color: "#aab4bd"; Layout.preferredWidth: 42
                    horizontalAlignment: Text.AlignRight
                }
            }
            Slider {
                id: slider
                Layout.fillWidth: true; Layout.preferredHeight: 25
                from: -100; to: 100; stepSize: 1
                value: root.parameters[root.parameterKey(modelData.suffix)] === undefined ? 0 :
                       Number(root.parameters[root.parameterKey(modelData.suffix)])
                background: Rectangle {
                    x: slider.leftPadding
                    y: slider.topPadding + (slider.availableHeight - height) / 2
                    width: slider.availableWidth; height: 9; radius: 4
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: modelData.left }
                        GradientStop { position: .5; color: "#888d91" }
                        GradientStop { position: 1; color: modelData.right }
                    }
                    border.width: 1; border.color: "#cfd4d8"
                }
                handle: Rectangle {
                    x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                    y: slider.topPadding + (slider.availableHeight - height) / 2
                    width: 18; height: 18; radius: 9
                    color: "#eef2f4"; border.width: 2; border.color: "#20262d"
                }
                onPressedChanged: {
                    if (pressed) root.editStarted()
                    else root.committed(root.parameterKey(modelData.suffix), value)
                }
                onMoved: root.previewed(root.parameterKey(modelData.suffix), value)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Méthode"); color: "#d5dbe0"; Layout.fillWidth: true }
        RadioButton {
            objectName: "selectiveColorRelative"
            text: qsTr("Relative")
            checked: root.parameters.relative === undefined || Boolean(root.parameters.relative)
            onClicked: {
                root.editStarted()
                root.previewed("relative", true)
                root.committed("relative", true)
            }
        }
        RadioButton {
            objectName: "selectiveColorAbsolute"
            text: qsTr("Absolue")
            checked: root.parameters.relative !== undefined && !Boolean(root.parameters.relative)
            onClicked: {
                root.editStarted()
                root.previewed("relative", false)
                root.committed("relative", false)
            }
        }
    }
}
