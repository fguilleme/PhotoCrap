import QtQuick
import QtQuick.Controls

ToolButton {
    id: root
    property string sampleMode: "color"
    property string hint: ""
    checkable: true
    implicitWidth:32;implicitHeight:32
    Accessible.name: hint
    ToolTip.visible: hovered
    ToolTip.text: hint
    background: Rectangle {
        radius:4
        color:root.checked ? "#345344" : root.down ? "#404a54" : root.hovered ? "#303b46" : "#222a33"
        border.color:root.checked ? "#74b393" : "#4a5662"
    }
    contentItem: Item {
        ToolIcon {
            anchors.centerIn:parent;width:23;height:23
            toolName:"picker"
            iconColor:root.checked ? "#c0f1d2" : "#d0d9e2"
        }
        Rectangle {
            anchors.right:parent.right;anchors.bottom:parent.bottom
            width:7;height:7;radius:3.5
            color:root.sampleMode==="black" ? "#08090a" : root.sampleMode==="gray" || root.sampleMode==="whiteBalance" ? "#969696"
                  : root.sampleMode==="white" ? "#ffffff" : "#5fca8a"
            border.color:"#e1e7ed"
        }
    }
}
