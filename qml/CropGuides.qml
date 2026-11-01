import QtQuick
import QtQuick.Shapes

// Small retained paths and rectangles, with no Canvas or intermediate image.
Item {
    id: root
    objectName: "cropGpuGuides"
    required property var points
    property real zoom: 1
    property point origin: Qt.point(0,0)
    property bool showGrid: true
    property bool perspective: false
    enabled: false
    // CurveRenderer was added in Qt 6.6; keep Qt 6.5's raster-guide fallback.
    readonly property bool curveRendererAvailable: "preferredRendererType" in guideShape
    visible: curveRendererAvailable
    readonly property var corners: points.length===4
        ? points.map(p=>Qt.point(p.x*zoom-origin.x,p.y*zoom-origin.y))
        : [Qt.point(0,0),Qt.point(0,0),Qt.point(0,0),Qt.point(0,0)]
    function at(a,b,t){return Qt.point(a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t)}
    function xy(p){return p.x+" "+p.y}
    readonly property string framePath: "M"+xy(corners[0])+"L"+xy(corners[1])
        +"L"+xy(corners[2])+"L"+xy(corners[3])+"Z"
    readonly property string gridPath: {
        let path=""
        for(let i=1;i<3;i++) {
            const t=i/3,p=corners
            path+="M"+xy(at(p[0],p[1],t))+"L"+xy(at(p[3],p[2],t))
            path+="M"+xy(at(p[0],p[3],t))+"L"+xy(at(p[1],p[2],t))
        }
        return path
    }
    component Stroke: ShapePath {
        id: stroke
        property string svg
        fillColor: "transparent"
        capStyle: ShapePath.FlatCap
        joinStyle: ShapePath.MiterJoin
        miterLimit: 10
        PathSvg { path: stroke.svg }
    }
    Shape {
        id: guideShape
        objectName: "cropGuideShape"
        anchors.fill: parent
        Component.onCompleted: if(root.curveRendererAvailable)
            guideShape["preferredRendererType"]=Shape["CurveRenderer"]
        Stroke {svg:root.framePath;strokeWidth:3;strokeColor:Qt.rgba(0,0,0,.9)}
        Stroke {svg:root.framePath;strokeWidth:1;strokeColor:"white"}
        Stroke {svg:root.showGrid?root.gridPath:"";strokeWidth:2;strokeColor:Qt.rgba(0,0,0,.65)}
        Stroke {svg:root.showGrid?root.gridPath:"";strokeWidth:1;strokeColor:Qt.rgba(1,1,1,.7)}
    }
    Repeater {
        model: root.perspective?4:8
        Rectangle {
            required property int index
            readonly property point center: index<4 ? root.corners[index]
                : root.at(root.corners[index-4],root.corners[(index-3)%4],.5)
            x:center.x-4.5;y:center.y-4.5;width:9;height:9
            color:"white";border.color:"#111111";border.width:1
            antialiasing:true
        }
    }
}
