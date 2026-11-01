#pragma once
#include "OverlayReference.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest>
#include <cmath>
#include <memory>

inline bool runLevelsGraphSmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Levels graph check failed: %s", _message);
        return _ok;
    };
    const bool hardware =
        QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi());
    QQmlComponent component(qmlEngine(_window));
    component.setData(R"QML(
import QtQuick
import PhotoCrap
Item {
    id: fixture
    width: 512; height: 360; clip: true
    property var histogram: []
    property real inputBlack: 0
    property real inputWhite: 255
    property real gamma: 1
    property real outputBlack: 0
    property real outputWhite: 255
    property real chartWidth: 320
    property real inputHeight: 205
    property real outputHeight: 48
    property real chartOpacity: 1
    property real chartRotation: 0
    property bool referenceVisible: false
    property bool showCharts: true
    Rectangle { anchors.fill: parent; color: "#5a7896" }
    LevelsChart {
        objectName: "levelsTestInput"
        x:48; y:30; width:fixture.chartWidth; height:fixture.inputHeight
        histogram:fixture.histogram; black:fixture.inputBlack; white:fixture.inputWhite; gamma:fixture.gamma
        visible:fixture.showCharts && !fixture.referenceVisible
        opacity:fixture.chartOpacity; rotation:fixture.chartRotation
    }
    LevelsChart {
        objectName: "levelsTestOutput"
        x:48; y:260; width:fixture.chartWidth; height:fixture.outputHeight
        output:true; black:fixture.outputBlack; white:fixture.outputWhite
        visible:fixture.showCharts && !fixture.referenceVisible
        opacity:fixture.chartOpacity; rotation:fixture.chartRotation
    }
    Canvas {
        id: reference0
        x:48; y:30; width:fixture.chartWidth; height:fixture.inputHeight
        visible:fixture.showCharts && fixture.referenceVisible
        opacity:fixture.chartOpacity; rotation:fixture.chartRotation
        onVisibleChanged: if(visible)requestPaint()
        property real leftEdge: 14
        property real rightEdge: width-14
        property real topEdge: 8
        property real bottomEdge: height-28
        function px(value) { return leftEdge+value/255*(rightEdge-leftEdge) }
        function valueX(pixel) { return Math.max(0,Math.min(255,(pixel-leftEdge)/(rightEdge-leftEdge)*255)) }
        function gammaT(value) { return value>=1?.5-(value-1)/4:.5+(1-value)/1.8 }
        function gammaValue(t) { return t<=.5?1+(.5-t)*4:1-(t-.5)*1.8 }
        function gammaX() { return px(fixture.inputBlack+(fixture.inputWhite-fixture.inputBlack)*gammaT(fixture.gamma)) }
        function triangle(c,x,color) { c.beginPath();c.moveTo(x,bottomEdge+2);c.lineTo(x-7,bottomEdge+15);c.lineTo(x+7,bottomEdge+15);c.closePath();c.fillStyle=color;c.fill();c.strokeStyle="#dbe2e8";c.stroke() }
        onPaint: {
            const c=getContext("2d");c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height));c.fillStyle="#151a20";c.fillRect(0,0,width,height)
            c.strokeStyle="#303943";for(let i=0;i<=4;i++){const x=leftEdge+(rightEdge-leftEdge)*i/4;c.beginPath();c.moveTo(x,topEdge);c.lineTo(x,bottomEdge);c.stroke()}
            if(fixture.histogram && fixture.histogram.length===256){c.beginPath();c.moveTo(leftEdge,bottomEdge);for(let b=0;b<256;b++)c.lineTo(px(b),bottomEdge-Number(fixture.histogram[b])*(bottomEdge-topEdge)*.92);c.lineTo(rightEdge,bottomEdge);c.closePath();c.fillStyle="rgba(206,216,225,.42)";c.fill()}
            c.strokeStyle="#66727d";c.strokeRect(leftEdge,topEdge,rightEdge-leftEdge,bottomEdge-topEdge)
            triangle(c,px(fixture.inputBlack),"#07090b");triangle(c,gammaX(),"#858b91");triangle(c,px(fixture.inputWhite),"#f4f5f6")
        }
    }
    Canvas {
        id: reference1
        x:48; y:260; width:fixture.chartWidth; height:fixture.outputHeight
        visible:fixture.showCharts && fixture.referenceVisible
        opacity:fixture.chartOpacity; rotation:fixture.chartRotation
        onVisibleChanged: if(visible)requestPaint()
        property real leftEdge: 14
        property real rightEdge: width-14
        function px(value) { return leftEdge+value/255*(rightEdge-leftEdge) }
        function valueX(pixel) { return Math.max(0,Math.min(255,(pixel-leftEdge)/(rightEdge-leftEdge)*255)) }
        onPaint: {
            const c=getContext("2d");c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height));const gradient=c.createLinearGradient(leftEdge,0,rightEdge,0);gradient.addColorStop(0,"#000");gradient.addColorStop(1,"#fff");c.fillStyle=gradient;c.fillRect(leftEdge,5,rightEdge-leftEdge,14)
            function marker(x,color){c.beginPath();c.moveTo(x,21);c.lineTo(x-7,35);c.lineTo(x+7,35);c.closePath();c.fillStyle=color;c.fill();c.strokeStyle="#dbe2e8";c.stroke()}
            marker(px(fixture.outputBlack),"#07090b");marker(px(fixture.outputWhite),"#f4f5f6")
        }
    }
}
)QML",
                      QUrl());
    if (!require(component.isReady(), qPrintable(component.errorString())))
        return false;
    std::unique_ptr<QObject> object(component.create());
    auto                    *root = qobject_cast<QQuickItem *>(object.get());
    if (!require(root, "create levels fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *input  = root->findChild<QQuickItem *>("levelsTestInput");
    auto *output = root->findChild<QQuickItem *>("levelsTestOutput");
    if (!require(input && output, "both levels charts exist"))
        return false;
    const auto frame = [&] {
        _window->update();
        QTest::qWait(50);
        return _window->grabWindow();
    };
    const auto visualItem = [&](QQuickItem *_parent, const QString &_name) -> QQuickItem * {
        QList<QQuickItem *> pending{_parent};
        while (!pending.isEmpty()) {
            auto *item = pending.takeLast();
            if (item->objectName() == _name)
                return item;
            pending.append(item->childItems());
        }
        return nullptr;
    };
    const auto matchesReference = [&] {
        root->setProperty("referenceVisible", true);
        const auto reference = frame();
        root->setProperty("referenceVisible", false);
        const auto  actual = frame();
        const auto  origin = root->mapToScene(QPointF{});
        const qreal ratio  = qreal(actual.width()) / _window->width();
        const QRect bounds(qFloor(origin.x() * ratio), qFloor(origin.y() * ratio),
                           qFloor(root->width() * ratio), qFloor(root->height() * ratio));
        return compareOverlayReference(actual, reference, bounds, hardware,
                                       "/tmp/photocrap-levels-graph");
    };
    QVariantList values;
    for (int i = 0; i < 256; ++i)
        values << (i % 23 == 0 ? 1. : .15 + .45 * std::abs(std::sin(i * .027)));
    root->setProperty("histogram", values);
    frame();
    if (!require(input->property("gpuChart").toBool() == hardware &&
                     output->property("gpuChart").toBool() == hardware &&
                     input->property("rasterActive").toBool() != hardware &&
                     output->property("rasterActive").toBool() != hardware && matchesReference(),
                 "input histogram and output gradient preserve their original rendering"))
        return false;
    QSignalSpy inputPaints(input, SIGNAL(painted())), outputPaints(output, SIGNAL(painted()));
    auto      *graph    = visualItem(input, "levelsInputGpuGraph");
    auto      *gradient = visualItem(output, "levelsOutputGradient");
    auto      *shape    = visualItem(input, "levelsHistogramShape");
    std::unique_ptr<QSignalSpy> histogramChanges, gridChanges, gradientChanges;
    if (hardware) {
        if (!require(graph && gradient && shape, "native chart elements exist"))
            return false;
        histogramChanges = std::make_unique<QSignalSpy>(graph, SIGNAL(histogramPathChanged()));
        gridChanges      = std::make_unique<QSignalSpy>(graph, SIGNAL(gridPathChanged()));
        gradientChanges  = std::make_unique<QSignalSpy>(gradient, SIGNAL(widthChanged()));
        if (!require(histogramChanges->isValid() && gridChanges->isValid() &&
                         gradientChanges->isValid(),
                     "observe cached paths and gradient geometry"))
            return false;
    }
    root->setProperty("inputBlack", 24.);
    root->setProperty("inputWhite", 230.);
    root->setProperty("outputBlack", 40.);
    root->setProperty("outputWhite", 200.);
    for (qreal gamma : {.1, .5, 1., 2., 3.}) {
        root->setProperty("gamma", gamma);
        frame();
        if (hardware) {
            auto       *marker   = visualItem(input, "levelsGpuMarker1");
            const qreal t        = gamma >= 1 ? .5 - (gamma - 1) / 4 : .5 + (1 - gamma) / 1.8;
            const qreal expected = 14 + (24 + (230 - 24) * t) / 255. * (input->width() - 28);
            if (!require(
                    marker && qAbs(marker->x() - expected) < .001 && histogramChanges->isEmpty() &&
                        gridChanges->isEmpty() && gradientChanges->isEmpty() &&
                        visualItem(input, "levelsHistogramShape") == shape &&
                        visualItem(output, "levelsOutputGradient") == gradient &&
                        inputPaints.isEmpty() && outputPaints.isEmpty(),
                    "moving black, white and gamma retains static geometry without raster paints"))
                return false;
        }
    }
    if (!require(hardware || (!inputPaints.isEmpty() && !outputPaints.isEmpty()),
                 "software fallback paints both charts"))
        return false;
    if (!require(matchesReference(), "adjusted input and output markers match the reference"))
        return false;
    for (qreal gamma : {.1, .5, 1., 2., 3.}) {
        root->setProperty("gamma", gamma);
        frame();
        if (!require(matchesReference(),
                     "gamma endpoints and overlapping markers preserve drawing order"))
            return false;
    }
    root->setProperty("inputBlack", 253.);
    root->setProperty("inputWhite", 254.);
    root->setProperty("outputBlack", 128.);
    root->setProperty("outputWhite", 128.);
    frame();
    if (!require(matchesReference(), "coincident output markers and narrow input range agree"))
        return false;
    root->setProperty("inputBlack", 0.);
    root->setProperty("inputWhite", 255.);
    root->setProperty("outputBlack", 0.);
    root->setProperty("outputWhite", 255.);
    root->setProperty("gamma", 1.);
    for (const QVariantList &histogram : {QVariantList{}, QVariantList{1., .2}, values}) {
        root->setProperty("histogram", histogram);
        frame();
        if (!require(matchesReference(), "histogram changes and invalid data update only the plot"))
            return false;
    }
    root->setProperty("chartWidth", 382.);
    root->setProperty("inputHeight", 188.);
    frame();
    if (!require(matchesReference(), "resizing updates both charts"))
        return false;
    root->setProperty("chartOpacity", .37);
    frame();
    if (!require(matchesReference(), "opacity composes each transparent chart once"))
        return false;
    root->setProperty("chartOpacity", 1.);
    root->setProperty("chartRotation", 13.);
    frame();
    if (!require(matchesReference(), "rotation preserves pointer and gradient placement"))
        return false;
    root->setProperty("chartRotation", 0.);
    root->setProperty("chartWidth", 367.5);
    root->setProperty("inputHeight", 187.25);
    root->setProperty("outputHeight", 48.25);
    frame();
    if (!require(!input->property("gpuChart").toBool() && !output->property("gpuChart").toBool() &&
                     matchesReference(),
                 "fractional dimensions retain Canvas sampling"))
        return false;
    const int hiddenInputPaints  = inputPaints.count();
    const int hiddenOutputPaints = outputPaints.count();
    root->setProperty("showCharts", false);
    root->setProperty("gamma", .75);
    root->setProperty("inputBlack", 20.);
    root->setProperty("outputWhite", 180.);
    values[100] = 1.;
    root->setProperty("histogram", values);
    frame();
    if (!require(inputPaints.count() == hiddenInputPaints &&
                     outputPaints.count() == hiddenOutputPaints,
                 "hidden charts retain updates without raster paints"))
        return false;
    root->setProperty("chartWidth", 368.);
    root->setProperty("inputHeight", 205.);
    root->setProperty("outputHeight", 48.);
    root->setProperty("showCharts", true);
    frame();
    if (!require(input->property("gpuChart").toBool() == hardware &&
                     output->property("gpuChart").toBool() == hardware && matchesReference(),
                 "restored charts reflect changes made while hidden"))
        return false;
    qInfo("Levels graph checks passed");
    return true;
}
