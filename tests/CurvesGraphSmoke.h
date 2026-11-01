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

inline bool runCurvesGraphSmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Curves graph check failed: %s", _message);
        return _ok;
    };
    const bool hardware =
        QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi());
    QQmlComponent component(qmlEngine(_window));
    component.setData(R"QML(
import QtQuick
import PhotoCrap
import "qrc:/qt/qml/PhotoCrap/qml/CurveInterpolation.js" as CurveInterpolation
Item {
    id: fixture
    width: 512; height: 380; clip: true
    property var histogram: []
    property var points: [{x:0,y:0},{x:255,y:255}]
    property int activePoint: -1
    readonly property var prepared: ({points:points,slopes:CurveInterpolation.prepare(points)})
    readonly property var slopes: prepared.slopes
    readonly property var samples: CurveInterpolation.sample(prepared.points,prepared.slopes)
    readonly property bool mathMatches: {
        for(let i=0;i<256;++i)if(!isFinite(samples[i]) || !isFinite(curveValue(i)) || Math.abs(samples[i]-curveValue(i))>1e-9)return false
        for(const i of [.5,17.75,127.5,254.9])
            if(!isFinite(CurveInterpolation.value(points,slopes,i)) || !isFinite(curveValue(i)) || Math.abs(CurveInterpolation.value(points,slopes,i)-curveValue(i))>1e-9)return false
        return true
    }
    property real chartWidth: 320
    property real chartHeight: 280
    property real chartOpacity: 1
    property real chartRotation: 0
    property bool referenceVisible: false
    property bool showChart: true
    function curveValue(input) {
        if(points.length<2)return input
        const delta=[],slopes=[]
        for(let i=0;i+1<points.length;i++)
            delta.push((points[i+1].y-points[i].y)/Math.max(.001,points[i+1].x-points[i].x))
        slopes.push(delta[0])
        for(let i=1;i+1<points.length;i++) {
            if(delta[i-1]*delta[i]<=0)slopes.push(0)
            else {
                const left=points[i].x-points[i-1].x,right=points[i+1].x-points[i].x
                slopes.push((left+right)/((left+2*right)/(3*delta[i-1])+(2*left+right)/(3*delta[i])))
            }
        }
        slopes.push(delta[delta.length-1])
        if(input<=points[0].x)return points[0].y
        if(input>=points[points.length-1].x)return points[points.length-1].y
        let segment=0
        while(segment+1<points.length-1 && input>points[segment+1].x)segment++
        const width=points[segment+1].x-points[segment].x
        const t=(input-points[segment].x)/width,t2=t*t,t3=t2*t
        return (2*t3-3*t2+1)*points[segment].y+(t3-2*t2+t)*width*slopes[segment]
                +(-2*t3+3*t2)*points[segment+1].y+(t3-t2)*width*slopes[segment+1]
    }
    Rectangle { anchors.fill: parent; color: "#5a7896" }
    CurvesChart {
        objectName: "curvesTestChart"
        x:48;y:30;width:fixture.chartWidth;height:fixture.chartHeight
        histogram:fixture.histogram;points:fixture.points;samples:fixture.samples;activePoint:fixture.activePoint
        visible:fixture.showChart && !fixture.referenceVisible
        opacity:fixture.chartOpacity;rotation:fixture.chartRotation
    }
    Canvas {
        id: reference
        x:48;y:30;width:fixture.chartWidth;height:fixture.chartHeight
        visible:fixture.showChart && fixture.referenceVisible
        opacity:fixture.chartOpacity;rotation:fixture.chartRotation
        onVisibleChanged: if(visible)requestPaint()
        property real leftEdge: 18
        property real rightEdge: width-10
        property real topEdge: 10
        property real bottomEdge: height-20
        function px(value) { return leftEdge+value/255*(rightEdge-leftEdge) }
        function py(value) { return bottomEdge-value/255*(bottomEdge-topEdge) }
        function valueX(pixel) { return Math.max(0,Math.min(255,(pixel-leftEdge)/(rightEdge-leftEdge)*255)) }
        function valueY(pixel) { return Math.max(0,Math.min(255,(bottomEdge-pixel)/(bottomEdge-topEdge)*255)) }
        onPaint: {
            const c=getContext("2d"); c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height)); c.fillStyle="#151a20"; c.fillRect(0,0,width,height)
            c.strokeStyle="#303943"; c.lineWidth=1
            for(let i=0;i<=4;i++) {
                const x=leftEdge+(rightEdge-leftEdge)*i/4, y=topEdge+(bottomEdge-topEdge)*i/4
                c.beginPath();c.moveTo(x,topEdge);c.lineTo(x,bottomEdge);c.stroke()
                c.beginPath();c.moveTo(leftEdge,y);c.lineTo(rightEdge,y);c.stroke()
            }
            if(fixture.histogram && fixture.histogram.length===256) {
                c.beginPath();c.moveTo(leftEdge,bottomEdge)
                for(let b=0;b<256;b++) c.lineTo(px(b),bottomEdge-Number(fixture.histogram[b])*(bottomEdge-topEdge)*.82)
                c.lineTo(rightEdge,bottomEdge);c.closePath();c.fillStyle="rgba(185,197,208,.25)";c.fill()
            }
            c.strokeStyle="#65717c";c.beginPath();c.moveTo(leftEdge,bottomEdge);c.lineTo(rightEdge,topEdge);c.stroke()
            c.strokeStyle="#e8edf1";c.lineWidth=2;c.beginPath();c.moveTo(px(0),py(fixture.curveValue(0)))
            for(let p=1;p<256;p++)c.lineTo(px(p),py(fixture.curveValue(p)))
            c.stroke()
            for(let j=0;j<fixture.points.length;j++) {
                const point=fixture.points[j], x=px(point.x), y=py(point.y)
                c.beginPath();c.arc(x,y,j===fixture.activePoint?6:5,0,Math.PI*2)
                c.fillStyle=j===fixture.activePoint?"#ffffff":"#222a32";c.fill()
                c.strokeStyle=j===fixture.activePoint?"#1d80e5":"#dce4ea";c.lineWidth=2;c.stroke()
            }
            c.strokeStyle="#66727d";c.lineWidth=1;c.strokeRect(leftEdge,topEdge,rightEdge-leftEdge,bottomEdge-topEdge)
        }
    }
}
)QML",
                      QUrl());
    if (!require(component.isReady(), qPrintable(component.errorString())))
        return false;
    std::unique_ptr<QObject> object(component.create());
    auto                    *root = qobject_cast<QQuickItem *>(object.get());
    if (!require(root, "create curves fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *chart = root->findChild<QQuickItem *>("curvesTestChart");
    if (!require(chart, "curves chart exists"))
        return false;
    const auto frame = [&] {
        _window->update();
        QTest::qWait(50);
        return _window->grabWindow();
    };
    const auto visualItem = [&](const QString &_name) -> QQuickItem * {
        QList<QQuickItem *> pending{root};
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
                                       "/tmp/photocrap-curves-graph");
    };
    QVariantList values;
    for (int i = 0; i < 256; ++i)
        values << (i % 23 == 0 ? 1. : .15 + .45 * std::abs(std::sin(i * .027)));
    root->setProperty("histogram", values);
    frame();
    if (!require(root->property("mathMatches").toBool() &&
                     chart->property("gpuChart").toBool() == hardware &&
                     chart->property("rasterActive").toBool() != hardware && matchesReference(),
                 "cached interpolation and initial drawing match the original editor"))
        return false;
    QSignalSpy paints(chart, SIGNAL(painted()));
    QSignalSpy samplesChanged(root, SIGNAL(samplesChanged())),
        slopesChanged(root, SIGNAL(slopesChanged()));
    auto                       *graph          = visualItem("curvesGpuGraph");
    auto                       *histogramShape = visualItem("curvesHistogramShape");
    auto                       *transferShape  = visualItem("curvesTransferShape");
    std::unique_ptr<QSignalSpy> histogramChanges, gridChanges, curveChanges;
    if (hardware) {
        if (!require(graph && histogramShape && transferShape, "native chart elements exist"))
            return false;
        histogramChanges = std::make_unique<QSignalSpy>(graph, SIGNAL(histogramPathChanged()));
        gridChanges      = std::make_unique<QSignalSpy>(graph, SIGNAL(gridPathChanged()));
        curveChanges     = std::make_unique<QSignalSpy>(graph, SIGNAL(curveOutlineChanged()));
        if (!require(histogramChanges->isValid() && gridChanges->isValid() &&
                         curveChanges->isValid(),
                     "observe cached graph paths"))
            return false;
    }
    for (int active : {0, 1, -1}) {
        root->setProperty("activePoint", active);
        frame();
        if (!require(samplesChanged.isEmpty() && slopesChanged.isEmpty(),
                     "changing active point does not evaluate the transfer again"))
            return false;
        if (hardware &&
            !require(histogramChanges->isEmpty() && gridChanges->isEmpty() &&
                         curveChanges->isEmpty() &&
                         visualItem("curvesHistogramShape") == histogramShape &&
                         visualItem("curvesTransferShape") == transferShape && paints.isEmpty(),
                     "selecting a point preserves paths without Canvas paints"))
            return false;
    }
    const auto points = [](std::initializer_list<QPointF> _points) {
        QVariantList list;
        for (const auto &p : _points)
            list << QVariantMap{{"x", p.x()}, {"y", p.y()}};
        return list;
    };
    const auto edited = points({{0, 0}, {64, 25}, {180, 220}, {255, 255}});
    root->setProperty("points", edited);
    root->setProperty("activePoint", 1);
    frame();
    if (!require(samplesChanged.count() == 1 && slopesChanged.count() == 1 &&
                     root->property("mathMatches").toBool(),
                 "point edits invalidate cached interpolation and preserve numeric values"))
        return false;
    if (hardware &&
        !require(histogramChanges->isEmpty() && gridChanges->isEmpty() &&
                     !curveChanges->isEmpty() &&
                     visualItem("curvesHistogramShape") == histogramShape && paints.isEmpty(),
                 "point edits update only the transfer path and handles"))
        return false;
    if (!require(hardware || !paints.isEmpty(), "software fallback actually paints"))
        return false;
    if (!require(matchesReference(), "edited curve and selected handle match the reference"))
        return false;
    const QList<QVariantList> cases{points({{0, 255}, {255, 0}}),
                                    points({{0, 128}, {255, 128}}),
                                    points({{0, 0}, {127, 0}, {128, 255}, {129, 0}, {255, 255}}),
                                    points({{0, 255}, {1, 0}, {254, 255}, {255, 0}}),
                                    points({{10, 40}, {64, 180}, {120, 50}, {210, 240}}),
                                    points({{0, 0}, {128, 60}, {128, 190}, {255, 255}}),
                                    points({{0, 0}, {255, 255}}),
                                    QVariantList{},
                                    points({{128, 128}})};
    for (const auto &curve : cases) {
        root->setProperty("points", curve);
        root->setProperty("activePoint", curve.size() > 2 ? 1 : -1);
        frame();
        if (!require(root->property("mathMatches").toBool() && matchesReference(),
                     "inverse, flat, sharp, shifted and degenerate curves preserve interpolation "
                     "and pixels"))
            return false;
    }
    root->setProperty("points", edited);
    root->setProperty("activePoint", 2);
    frame();
    const int sampleCount = samplesChanged.count(), slopeCount = slopesChanged.count();
    for (const QVariantList &histogram : {QVariantList{}, QVariantList{1., .2}, values}) {
        root->setProperty("histogram", histogram);
        frame();
        if (!require(samplesChanged.count() == sampleCount && slopesChanged.count() == slopeCount &&
                         matchesReference(),
                     "new histogram data does not evaluate the transfer again"))
            return false;
    }
    root->setProperty("chartWidth", 382.);
    root->setProperty("chartHeight", 265.);
    frame();
    if (!require(samplesChanged.count() == sampleCount && slopesChanged.count() == slopeCount &&
                     matchesReference(),
                 "resizing reprojects the same sampled transfer"))
        return false;
    root->setProperty("chartOpacity", .37);
    frame();
    if (!require(matchesReference(), "opacity composes grid, curve and handles once"))
        return false;
    root->setProperty("chartOpacity", 1.);
    root->setProperty("chartRotation", 13.);
    frame();
    if (!require(matchesReference(), "rotation preserves curve and handle alignment"))
        return false;
    root->setProperty("chartRotation", 0.);
    root->setProperty("chartWidth", 367.5);
    root->setProperty("chartHeight", 267.25);
    frame();
    if (!require(!chart->property("gpuChart").toBool() && matchesReference(),
                 "fractional dimensions retain Canvas sampling"))
        return false;
    const int hiddenPaints = paints.count();
    root->setProperty("showChart", false);
    root->setProperty("points", points({{0, 20}, {100, 220}, {255, 240}}));
    root->setProperty("activePoint", 0);
    values[100] = 1.;
    root->setProperty("histogram", values);
    frame();
    if (!require(paints.count() == hiddenPaints,
                 "hidden chart retains changes without raster painting"))
        return false;
    root->setProperty("chartWidth", 368.);
    root->setProperty("chartHeight", 280.);
    root->setProperty("showChart", true);
    frame();
    if (!require(chart->property("gpuChart").toBool() == hardware &&
                     root->property("mathMatches").toBool() && matchesReference(),
                 "restoring the chart displays all hidden changes"))
        return false;
    qInfo("Curves graph checks passed");
    return true;
}
