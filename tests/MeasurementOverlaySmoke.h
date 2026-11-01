#pragma once
#include "OverlayReference.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest>
#include <memory>

inline bool runMeasurementOverlaySmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Measurement overlay check failed: %s", _message);
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
    width: 512; height: 320
    property point samplePoint: Qt.point(200,80)
    property color sampleColor: "red"
    property color previousColor: "#00ff00"
    property bool showRing: true
    property real panX: 0
    property real panY: 0
    property real viewRotation: 0
    property bool referenceVisible: false
    property real measureZoom: 16
    property point measureStart: Qt.point(8,8)
    property point measureEnd: Qt.point(20,18)
    property bool showRuler: true
    Rectangle { anchors.fill: parent; color: "#5a7896" }
component LegacyRulerOverlay: ScreenOverlayCanvas {
    id: root
    required property point startPoint
    required property point endPoint
    enabled: false
    readonly property rect lineBounds: {
        const left=Math.max(screenBounds.x,Math.floor(Math.min(startPoint.x,endPoint.x)*zoom-6))
        const top=Math.max(screenBounds.y,Math.floor(Math.min(startPoint.y,endPoint.y)*zoom-6))
        const right=Math.min(screenBounds.x+screenBounds.width,Math.ceil(Math.max(startPoint.x,endPoint.x)*zoom+6))
        const bottom=Math.min(screenBounds.y+screenBounds.height,Math.ceil(Math.max(startPoint.y,endPoint.y)*zoom+6))
        return Qt.rect(left,top,Math.max(0,right-left),Math.max(0,bottom-top))
    }
    x: lineBounds.x
    y: lineBounds.y
    width: lineBounds.width
    height: lineBounds.height
    onStartPointChanged: if (visible) requestPaint()
    onEndPointChanged: if (visible) requestPaint()
    onZoomChanged: if (visible) requestPaint()
    onVisibleChanged: if (visible) requestPaint()
    onPaint: {
        const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
        c.translate(-x,-y)
        const x1=startPoint.x*zoom,y1=startPoint.y*zoom
        const x2=endPoint.x*zoom,y2=endPoint.y*zoom
        c.strokeStyle="rgba(0,0,0,.85)";c.lineWidth=4
        c.beginPath();c.moveTo(x1,y1);c.lineTo(x2,y2);c.stroke()
        c.strokeStyle="#f5f7f9";c.lineWidth=2
        c.beginPath();c.moveTo(x1,y1);c.lineTo(x2,y2);c.stroke()
        for(const p of [{x:x1,y:y1},{x:x2,y:y2}]) {
            c.beginPath();c.arc(p.x,p.y,4,0,Math.PI*2)
            c.fillStyle="#f5f7f9";c.fill();c.strokeStyle="#101419";c.lineWidth=1;c.stroke()
        }
    }
}

    Flickable {
        id: view
        anchors.fill: parent
        contentWidth: board.width; contentHeight: board.height
        contentX: panX; contentY: panY
        clip: true; interactive: false
        Item {
            id: board
            objectName: "measurementTestBoard"
            width: 16384*fixture.measureZoom; height: 16384*fixture.measureZoom
            rotation: viewRotation; transformOrigin: Item.TopLeft
            RulerOverlay {
                objectName: "measurementTestRuler"
                viewportItem: view; zoom: fixture.measureZoom
                startPoint: fixture.measureStart; endPoint: measureEnd
                visible: showRuler && !referenceVisible
            }
            LegacyRulerOverlay {
                viewportItem: view;zoom:fixture.measureZoom
                startPoint:fixture.measureStart;endPoint:fixture.measureEnd
                visible:showRuler && referenceVisible
            }
            SamplingRing {
                objectName: "measurementTestRing"
                x: samplePoint.x-width/2; y: samplePoint.y-height/2
                sampleColor: fixture.sampleColor
                previousColor: fixture.previousColor
                visible: showRing
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
    if (!require(root, "create the measurement overlay fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *ring  = root->findChild<QQuickItem *>("measurementTestRing");
    auto *ruler = root->findChild<QQuickItem *>("measurementTestRuler");
    auto *board = root->findChild<QQuickItem *>("measurementTestBoard");
    if (!require(ring && ruler && board, "measurement components exist"))
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
        return compareOverlayReference(actual, reference, bounds, hardware, "/tmp/photocrap-ruler");
    };
    const auto pixel = [&](const QImage &_frame, const QQuickItem *_item, QPointF _point) {
        const auto   scene = _item->mapToScene(_point);
        const qreal  ratio = qreal(_frame.width()) / _window->width();
        const QPoint at(qFloor(scene.x() * ratio), qFloor(scene.y() * ratio));
        return _frame.rect().contains(at) ? _frame.pixelColor(at) : QColor{};
    };
    const auto near = [](QColor _a, QColor _b) {
        return qAbs(_a.red() - _b.red()) <= 2 && qAbs(_a.green() - _b.green()) <= 2 &&
               qAbs(_a.blue() - _b.blue()) <= 2;
    };
    const auto first = frame();
    if (!require(ring->size() == QSizeF(84, 84) && near(pixel(first, board, {200, 45}), Qt::red) &&
                     near(pixel(first, board, {200, 115}), Qt::green) &&
                     near(pixel(first, board, {200, 80}), QColor("#5a7896")),
                 "sampling ring stays local on a 16K image at 16x with both colours and an empty "
                 "centre"))
        return false;
    if (!require(ruler->width() < 220 && ruler->height() < 190 &&
                     near(pixel(first, board, {224, 208}), QColor("#f5f7f9")) &&
                     near(pixel(first, board, {128, 128}), QColor("#f5f7f9")),
                 "short ruler retains its segment and handles in a local backing surface"))
        return false;
    auto      *guides = visualItem("rulerGpuGuides");
    auto      *raster = visualItem("rulerRasterGuides");
    const bool vectorGuides =
        hardware && guides && guides->property("curveRendererAvailable").toBool();
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if (!require(!hardware || vectorGuides, "RHI uses retained GPU ruler guides"))
        return false;
#endif
    if (!require(vectorGuides ? !raster : raster != nullptr,
                 "ruler creates either GPU guides or its raster fallback"))
        return false;
    if (vectorGuides) {
        auto *shape = guides->findChild<QQuickItem *>("rulerGuideShape");
        if (!require(shape && shape->property("rendererType") ==
                                  shape->property("preferredRendererType"),
                     "ruler paths use CurveRenderer"))
            return false;
    }
    if (!require(matchesReference(), "ruler appearance matches the independent raster reference"))
        return false;
    QSignalSpy rulerPaints(ruler, SIGNAL(painted()));
    root->setProperty("measureEnd", QPointF(22, 18));
    const auto changedRuler = frame();
    if (!require((vectorGuides ? rulerPaints.isEmpty() : !rulerPaints.isEmpty()) &&
                     near(pixel(changedRuler, board, {240, 208}), QColor("#f5f7f9")) &&
                     matchesReference(),
                 "moving the ruler updates its line without raster painting on GPU"))
        return false;
    root->setProperty("measureEnd", QPointF(20, 18));
    frame();
    QSignalSpy paints(ring, SIGNAL(painted()));
    if (!require(paints.isValid(), "observe real sampling ring paints"))
        return false;
    root->setProperty("samplePoint", QPointF(280, 80));
    const auto moved = frame();
    if (!require(paints.isEmpty() && ring->size() == QSizeF(84, 84) &&
                     near(pixel(moved, board, {280, 45}), Qt::red) &&
                     near(pixel(moved, board, {200, 45}), QColor("#5a7896")),
                 "moving the ring reuses its texture and clears its former screen position"))
        return false;
    root->setProperty("sampleColor", QColor(Qt::blue));
    const auto recoloured = frame();
    if (!require(!paints.isEmpty() && near(pixel(recoloured, board, {280, 45}), Qt::blue) &&
                     near(pixel(recoloured, board, {280, 115}), Qt::green),
                 "a changed sample repaints its half while preserving the previous colour"))
        return false;
    root->setProperty("showRing", false);
    root->setProperty("previousColor", QColor(Qt::yellow));
    const auto noRing = frame();
    if (!require(near(pixel(noRing, board, {280, 45}), QColor("#5a7896")),
                 "hiding the ring clears its former position"))
        return false;
    root->setProperty("showRing", true);
    const auto restoredRing = frame();
    if (!require(near(pixel(restoredRing, board, {280, 45}), Qt::blue) &&
                     near(pixel(restoredRing, board, {280, 115}), Qt::yellow),
                 "showing the ring uses both current colours after a hidden change"))
        return false;
    root->setProperty("panX", 64);
    root->setProperty("panY", 32);
    const auto scrolled = frame();
    if (!require(ruler->width() <= 512 && ruler->height() <= 320 &&
                     near(pixel(scrolled, board, {224, 208}), QColor("#f5f7f9")),
                 "ruler coordinates survive scrolling of the enlarged document"))
        return false;
    if (!require(matchesReference(), "scrolled ruler preserves the raster reference"))
        return false;
    root->setProperty("panX", 0);
    root->setProperty("panY", 0);
    root->setProperty("viewRotation", 12);
    const auto rotated = frame();
    // Software resamples the thin raster stroke when rotating it; check
    // coverage at its transformed centre rather than requiring an unfiltered white.
    const auto rotatedStroke = pixel(rotated, board, {224, 208});
    if (!require(ruler->width() <= 610 && ruler->height() <= 610 && rotatedStroke.red() > 180 &&
                     rotatedStroke.green() > 180 && rotatedStroke.blue() > 180,
                 "ruler follows canvas rotation without expanding to document dimensions"))
        return false;
    if (!require(matchesReference(), "rotated ruler preserves the raster reference"))
        return false;
    root->setProperty("viewRotation", 0);
    for (qreal z : {.5, 1., 4., 16.}) {
        root->setProperty("measureZoom", z);
        frame();
        if (!require(ruler->width() <= 512 && ruler->height() <= 320 && matchesReference(),
                     "ruler guides keep their size and position through zoom changes"))
            return false;
    }
    root->setProperty("measureEnd", QPointF(2, 2));
    frame();
    if (!require(matchesReference(), "reversed ruler endpoints preserve the line and handles"))
        return false;
    root->setProperty("measureEnd", QPointF(8, 8));
    frame();
    if (!require(matchesReference(), "a zero-length ruler retains its endpoint marker"))
        return false;
    root->setProperty("measureEnd", QPointF(16380, 8));
    root->setProperty("panX", 600);
    const auto longLine = frame();
    if (!require(ruler->width() <= 512 && ruler->height() <= 12 &&
                     near(pixel(longLine, board, {824, 128}), QColor("#f5f7f9")),
                 "a document-wide ruler stays a viewport-clipped narrow strip"))
        return false;
    if (!require(matchesReference(),
                 "a document-wide ruler is clipped without changing its appearance"))
        return false;
    if (!require(vectorGuides ? rulerPaints.isEmpty() : !rulerPaints.isEmpty(),
                 "all ruler updates avoid raster painting only on the GPU path"))
        return false;
    root->setProperty("showRuler", false);
    const auto hidden = frame();
    if (!require(near(pixel(hidden, board, {824, 128}), QColor("#5a7896")),
                 "hiding the ruler clears its former pixels"))
        return false;
    root->setProperty("showRuler", true);
    root->setProperty("panY", 600);
    frame();
    return require(ruler->height() == 0 && ruler->width() <= 512,
                   "a ruler outside the visible area has no backing surface");
}
