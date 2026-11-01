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

inline bool runHistogramRenderSmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Histogram rendering check failed: %s", _message);
        return _ok;
    };
    const bool hardware =
        QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi());
    bool          vectorChart = false;
    QQmlComponent component(qmlEngine(_window));
    component.setData(R"QML(
import QtQuick
import PhotoCrap
Item {
    id: fixture
    width: 512; height: 320; clip: true
    property var histogramData: ({})
    property bool referenceVisible: false
    property bool showChart: true
    property real chartWidth: 256
    property real chartHeight: 100
    property real chartOpacity: 1
    property real chartRotation: 0
    property point chartPosition: Qt.point(32,24)
    Rectangle { anchors.fill: parent; color: "#5a7896" }
    HistogramChart {
        objectName: "histogramTestChart"
        x: chartPosition.x; y: chartPosition.y
        width: chartWidth; height: chartHeight
        opacity: chartOpacity; rotation: chartRotation
        histogramData: fixture.histogramData
        visible: showChart && !referenceVisible
    }
    Canvas {
        id: referenceChart
        objectName: "histogramTestReference"
        x: chartPosition.x; y: chartPosition.y
        width: chartWidth; height: chartHeight
        opacity: chartOpacity; rotation: chartRotation
        visible: showChart && referenceVisible
        onVisibleChanged: if (visible) requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: fixture; function onHistogramDataChanged() { if (referenceChart.visible) referenceChart.requestPaint() } }
            onPaint: {
                const context = getContext("2d")
                context.reset()
                context.fillStyle = "#14191f"
                context.fillRect(0, 0, width, height)
                const data = fixture.histogramData || ({})
                function channel(values, color) {
                    if (!values || values.length !== 256) return
                    context.beginPath()
                    context.moveTo(0, height)
                    for (let i = 0; i < 256; ++i)
                        context.lineTo(i * width / 255, height - values[i] * (height - 4))
                    context.lineTo(width, height)
                    context.closePath()
                    context.fillStyle = color
                    context.fill()
                }
                channel(data.red, "rgba(245,63,69,.48)")
                channel(data.green, "rgba(55,220,95,.43)")
                channel(data.blue, "rgba(64,112,255,.48)")
                context.strokeStyle = "rgba(235,239,242,.7)"
                context.lineWidth = 1
                const luminance = data.luminance
                if (luminance && luminance.length === 256) {
                    context.beginPath()
                    for (let i = 0; i < 256; ++i) {
                        const x = i * width / 255
                        const y = height - luminance[i] * (height - 4)
                        if (i === 0) context.moveTo(x, y); else context.lineTo(x, y)
                    }
                    context.stroke()
                }
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
    auto *chart = root->findChild<QQuickItem *>("histogramTestChart");
    if (!require(chart, "the histogram component exists"))
        return false;
    QQuickWindow *target = _window;
    const auto    frame  = [&] {
        target->update();
        QTest::qWait(50);
        return target->grabWindow();
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
        const qreal ratio  = qreal(actual.width()) / target->width();
        const QRect bounds(qFloor(origin.x() * ratio), qFloor(origin.y() * ratio),
                           qFloor(root->width() * ratio), qFloor(root->height() * ratio));
        return compareOverlayReference(actual, reference, bounds, vectorChart,
                                       "/tmp/photocrap-histogram");
    };
    const auto pixel = [&](const QImage &_frame, QPointF _point) {
        const auto  scene = chart->mapToScene(_point);
        const qreal ratio = qreal(_frame.width()) / target->width();
        return _frame.pixelColor(qFloor(scene.x() * ratio), qFloor(scene.y() * ratio));
    };
    const auto near = [](QColor _a, QColor _b) {
        return qAbs(_a.red() - _b.red()) <= 3 && qAbs(_a.green() - _b.green()) <= 3 &&
               qAbs(_a.blue() - _b.blue()) <= 3;
    };
    const auto over = [](QColor _background, QColor _color, qreal _alpha) {
        return QColor(qRound(_color.red() * _alpha + _background.red() * (1 - _alpha)),
                      qRound(_color.green() * _alpha + _background.green() * (1 - _alpha)),
                      qRound(_color.blue() * _alpha + _background.blue() * (1 - _alpha)));
    };
    const auto constant = [](qreal _value) {
        QVariantList values;
        for (int i = 0; i < 256; ++i)
            values.append(_value);
        return values;
    };
    const auto setData = [&](const QVariantMap &_data) {
        root->setProperty("histogramData", _data);
    };
    setData({{"red", constant(.8)},
             {"green", constant(.6)},
             {"blue", constant(.4)},
             {"luminance", constant(.2)}});
    const auto initial = frame();
    vectorChart        = chart->property("gpuChart").toBool();
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if (!require(!hardware || vectorChart, "RHI uses the GPU histogram"))
        return false;
#endif
    auto *shape = visualItem("histogramCurveShape");
    if (!require(vectorChart ? shape && !chart->property("rasterActive").toBool() &&
                                   shape->property("rendererType") ==
                                       shape->property("preferredRendererType")
                             : chart->property("rasterActive").toBool(),
                 "the histogram uses CurveRenderer or its Canvas fallback"))
        return false;
    const QColor background("#14191f");
    const auto   red   = over(background, {245, 63, 69}, .48);
    const auto   green = over(red, {55, 220, 95}, .43);
    const auto   blue  = over(green, {64, 112, 255}, .48);
    if (!require(near(pixel(initial, {128, 10}), background) &&
                     near(pixel(initial, {128, 30}), red) &&
                     near(pixel(initial, {128, 50}), green) &&
                     near(pixel(initial, {128, 70}), blue) && matchesReference(),
                 "RGB bands preserve their baseline, colours and source-over order"))
        return false;
    QSignalSpy paints(chart, SIGNAL(painted()));
    if (!require(paints.isValid(), "observe histogram raster paints"))
        return false;
    QVariantList r, g, b, l;
    for (int i = 0; i < 256; ++i) {
        r.append(qMax(0., 1. - std::abs(i - 64.) / 45.));
        g.append(qMax(0., 1. - std::abs(i - 160.) / 60.));
        b.append(.25 + .2 * std::sin(i * .06));
        l.append(.3 + .2 * std::sin(i * .04));
    }
    const QVariantMap varied{{"red", r}, {"green", g}, {"blue", b}, {"luminance", l}};
    setData(varied);
    const auto changed = frame();
    if (!require((vectorChart ? paints.isEmpty() : !paints.isEmpty()) && changed != initial &&
                     matchesReference(),
                 "new statistics update all channels without raster painting on GPU"))
        return false;
    paints.clear();
    auto *graph = visualItem("histogramGpuChart");
    root->setProperty("chartPosition", QPointF(48.25, 38.75));
    frame();
    if (!require(paints.isEmpty() && visualItem("histogramGpuChart") == graph,
                 "moving the panel retains its chart and avoids painting"))
        return false;
    if (!require(matchesReference(), "fractional panel positioning preserves chart pixels"))
        return false;
    for (const auto size :
         {QSizeF(128, 64), QSizeF(384, 180), QSizeF(257.5, 121.25), QSizeF(19, 9)}) {
        root->setProperty("chartWidth", size.width());
        root->setProperty("chartHeight", size.height());
        frame();
        if (!require(matchesReference(), "panel resizing keeps all 256 bins and luminance aligned"))
            return false;
    }
    root->setProperty("chartWidth", 256.);
    root->setProperty("chartHeight", 100.);
    root->setProperty("chartOpacity", .37);
    frame();
    if (!require(matchesReference(), "inherited opacity applies after the RGB composition"))
        return false;
    root->setProperty("chartOpacity", 1.);
    root->setProperty("chartRotation", 12.);
    frame();
    if (!require(matchesReference(), "transformed charts preserve the composed channel colours"))
        return false;
    root->setProperty("chartRotation", 0.);
    auto impulses = constant(0.);
    impulses[0]   = 1.;
    impulses[127] = 1.;
    impulses[255] = 1.;
    setData({{"red", impulses},
             {"green", constant(0.)},
             {"blue", constant(0.)},
             {"luminance", impulses}});
    frame();
    if (!require(matchesReference(), "edge bins, zeros and isolated peaks remain visible"))
        return false;
    impulses[0]   = .35;
    impulses[127] = .35;
    impulses[255] = .35;
    setData({{"luminance", impulses}});
    frame();
    if (!require(matchesReference(), "lower narrow peaks retain the legacy miter limit"))
        return false;
    setData(
        {{"red", QVariantList{1., .5}}, {"green", QVariantList{}}, {"luminance", constant(0.)}});
    frame();
    if (!require(matchesReference(), "incomplete channels are skipped as in the legacy chart"))
        return false;
    root->setProperty("showChart", false);
    setData(varied);
    frame();
    root->setProperty("showChart", true);
    frame();
    if (!require(matchesReference(), "showing a hidden histogram uses the latest statistics"))
        return false;
    if (hardware && vectorChart) {
        QQuickWindow floating;
        floating.resize(700, 500);
        root->setParentItem(floating.contentItem());
        floating.show();
        const bool exposed  = QTest::qWaitForWindowExposed(&floating, 2000);
        target              = &floating;
        const bool detached = exposed && matchesReference() &&
                              chart->property("gpuChart").toBool() &&
                              !chart->property("rasterActive").toBool();
        root->setParentItem(_window->contentItem());
        target = _window;
        floating.hide();
        frame();
        if (!require(detached && matchesReference() && chart->property("gpuChart").toBool() &&
                         !chart->property("rasterActive").toBool(),
                     "GPU chart moves to a floating window and back"))
            return false;
    }
    setData({});
    const auto empty = frame();
    if (!require(near(pixel(empty, {128, 50}), background) && matchesReference(),
                 "empty histograms clear all channels"))
        return false;
    qInfo("Histogram rendering checks passed");
    return true;
}
