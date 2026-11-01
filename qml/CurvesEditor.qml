import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "CurveInterpolation.js" as CurveInterpolation

ColumnLayout {
    id: root
    objectName: "curvesEditorRoot"
    property var histogram: []
    property var sourcePoints: [{"x": 0, "y": 0}, {"x": 255, "y": 255}]
    property var points: []
    property int activePoint: -1
    property bool dragging: false
    property string pickerMode: ""
    signal editStarted()
    signal previewed(var points)
    signal committed(var points)
    signal pickerSelected(string mode)
    spacing: 6

    function copyPoints(value) {
        const result=[]
        if(value) for(let i=0;i<value.length;i++)
            result.push({x:Math.max(0,Math.min(255,Number(value[i].x))),
                         y:Math.max(0,Math.min(255,Number(value[i].y)))})
        if(result.length<2) return [{x:0,y:0},{x:255,y:255}]
        result.sort((a,b)=>a.x-b.x)
        return result
    }
    function sync() { if(!dragging) { points=copyPoints(sourcePoints) } }
    function emittedPoints() { return copyPoints(points) }
    function addColorSample(color) {
        const input=Math.max(0,Math.min(255,Math.round((.2126*color.r+.7152*color.g+.0722*color.b)*255)))
        const output=curveValue(input)
        editStarted()
        let nearest=-1
        for(let i=0;i<points.length;i++) if(Math.abs(points[i].x-input)<2) nearest=i
        if(nearest>=0) points[nearest]={x:input,y:output}
        else points.push({x:input,y:output})
        points.sort((a,b)=>a.x-b.x)
        activePoint=points.findIndex(point=>Math.abs(point.x-input)<.5)
        points=points.slice()
        committed(emittedPoints())
    }
    readonly property var interpolationState: ({points:points,slopes:CurveInterpolation.prepare(points)})
    readonly property var curveSamples: CurveInterpolation.sample(interpolationState.points,interpolationState.slopes)
    function curveValue(input) { return CurveInterpolation.value(interpolationState.points,interpolationState.slopes,input) }

    onSourcePointsChanged: sync()
    Component.onCompleted: sync()

    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("RVB composite"); color: "#c4cdd5"; Layout.fillWidth: true }
        Label { text: qsTr("Pipettes"); color: "#7f8b96"; font.pixelSize: 10 }
        EyedropperButton {
            objectName: "curveColorPicker"
            sampleMode: "color"; checked: root.pickerMode === "color"
            hint: qsTr("Échantillonner une couleur dans la courbe")
            onClicked: root.pickerSelected(checked ? "color" : "")
        }
        EyedropperButton {
            objectName: "curveBlackPicker"
            sampleMode: "black"; checked: root.pickerMode === "black"
            hint: qsTr("Définir le point noir")
            onClicked: root.pickerSelected(checked ? "black" : "")
        }
        EyedropperButton {
            objectName: "curveGrayPicker"
            sampleMode: "gray"; checked: root.pickerMode === "gray"
            hint: qsTr("Neutraliser un gris")
            onClicked: root.pickerSelected(checked ? "gray" : "")
        }
        EyedropperButton {
            objectName: "curveWhitePicker"
            sampleMode: "white"; checked: root.pickerMode === "white"
            hint: qsTr("Définir le point blanc")
            onClicked: root.pickerSelected(checked ? "white" : "")
        }
    }
    CurvesChart {
        id: curveCanvas
        objectName: "curvesEditor"
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(width, 320)
        Layout.minimumHeight: 230
        histogram: root.histogram
        points: root.points
        samples: root.curveSamples
        activePoint: root.activePoint
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: function(mouse) {
                let nearest=-1, distance=14
                for(let i=0;i<root.points.length;i++) {
                    const dx=mouse.x-curveCanvas.px(root.points[i].x),dy=mouse.y-curveCanvas.py(root.points[i].y)
                    const d=Math.sqrt(dx*dx+dy*dy);if(d<distance){distance=d;nearest=i}
                }
                if(mouse.button===Qt.RightButton) {
                    if(nearest>0 && nearest<root.points.length-1) {
                        root.editStarted();root.points.splice(nearest,1);root.points=root.points.slice();root.activePoint=-1
                        root.committed(root.emittedPoints())
                    }
                    return
                }
                root.editStarted();root.dragging=true
                if(nearest<0) {
                    root.points.push({x:curveCanvas.valueX(mouse.x),y:curveCanvas.valueY(mouse.y)})
                    root.points.sort((a,b)=>a.x-b.x);nearest=root.points.findIndex(p=>Math.abs(p.x-curveCanvas.valueX(mouse.x))<.5)
                }
                root.activePoint=nearest;updatePoint(mouse.x,mouse.y)
            }
            function updatePoint(x,y) {
                if(root.activePoint<0)return
                const previous=root.activePoint>0?root.points[root.activePoint-1].x+1:0
                const next=root.activePoint+1<root.points.length?root.points[root.activePoint+1].x-1:255
                const lockedX=root.activePoint===0?0:root.activePoint===root.points.length-1?255:Math.max(previous,Math.min(next,curveCanvas.valueX(x)))
                root.points[root.activePoint]={x:lockedX,y:curveCanvas.valueY(y)};root.points=root.points.slice()
                root.previewed(root.emittedPoints())
            }
            onPositionChanged: function(mouse) { if(pressed && (mouse.buttons & Qt.LeftButton)) updatePoint(mouse.x,mouse.y) }
            onReleased: {
                if(root.dragging)root.committed(root.emittedPoints())
                root.dragging=false
            }
            onCanceled: root.dragging=false
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Ombres"); color: "#7f8b96" }
        Item { Layout.fillWidth: true }
        Label { text: qsTr("Clic : ajouter · clic droit : supprimer"); color: "#7f8b96"; font.pixelSize: 10 }
        Item { Layout.fillWidth: true }
        Label { text: qsTr("Hautes lumières"); color: "#7f8b96" }
    }
}
