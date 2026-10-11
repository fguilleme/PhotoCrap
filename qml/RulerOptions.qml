import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property point start: Qt.point(0,0)
    property point end: Qt.point(0,0)
    property bool hasMeasure: false
    signal clearMeasure()
    color: "#20262e"
    implicitHeight:44
    readonly property real dx: hasMeasure ? end.x-start.x : 0
    readonly property real dy: hasMeasure ? end.y-start.y : 0
    RowLayout {
        anchors.fill:parent;anchors.leftMargin:18;anchors.rightMargin:18;spacing:8
        ToolIcon { toolName:"ruler";Layout.preferredWidth:24;Layout.preferredHeight:24 }
        Repeater {
            model:[{name:"X",value:root.hasMeasure?root.start.x:0},{name:"Y",value:root.hasMeasure?root.start.y:0},
                   {name:"L",value:root.dx},{name:"H",value:root.dy},
                   {name:"A",value:Math.atan2(root.dy,root.dx)*180/Math.PI,angle:true},
                   {name:"D1",value:Math.sqrt(root.dx*root.dx+root.dy*root.dy)}]
            delegate:RowLayout {
                required property var modelData
                spacing:5
                Label { text:modelData.name+":";color:"#aebbc8" }
                Rectangle {
                    color:"#171e26";border.color:"#3c4752";radius:3
                    implicitWidth:84;implicitHeight:28
                    Label { anchors.centerIn:parent;text:modelData.value.toFixed(1)+(modelData.angle?" °":" px");color:"#d2dce5";font.pixelSize:11 }
                }
            }
        }
        Button { objectName:"clearRulerMeasure";text:qsTr("Effacer");enabled:root.hasMeasure;implicitHeight:30;onClicked:root.clearMeasure() }
        Label { text:qsTr("Maj : contraindre à 45°");color:"#87929e";Layout.fillWidth:true;elide:Text.ElideRight }
    }
}
