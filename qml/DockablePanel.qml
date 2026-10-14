import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property string panelId
    required property string title
    property Component contentComponent
    property bool collapsed: false
    property bool floating: false
    property bool panelShown: true
    property bool temporarilyHidden: false
    property bool hideOnClose: false
    property bool applicationClosing: false
    property int dockedHeight: 150
    property int minimumContentHeight: 48
    readonly property int headerHeight: 34
    property string tabTitle: title
    property var groupTabs: []
    property string activeGroupPanel: panelId
    property string dropIndicator: ""
    property alias floatingX: floatingWindow.x
    property alias floatingY: floatingWindow.y
    property alias floatingWidth: floatingWindow.width
    property alias floatingHeight: floatingWindow.height

    signal dropRequested(real globalX, real globalY, real horizontalDelta, real verticalDelta)
    signal dragUpdated(real globalX, real globalY)
    signal dragFinished()
    signal groupTabSelected(string panelId)
    signal ungroupRequested(string panelId)
    signal floatingChangedByUser(bool floating)
    signal layoutChanged()
    signal hideRequested()

    function setPanelFloating(value) {
        floating = value
        if (value) {
            if (!temporarilyHidden) floatingWindow.show()
            frame.parent = floatingWindow.contentItem
        } else {
            frame.parent = root
            floatingWindow.hide()
        }
    }

    implicitHeight: floating ? 0 : (collapsed ? headerHeight : dockedHeight)
    visible: panelShown && !temporarilyHidden && !floating && (groupTabs.length <= 1 || activeGroupPanel === panelId)

    onTemporarilyHiddenChanged: {
        if (floating) {
            if (temporarilyHidden) floatingWindow.hide()
            else floatingWindow.show()
        }
    }
    onCollapsedChanged: layoutChanged()
    onDockedHeightChanged: layoutChanged()
    onFloatingChanged: layoutChanged()
    onPanelShownChanged: {
        if (!panelShown && floating) setPanelFloating(false)
        layoutChanged()
    }

    ApplicationWindow {
        id: floatingWindow
        objectName: root.panelId + "FloatingWindow"
        visible: false
        width: 300
        height: Math.max(180, root.dockedHeight)
        minimumWidth: 230
        minimumHeight: 110
        title: root.title + " — PhotoCrap"
        color: "#242a32"
        flags: Qt.Tool
        onXChanged: root.layoutChanged()
        onYChanged: root.layoutChanged()
        onWidthChanged: root.layoutChanged()
        onHeightChanged: root.layoutChanged()
        onClosing: function(close) {
            if (root.applicationClosing) return
            close.accepted = false
            if (root.hideOnClose) root.hideRequested()
            else {
                root.setPanelFloating(false)
                root.floatingChangedByUser(false)
            }
        }
    }

    Window {
        id: dragOutlineWindow
        visible: headerMouse.pressed && !root.floating
        width: 170
        height: 34
        x: Math.round(headerMouse.currentGlobalX + 14)
        y: Math.round(headerMouse.currentGlobalY + 12)
        color: "transparent"
        flags: Qt.ToolTip | Qt.FramelessWindowHint | Qt.WindowTransparentForInput
        Rectangle {
            anchors.fill: parent
            color: "#dd354453"
            border.width: 2
            border.color: "#a8d5ff"
            radius: 6
            Label {
                anchors.centerIn: parent
                text: root.title
                color: "#ffffff"
                font.bold: true
            }
        }
    }

    Rectangle {
        id: frame
        objectName: root.panelId + "DockFrame"
        anchors.fill: parent
        color: "#242a32"
        border.width: 1
        border.color: "#3a444f"
        radius: root.floating ? 6 : 0

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                id: titleBar
                Layout.fillWidth: true
                Layout.preferredHeight: root.headerHeight
                color: headerMouse.pressed ? "#3a4652" : "#2c343e"

                RowLayout {
                    z: 2
                    anchors.fill: parent
                    anchors.leftMargin: 7
                    anchors.rightMargin: 4
                    spacing: 4
                    Label {
                        text: root.floating ? "⋮⋮" : "⠿"
                        color: "#7f8b97"
                        font.pixelSize: 12
                    }
                    Label {
                        visible: root.groupTabs.length <= 1
                        text: root.title.toUpperCase()
                        color: "#c4cdd6"
                        font.pixelSize: 10
                        font.bold: true
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        visible: root.groupTabs.length > 1
                        Layout.fillWidth: true
                        spacing: 2
                        Repeater {
                            model: root.groupTabs
                            delegate: TabButton {
                                required property var modelData
                                text: modelData.title.toUpperCase()
                                checked: root.activeGroupPanel === modelData.panelId
                                implicitHeight: 25
                                Layout.fillWidth: true
                                onClicked: root.groupTabSelected(modelData.panelId)
                                background: Rectangle {
                                    radius: 3
                                    color: parent.checked ? "#465463" : "#252c34"
                                    border.width: 1
                                    border.color: parent.checked ? "#8194a8" : "#3a444f"
                                }
                                contentItem: Label {
                                    text: parent.text
                                    color: parent.checked ? "#ffffff" : "#aeb9c4"
                                    font.pixelSize: 9
                                    font.bold: parent.checked
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                    ToolButton {
                        visible: !root.floating
                        text: root.collapsed ? "▸" : "▾"
                        implicitWidth: 28
                        implicitHeight: 28
                        font.pixelSize: 16
                        ToolTip.visible: hovered
                        ToolTip.text: root.collapsed ? qsTr("Déplier le panneau") : qsTr("Replier le panneau")
                        onClicked: root.collapsed = !root.collapsed
                    }
                    ToolButton {
                        objectName: root.panelId + "UngroupButton"
                        visible: !root.floating && root.groupTabs.length > 1
                        text: "↥"
                        implicitWidth: visible ? 28 : 0
                        implicitHeight: 28
                        font.pixelSize: 16
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Sortir du groupe d’onglets")
                        onClicked: root.ungroupRequested(root.panelId)
                    }
                    ToolButton {
                        text: root.floating ? "↘" : "↗"
                        implicitWidth: 28
                        implicitHeight: 28
                        font.pixelSize: 16
                        ToolTip.visible: hovered
                        ToolTip.text: root.floating ? qsTr("Ancrer le panneau") : qsTr("Détacher le panneau")
                        onClicked: {
                            root.setPanelFloating(!root.floating)
                            root.floatingChangedByUser(root.floating)
                        }
                    }
                }

                MouseArea {
                    id: headerMouse
                    objectName: root.panelId + "DockHeader"
                    z: 1
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: root.groupTabs.length > 1 ? 92 : 62
                    hoverEnabled: true
                    preventStealing: true
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    property real startGlobalX: 0
                    property real startGlobalY: 0
                    property real currentGlobalX: 0
                    property real currentGlobalY: 0
                    onPressed: function(mouse) {
                        const point = mapToGlobal(mouse.x, mouse.y)
                        startGlobalX = point.x
                        startGlobalY = point.y
                        currentGlobalX = point.x
                        currentGlobalY = point.y
                    }
                    onPositionChanged: function(mouse) {
                        const point = mapToGlobal(mouse.x, mouse.y)
                        currentGlobalX = point.x
                        currentGlobalY = point.y
                        if (pressed) root.dragUpdated(point.x, point.y)
                    }
                    onReleased: function(mouse) {
                        const point = mapToGlobal(mouse.x, mouse.y)
                        const deltaX = point.x - startGlobalX
                        const deltaY = point.y - startGlobalY
                        root.dragFinished()
                        if ((Math.abs(deltaX) > 45 || Math.abs(deltaY) > 12) && !root.floating)
                            root.dropRequested(point.x, point.y, deltaX, deltaY)
                    }
                    onCanceled: root.dragFinished()
                    onDoubleClicked: {
                        root.setPanelFloating(!root.floating)
                        root.floatingChangedByUser(root.floating)
                    }
                }
            }

            Item {
                id: contentHost
                visible: root.floating || !root.collapsed
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: visible ? root.minimumContentHeight : 0
                clip: true
                Loader {
                    anchors.fill: parent
                    sourceComponent: root.contentComponent
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            z: 40
            visible: root.dropIndicator === "tab"
            color: "transparent"
            border.width: 3
            border.color: "#d8e7f5"
            radius: 5
        }
        Rectangle {
            z: 41
            visible: root.dropIndicator === "before" || root.dropIndicator === "after"
            x: 0
            y: root.dropIndicator === "after" ? parent.height - height : 0
            width: parent.width
            height: 5
            color: "#ffffff"
        }
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 7
        z: 30
        visible: !root.floating && !root.collapsed
        cursorShape: Qt.SizeVerCursor
        property real pressGlobalY: 0
        property real initialHeight: 0
        onPressed: function(mouse) {
            pressGlobalY = mapToGlobal(mouse.x, mouse.y).y
            initialHeight = root.dockedHeight
        }
        onPositionChanged: function(mouse) {
            if (!pressed) return
            const globalY = mapToGlobal(mouse.x, mouse.y).y
            root.dockedHeight = Math.max(root.minimumContentHeight + root.headerHeight,
                                         Math.min(650, initialHeight + globalY - pressGlobalY))
        }
    }
}
