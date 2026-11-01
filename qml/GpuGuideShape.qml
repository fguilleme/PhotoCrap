import QtQuick
import QtQuick.Shapes

// Dynamic access keeps Qt 6.5's raster fallback loadable; CurveRenderer is 6.6+.
Shape {
    id: root
    readonly property bool curveRendererAvailable: "preferredRendererType" in root
    visible: curveRendererAvailable
    Component.onCompleted: if(curveRendererAvailable)
        root["preferredRendererType"]=Shape["CurveRenderer"]
}
