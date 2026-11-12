import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "meshyDialog"
    required property var controller
    required property var settings
    required property bool hasSelection
    readonly property string keySource: (settings.revision, settings.keySource("meshy"))
    readonly property bool recovering: tabs.currentIndex === 1

    anchors.centerIn: parent
    width: Math.min(560, parent.width - 32)
    height: Math.min(implicitHeight, parent.height - 32)
    padding: 18
    title: qsTr("Calque 3D · Meshy")
    modal: true

    onOpened: {
        tabs.currentIndex = 0
        source.currentIndex = hasSelection ? 2 : 0
        description.clear()
        taskId.clear()
        imageTask.checked = true
        key.clear()
        remember.checked = true
        ultra.checked = false
        limitTriangles.checked = false
        triangles.value = 1000000
        if (source.currentIndex === 0)
            description.forceActiveFocus()
    }
    onClosed: key.clear()

    background: Rectangle {
        color: "#20262e"
        border.color: "#46535f"
        radius: 8
    }

    header: Item {
        implicitHeight: 48
        Label {
            anchors.left: parent.left
            anchors.leftMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            text: dialog.title
            color: "#f2f6f8"
            font.pixelSize: 17
            font.bold: true
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: "#39434e"
        }
    }

    component MeshyTab: TabButton {
        id: tab
        implicitHeight: 36
        contentItem: Text {
            text: tab.text
            color: tab.checked ? "#e7f4ec" : "#b7c4cf"
            font.pixelSize: 13
            font.bold: tab.checked
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 5
            color: tab.checked ? "#31463d" : tab.hovered ? "#303944" : "#272f38"
            border.color: tab.activeFocus || tab.checked ? "#82aa93" : "#39444f"
            border.width: tab.activeFocus ? 2 : 1
        }
    }

    contentItem: ColumnLayout {
        spacing: 14

        TabBar {
            id: tabs
            objectName: "meshyTabs"
            Layout.fillWidth: true
            spacing: 6
            background: Item {}
            onCurrentIndexChanged: {
                if (dialog.opened) {
                    if (dialog.recovering)
                        taskId.forceActiveFocus()
                    else
                        source.forceActiveFocus()
                }
            }
            MeshyTab { text: qsTr("Générer") }
            MeshyTab { text: qsTr("Récupérer") }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitHeight: form.implicitHeight
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: form
                width: scroll.availableWidth
                spacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: !dialog.recovering
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Source") }
                        ComboBox {
                            id: source
                            objectName: "meshySource"
                            Accessible.name: qsTr("Source")
                            Layout.fillWidth: true
                            model: dialog.hasSelection
                                   ? [qsTr("Description"), qsTr("Image du document"), qsTr("Sélection")]
                                   : [qsTr("Description"), qsTr("Image du document")]
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 94
                        visible: source.currentIndex === 0
                        background: Rectangle {
                            color: "#181e25"
                            border.color: description.activeFocus ? "#8eb39a" : "#3b4551"
                            radius: 5
                        }
                        TextArea {
                            id: description
                            objectName: "meshyPrompt"
                            placeholderText: qsTr("Décrivez l’objet à générer")
                            Accessible.name: placeholderText
                            color: "#e5eaf0"
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            background: null
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        color: "#a5b2c0"
                        wrapMode: Text.WordWrap
                        text: qsTr("La génération et la texture utilisent des crédits Meshy.")
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: dialog.recovering
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Type de tâche"); Layout.fillWidth: true }
                        ButtonGroup { id: taskTypes }
                        RadioButton {
                            id: imageTask
                            text: qsTr("Image")
                            ButtonGroup.group: taskTypes
                        }
                        RadioButton {
                            text: qsTr("Texte")
                            ButtonGroup.group: taskTypes
                        }
                    }
                    AiTextField {
                        id: taskId
                        objectName: "meshyTaskId"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Identifiant de la tâche Meshy")
                        Accessible.name: placeholderText
                        maximumLength: 128
                        validator: RegularExpressionValidator { regularExpression: /^[A-Za-z0-9_-]{1,128}$/ }
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#a5b2c0"
                        text: qsTr("Récupère le résultat d’une tâche déjà lancée sur le même compte Meshy.")
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: meshOptions.implicitHeight + 22
                    radius: 6
                    color: "#252e37"
                    border.color: "#39454f"

                    ColumnLayout {
                        id: meshOptions
                        anchors.fill: parent
                        anchors.margins: 11
                        spacing: 6

                        Label { text: qsTr("Maillage"); font.bold: true }
                        CheckBox {
                            id: ultra
                            visible: !dialog.recovering
                            text: qsTr("Géométrie Ultra 4K (+5 crédits Meshy)")
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            CheckBox {
                                id: limitTriangles
                                objectName: "meshyLimitTriangles"
                                Layout.fillWidth: true
                                text: qsTr("Réduire le maillage à l’import")
                            }
                            DarkSpinBox {
                                id: triangles
                                objectName: "meshyTriangleTarget"
                                Layout.preferredWidth: 144
                                visible: limitTriangles.checked
                                from: 100
                                to: 8000000
                                stepSize: 100000
                                Accessible.name: qsTr("Triangles cibles")
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: limitTriangles.checked
                            wrapMode: Text.WordWrap
                            color: "#b1bfca"
                            text: qsTr("PhotoCrap vise ce nombre de triangles et préserve les détails fins. Le GLB source reste dans le projet.")
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#39434e"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Clé API Meshy") }
                        AiTextField {
                            id: key
                            objectName: "meshyApiKey"
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            Accessible.name: qsTr("Clé API Meshy")
                            placeholderText: dialog.keySource === "environment"
                                             ? qsTr("Clé fournie par MESHY_API_KEY")
                                             : dialog.keySource === "stored"
                                             ? qsTr("Clé du trousseau · saisir pour la remplacer")
                                             : qsTr("Saisissez votre clé API")
                        }
                    }
                    CheckBox {
                        id: remember
                        visible: key.text.length > 0
                        text: qsTr("Enregistrer la clé dans le trousseau")
                    }
                }
            }
        }
    }

    footer: DialogButtonBox {
        padding: 12
        background: Rectangle { color: "transparent" }
        DialogActionButton {
            text: qsTr("Annuler")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: dialog.reject()
        }
        DialogActionButton {
            objectName: "meshyGenerate"
            text: dialog.recovering ? qsTr("Récupérer") : qsTr("Générer")
            defaultButton: !description.activeFocus
            enabled: !dialog.controller.busy
                     && (key.text.trim().length > 0 || dialog.keySource === "environment" || dialog.keySource === "stored")
                     && (dialog.recovering ? taskId.acceptableInput
                         : source.currentIndex !== 0 || description.text.trim().length > 0
                           && description.text.trim().length <= 800)
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: {
                const target = limitTriangles.checked ? triangles.value : 0
                const started = dialog.recovering
                    ? dialog.controller.recover(taskId.text, !imageTask.checked, key.text, remember.checked, target)
                    : dialog.controller.generate(source.currentIndex, description.text, key.text, remember.checked,
                                                 ultra.checked, target)
                if (started)
                    dialog.close()
            }
        }
    }
}
