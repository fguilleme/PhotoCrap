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

inline bool runThresholdGraphSmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Threshold graph check failed: %s", _message);
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
    width: 512; height: 320; clip: true
    property var histogram: []
    property real level: 128
    property bool referenceVisible: false
    property bool showChart: true
    property real chartWidth: 320
    property real chartHeight: 190
    property real chartOpacity: 1
    property real chartRotation: 0
    Rectangle { anchors.fill: parent; color: "#5a7896" }
    ThresholdHistogram {
        objectName: "thresholdTestChart"
        x: 48; y: 40
        width: fixture.chartWidth; height: fixture.chartHeight
        opacity: fixture.chartOpacity; rotation: fixture.chartRotation
        histogram: fixture.histogram; level: fixture.level
        visible: fixture.showChart && !fixture.referenceVisible
    }
    Canvas {
        id: reference
        x: 48; y: 40
        width: fixture.chartWidth; height: fixture.chartHeight
        opacity: fixture.chartOpacity; rotation: fixture.chartRotation
        visible: fixture.showChart && fixture.referenceVisible
        onVisibleChanged: if(visible)requestPaint()
        Connections {
            target: fixture
            function onHistogramChanged() { if(reference.visible)reference.requestPaint() }
            function onLevelChanged() { if(reference.visible)reference.requestPaint() }
        }
        property real leftEdge: 12
        property real rightEdge: width - 12
        property real topEdge: 8
        property real bottomEdge: height - 30

        function px(value) {
            return leftEdge + value / 255 * (rightEdge - leftEdge)
        }
        function valueAt(pixel) {
            return Math.round(Math.max(0, Math.min(255,
                (pixel - leftEdge) / (rightEdge - leftEdge) * 255)))
        }
        onPaint: {
            const c = getContext("2d")
            c.reset(); c.clearRect(0,0,Math.ceil(width),Math.ceil(height))
            c.fillStyle = "#151a20"
            c.fillRect(0, 0, width, height)
            c.strokeStyle = "#303943"
            for (let i = 0; i <= 4; ++i) {
                const x = leftEdge + (rightEdge - leftEdge) * i / 4
                c.beginPath(); c.moveTo(x, topEdge); c.lineTo(x, bottomEdge); c.stroke()
            }
            if (fixture.histogram && fixture.histogram.length === 256) {
                c.beginPath(); c.moveTo(leftEdge, bottomEdge)
                for (let bin = 0; bin < 256; ++bin)
                    c.lineTo(px(bin), bottomEdge - Number(fixture.histogram[bin]) *
                             (bottomEdge - topEdge) * .92)
                c.lineTo(rightEdge, bottomEdge); c.closePath()
                c.fillStyle = "rgba(206,216,225,.48)"; c.fill()
            }
            const split = px(fixture.level)
            c.fillStyle = "#050607"; c.fillRect(leftEdge, bottomEdge + 5, split - leftEdge, 12)
            c.fillStyle = "#f2f3f4"; c.fillRect(split, bottomEdge + 5, rightEdge - split, 12)
            c.strokeStyle = "#70c995"; c.lineWidth = 2
            c.beginPath(); c.moveTo(split, topEdge); c.lineTo(split, bottomEdge + 19); c.stroke()
            c.beginPath(); c.moveTo(split, bottomEdge + 20)
            c.lineTo(split - 7, bottomEdge + 28); c.lineTo(split + 7, bottomEdge + 28)
            c.closePath(); c.fillStyle = "#70c995"; c.fill()
            c.lineWidth = 1; c.strokeStyle = "#66727d"
            c.strokeRect(leftEdge, topEdge, rightEdge - leftEdge, bottomEdge - topEdge)
        }
    }
}
)QML",
                      QUrl());
    if (!require(component.isReady(), qPrintable(component.errorString())))
        return false;
    std::unique_ptr<QObject> object(component.create());
    auto                    *root = qobject_cast<QQuickItem *>(object.get());
    if (!require(root, "create the histogram fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *chart = root->findChild<QQuickItem *>("thresholdTestChart");
    if (!require(chart, "threshold histogram exists"))
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
                                       "/tmp/photocrap-threshold-graph");
    };
    QVariantList values;
    for (int i = 0; i < 256; ++i)
        values << (i % 23 == 0 ? 1. : .15 + .45 * std::abs(std::sin(i * .027)));
    root->setProperty("histogram", values);
    frame();
    if (!require(chart->property("gpuChart").toBool() == hardware &&
                     chart->property("rasterActive").toBool() != hardware && matchesReference(),
                 "native and software paths preserve the initial histogram and markers"))
        return false;
    QSignalSpy                  paints(chart, SIGNAL(painted()));
    auto                       *graph = visualItem("thresholdGpuGraph");
    std::unique_ptr<QSignalSpy> pathChanges, gridChanges;
    QQuickItem                 *histogramShape = nullptr;
    if (hardware) {
        if (!require(graph, "native graph exists"))
            return false;
        pathChanges    = std::make_unique<QSignalSpy>(graph, SIGNAL(histogramPathChanged()));
        gridChanges    = std::make_unique<QSignalSpy>(graph, SIGNAL(gridPathChanged()));
        histogramShape = visualItem("thresholdHistogramShape");
        if (!require(pathChanges->isValid() && gridChanges->isValid() && histogramShape,
                     "observe static geometry updates"))
            return false;
    }
    for (int level : {0, 1, 32, 127, 128, 200, 254, 255}) {
        root->setProperty("level", level);
        frame();
        if (hardware) {
            auto      *marker = visualItem("thresholdGpuMarker");
            const auto split  = 12 + level / 255. * (chart->width() - 24);
            if (!require(marker && qAbs(marker->x() - (split - 1)) < .001 &&
                             pathChanges->isEmpty() && gridChanges->isEmpty() &&
                             visualItem("thresholdHistogramShape") == histogramShape &&
                             paints.isEmpty(),
                         "moving the cutoff retains histogram geometry without Canvas paints"))
                return false;
        }
    }
    if (!require(hardware || !paints.isEmpty(), "software fallback really paints"))
        return false;
    if (!require(matchesReference(), "white-end cutoff matches the raster reference"))
        return false;
    for (int level : {0, 1, 128, 254}) {
        root->setProperty("level", level);
        frame();
        if (!require(matchesReference(), "cutoff endpoints, split bands and pointer agree"))
            return false;
    }
    // Empty and invalid histograms show the grid without retaining old bins.
    for (const QVariantList &histogram : {QVariantList{}, QVariantList{1., .2}, values}) {
        root->setProperty("histogram", histogram);
        frame();
        if (!require(matchesReference(),
                     "histogram changes invalidate only the corresponding geometry"))
            return false;
    }
    root->setProperty("chartWidth", 382.);
    root->setProperty("chartHeight", 176.);
    frame();
    if (!require(matchesReference(), "resizing updates plot coordinates"))
        return false;
    root->setProperty("chartOpacity", .37);
    frame();
    if (!require(matchesReference(), "opacity composes histogram, grid and markers once"))
        return false;
    root->setProperty("chartOpacity", 1.);
    root->setProperty("chartRotation", 13.);
    frame();
    if (!require(matchesReference(), "rotated graph keeps marker alignment"))
        return false;
    root->setProperty("chartRotation", 0.);
    root->setProperty("chartWidth", 367.5);
    root->setProperty("chartHeight", 187.25);
    frame();
    if (!require(!chart->property("gpuChart").toBool() && matchesReference(),
                 "fractional dimensions preserve original Canvas sampling"))
        return false;
    root->setProperty("showChart", false);
    root->setProperty("level", 19);
    values[100] = 1.;
    root->setProperty("histogram", values);
    frame();
    root->setProperty("chartWidth", 320.);
    root->setProperty("chartHeight", 190.);
    root->setProperty("showChart", true);
    frame();
    if (!require(chart->property("gpuChart").toBool() == hardware && matchesReference(),
                 "restoring the graph displays changes made while hidden"))
        return false;
    qInfo("Threshold graph checks passed");
    return true;
}
