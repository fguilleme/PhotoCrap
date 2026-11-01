#pragma once
#include "Document.h"
#include "OverlayReference.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest>
#include <memory>

inline bool runPenOverlaySmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Pen overlay check failed: %s", _message);
        return _ok;
    };
    const bool hardware =
        QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi());
    bool          vectorCurve = false;
    Document      document;
    QQmlComponent component(qmlEngine(_window));
    component.setData(R"(
import QtQuick
import PhotoCrap
Item {
    id: fixture
    required property var document
    width: 512; height: 320
    property bool referenceVisible: false
    property real zoom: 1
    property real panX: 0
    property real panY: 0
    property real viewRotation: 0
    Rectangle { anchors.fill: parent; color: "#5a7896" }
component LegacyPenOverlay: ScreenOverlayCanvas {
    id: root
    required property var document
    enabled: false
    antialiasing: true

    // Keep the QVariant snapshot until the path changes, independently of hover.
    readonly property var activePath: {
        const paths = document.penPaths, index = document.activePenPath
        return index >= 0 && index < paths.length ? paths[index] : null
    }
    readonly property var points: activePath ? activePath.points : []
    readonly property var curveHull: {
        const hull = []
        for (const p of points)
            hull.push(p, {x:p.inX, y:p.inY}, {x:p.outX, y:p.outY})
        return hull
    }
    readonly property var handleHull: {
        const hull = []
        for (const p of points) {
            hull.push(p)
            if (p.selected)
                hull.push({x:p.inX, y:p.inY}, {x:p.outX, y:p.outY})
        }
        return hull
    }
    function clippedBounds(hull, margin) {
        if (!hull.length)
            return Qt.rect(0,0,0,0)
        let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity
        for (const p of hull) {
            left = Math.min(left, p.x*zoom); top = Math.min(top, p.y*zoom)
            right = Math.max(right, p.x*zoom); bottom = Math.max(bottom, p.y*zoom)
        }
        left = Math.max(screenBounds.x, Math.floor(left-margin))
        top = Math.max(screenBounds.y, Math.floor(top-margin))
        right = Math.min(screenBounds.x+screenBounds.width, Math.ceil(right+margin))
        bottom = Math.min(screenBounds.y+screenBounds.height, Math.ceil(bottom+margin))
        return Qt.rect(left, top, Math.max(0,right-left), Math.max(0,bottom-top))
    }
    readonly property rect pathBounds: clippedBounds(curveHull, 3)
    readonly property rect handlesBounds: clippedBounds(handleHull, 5)
    readonly property rect rubberBounds: {
        if (!points.length)
            return Qt.rect(0,0,0,0)
        const last = points[points.length-1]
        return clippedBounds([last, {x:last.outX,y:last.outY}, document.penHover], 3)
    }
    x: pathBounds.x; y: pathBounds.y
    width: pathBounds.width; height: pathBounds.height

    function repaintPath() {
        if (!visible) return
        requestPaint()
        if (handles) handles.requestPaint()
        if (rubberBand && rubberBand.visible) rubberBand.requestPaint()
    }
    onVisibleChanged: repaintPath()
    onZoomChanged: repaintPath()
    onPaint: {
        const c = getContext("2d")
        c.reset(); c.clearRect(0,0,width,height)
        if (!points.length) return
        c.translate(-x,-y)
        function segment(a,b) {
            c.bezierCurveTo(a.outX*zoom,a.outY*zoom,b.inX*zoom,b.inY*zoom,b.x*zoom,b.y*zoom)
        }
        c.beginPath(); c.moveTo(points[0].x*zoom,points[0].y*zoom)
        for (let i=1; i<points.length; ++i) segment(points[i-1],points[i])
        if (activePath.closed) segment(points[points.length-1],points[0])
        c.strokeStyle="#1a2b3b"; c.lineWidth=3; c.stroke()
        c.strokeStyle="#73c5ff"; c.lineWidth=1.5; c.stroke()
    }
    Canvas {
        id: rubberBand
        objectName: "penReferenceRubberBand"
        enabled: false; antialiasing: true; z: 1
        visible: root.document.penDrawing && root.document.penRubberBand
                 && root.activePath !== null && !root.activePath.closed && root.points.length > 0
        x: root.rubberBounds.x-root.x; y: root.rubberBounds.y-root.y
        width: root.rubberBounds.width; height: root.rubberBounds.height
        onWidthChanged: if (visible) requestPaint()
        onHeightChanged: if (visible) requestPaint()
        onVisibleChanged: if (visible) requestPaint()
        onPaint: {
            const c = getContext("2d")
            c.reset(); c.clearRect(0,0,width,height)
            if (!root.points.length) return
            c.translate(-root.rubberBounds.x,-root.rubberBounds.y)
            const last=root.points[root.points.length-1], to=root.document.penHover
            c.beginPath(); c.moveTo(last.x*root.zoom,last.y*root.zoom)
            c.bezierCurveTo(last.outX*root.zoom,last.outY*root.zoom,
                            to.x*root.zoom,to.y*root.zoom,to.x*root.zoom,to.y*root.zoom)
            c.setLineDash([4,3]); c.strokeStyle="#73c5ff"; c.lineWidth=1.5; c.stroke()
        }
    }
    Canvas {
        id: handles
        objectName: "penReferenceHandles"
        enabled: false; antialiasing: true; z: 2
        visible: root.points.length > 0
        x: root.handlesBounds.x-root.x; y: root.handlesBounds.y-root.y
        width: root.handlesBounds.width; height: root.handlesBounds.height
        onWidthChanged: if (visible) requestPaint()
        onHeightChanged: if (visible) requestPaint()
        onVisibleChanged: if (visible) requestPaint()
        onPaint: {
            const c = getContext("2d")
            c.reset(); c.clearRect(0,0,width,height)
            c.translate(-root.handlesBounds.x,-root.handlesBounds.y)
            for (const a of root.points) {
                const px=a.x*root.zoom, py=a.y*root.zoom
                if (a.selected) {
                    for (const h of [{x:a.inX*root.zoom,y:a.inY*root.zoom},
                                     {x:a.outX*root.zoom,y:a.outY*root.zoom}]) {
                        if (Math.hypot(h.x-px,h.y-py)<.1) continue
                        c.beginPath(); c.moveTo(px,py); c.lineTo(h.x,h.y)
                        c.strokeStyle="#73c5ff"; c.lineWidth=1; c.stroke()
                        c.beginPath(); c.arc(h.x,h.y,3,0,2*Math.PI)
                        c.fillStyle="#eef8ff"; c.fill(); c.strokeStyle="#1b4968"; c.stroke()
                    }
                }
                c.fillStyle=a.selected?"#73c5ff":"#ffffff"; c.fillRect(px-3,py-3,6,6)
                c.strokeStyle="#183e59"; c.lineWidth=1; c.strokeRect(px-3,py-3,6,6)
            }
        }
    }
    onHandlesBoundsChanged: if (handles && handles.visible) handles.requestPaint()
    onRubberBoundsChanged: if (rubberBand && rubberBand.visible) rubberBand.requestPaint()
    Connections {
        target: root.document
        function onPenPathsChanged() { root.repaintPath() }
        function onPenHoverChanged() { if (rubberBand.visible) rubberBand.requestPaint() }
    }
}


    Flickable {
        id: view
        anchors.fill: parent; clip: true; interactive: false
        contentWidth: board.width; contentHeight: board.height
        contentX: panX; contentY: panY
        Item {
            id: board
            objectName: "penTestBoard"
            width: document.width*fixture.zoom; height: document.height*fixture.zoom
            rotation: viewRotation; transformOrigin: Item.TopLeft
            PenOverlay {
                objectName: "penTestOverlay"
                document: fixture.document; viewportItem: view; zoom: fixture.zoom
                visible: !referenceVisible && document.activePenPath>=0
            }
            LegacyPenOverlay {
                objectName: "penTestReference"
                document: fixture.document; viewportItem: view; zoom: fixture.zoom
                visible: referenceVisible && document.activePenPath>=0
            }
        }
    }
}
)",
                      QUrl());
    if (!require(component.isReady(), qPrintable(component.errorString())))
        return false;
    std::unique_ptr<QObject> object(
        component.createWithInitialProperties({{"document", QVariant::fromValue(&document)}}));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!require(root, "create the pen overlay fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *overlay = root->findChild<QQuickItem *>("penTestOverlay");
    auto *handles = root->findChild<QQuickItem *>("penHandlesOverlay");
    auto *band    = root->findChild<QQuickItem *>("penRubberBandOverlay");
    if (!require(overlay && handles && band, "pen overlay layers exist"))
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
        const qreal ratio  = qreal(actual.width()) / _window->width();
        const auto  origin = root->mapToScene(QPointF{});
        const QRect bounds(qFloor(origin.x() * ratio), qFloor(origin.y() * ratio),
                           qFloor(root->width() * ratio), qFloor(root->height() * ratio));
        return compareOverlayReference(actual, reference, bounds, vectorCurve,
                                       "/tmp/photocrap-pen-curve");
    };
    QSignalSpy pathPaints(overlay, SIGNAL(painted()));
    QSignalSpy handlePaints(handles, SIGNAL(painted()));
    QSignalSpy bandPaints(band, SIGNAL(painted()));
    QSignalSpy snapshots(overlay, SIGNAL(activePathChanged()));
    QSignalSpy paths(overlay, SIGNAL(curvePathChanged()));
    if (!require(pathPaints.isValid() && handlePaints.isValid() && bandPaints.isValid() &&
                     snapshots.isValid() && paths.isValid(),
                 "observe layer painting and native path snapshots"))
        return false;
    for (int bits : {8, 16}) {
        if (!require(document.newDocument(512, 320, false, bits),
                     "create the pen reference document"))
            return false;
        document.setTool("pen");
        document.setPenRubberBand(true);
        for (const auto point : {QPointF(80, 70), QPointF(180, 70), QPointF(240, 220)}) {
            document.beginPenGesture(point.x(), point.y());
            if (point.x() > 100)
                document.updatePenGesture(point.x() + 30, point.y() + 30);
            document.endPenGesture();
        }
        document.updatePenHover(340, 130);
        const auto original = document.layerImage(0);
        const auto history  = document.historyIndex();
        frame();
        auto *curve  = visualItem("penGpuCurve");
        auto *raster = visualItem("penRasterCurve");
        vectorCurve  = hardware && curve && curve->property("curveRendererAvailable").toBool();
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        if (!require(!hardware || vectorCurve, "RHI uses retained GPU pen curves"))
            return false;
#endif
        if (!require(vectorCurve ? !raster && curve->property("rendererType") ==
                                                  curve->property("preferredRendererType")
                                 : raster != nullptr,
                     "only the main pen curve switches between CurveRenderer and Canvas"))
            return false;
        if (!require(matchesReference(),
                     "separate curve, rubber band and handles match the original drawing"))
            return false;
        pathPaints.clear();
        handlePaints.clear();
        bandPaints.clear();
        snapshots.clear();
        paths.clear();
        const auto *retainedCurve = visualItem("penGpuCurve");
        const auto  beforeHover   = frame();
        for (const auto point : {QPointF(300, 100), QPointF(110, 60), QPointF(300, 170)}) {
            document.updatePenHover(point.x(), point.y());
            frame();
        }
        if (!require(
                pathPaints.isEmpty() && handlePaints.isEmpty() && snapshots.isEmpty() &&
                    paths.isEmpty() && visualItem("penGpuCurve") == retainedCurve &&
                    bandPaints.count() >= 3 && frame() != beforeHover && band->width() < 512 &&
                    band->height() < 320 && document.historyIndex() == history &&
                    document.layerImage(0) == original,
                "hover repaints only the local provisional curve without path conversion or edits"))
            return false;
        if (!require(matchesReference(),
                     "a moved provisional curve preserves draw order at crossings"))
            return false;
        pathPaints.clear();
        handlePaints.clear();
        bandPaints.clear();
        document.setPenRubberBand(false);
        frame();
        document.updatePenHover(200, 90);
        frame();
        if (!require(!band->isVisible() && pathPaints.isEmpty() && handlePaints.isEmpty() &&
                         bandPaints.isEmpty(),
                     "disabling the guide avoids all hover paints"))
            return false;
        if (!require(matchesReference(), "hiding the guide clears it without changing the path"))
            return false;
        document.setPenRubberBand(true);
        frame();
        pathPaints.clear();
        handlePaints.clear();
        document.beginPenGesture(80, 70, 6, true);
        document.updatePenGesture(90, 85);
        document.endPenGesture();
        frame();
        if (!require((vectorCurve ? pathPaints.isEmpty() : !pathPaints.isEmpty()) &&
                         !handlePaints.isEmpty() && matchesReference(),
                     "anchor edits update GPU curves without raster painting and refresh handles"))
            return false;
        document.undo();
        frame();
        if (!require(matchesReference(), "undo restores curve, handles and guide"))
            return false;
        for (qreal zoom : {.5, 2., 8.}) {
            root->setProperty("zoom", zoom);
            root->setProperty("panX", zoom > 1 ? 100 * zoom : 0);
            root->setProperty("panY", zoom > 1 ? 60 * zoom : 0);
            frame();
            if (!require(
                    overlay->width() <= 512 && overlay->height() <= 320 &&
                        handles->width() <= 512 && handles->height() <= 320 &&
                        band->width() <= 512 && band->height() <= 320 && matchesReference(),
                    "all pen layers stay bounded and preserve pixels through zoom and scrolling"))
                return false;
        }
        root->setProperty("zoom", 64.);
        root->setProperty("panX", 82 * 64.);
        root->setProperty("panY", 64 * 64.);
        frame();
        if (!require(overlay->width() <= 512 && overlay->height() <= 320 && matchesReference(),
                     "a 32768-pixel artboard keeps the main curve bounded to the visible region"))
            return false;
        root->setProperty("zoom", 8.);
        root->setProperty("panX", 0);
        root->setProperty("panY", 0);
        document.updatePenHover(20, 20);
        frame();
        if (!require((overlay->width() == 0 || overlay->height() == 0) && band->width() > 0 &&
                         band->height() > 0 && matchesReference(),
                     "a provisional curve remains visible when its retained path is offscreen"))
            return false;
        root->setProperty("zoom", 1.);
        root->setProperty("panX", 0);
        root->setProperty("panY", 0);
        root->setProperty("viewRotation", 12);
        frame();
        if (!require(overlay->width() <= 610 && overlay->height() <= 610 &&
                         handles->width() <= 610 && handles->height() <= 610 && band->isVisible() &&
                         matchesReference(),
                     "rotated pen layers keep their bounds and preserve the raster reference"))
            return false;
        root->setProperty("viewRotation", 0);
        root->setProperty("panX", 45.25);
        root->setProperty("panY", 20.75);
        frame();
        if (!require(matchesReference(), "fractional scrolling preserves pen strokes"))
            return false;
        root->setProperty("panX", 0.);
        root->setProperty("panY", 0.);
        document.closePenPath();
        frame();
        if (!require(!band->isVisible() && matchesReference(),
                     "closing the path drops the provisional curve"))
            return false;
        pathPaints.clear();
        handlePaints.clear();
        bandPaints.clear();
        snapshots.clear();
        paths.clear();
        document.updatePenHover(300, 180);
        frame();
        if (!require(pathPaints.isEmpty() && handlePaints.isEmpty() && bandPaints.isEmpty() &&
                         snapshots.isEmpty() && paths.isEmpty(),
                     "closed paths do not repaint or reconvert on hover"))
            return false;
        document.setActivePenPath(-1);
        frame();
        if (!require(!overlay->isVisible() && !band->isVisible() && !handles->isVisible() &&
                         document.layerImage(0) == original,
                     "hiding the path clears all layers and preserves pixels"))
            return false;
        document.setActivePenPath(0);
        frame();
        if (!require(overlay->isVisible() && matchesReference(),
                     "reactivating the pen path restores the retained curve"))
            return false;
        document.newPenPath();
        document.beginPenGesture(80, 70);
        document.endPenGesture();
        frame();
        if (!require(matchesReference(), "a lone anchor does not create a spurious curve"))
            return false;
        for (const auto point : {QPointF(240, 220), QPointF(80, 220), QPointF(240, 70)}) {
            document.beginPenGesture(point.x(), point.y());
            document.endPenGesture();
        }
        document.closePenPath();
        frame();
        if (!require(matchesReference(),
                     "straight segments, sharp joins and crossings remain intact"))
            return false;
        if (!require(document.layerImage(0) == original,
                     "all overlay views and path edits preserve the document pixels"))
            return false;
    }
    return true;
}
