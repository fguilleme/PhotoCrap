#pragma once
#include "Document.h"
#include "OverlayReference.h"
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest>
#include <memory>

inline bool runLassoGuideSmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Lasso guide check failed: %s", _message);
        return _ok;
    };
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    const bool expectedGpu =
        QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi());
#else
    const bool expectedGpu = false;
#endif
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
    property point pointer: Qt.point(340,130)
    property bool pointerInside: true
    property real phase: 0
    property real guideOpacity: 1
    Rectangle { anchors.fill: parent; color: "#5a7896" }
    component LegacyGuide: ScreenOverlayCanvas {
        id: legacy
        required property var document
        property real pointerX: 0
        property real pointerY: 0
        property bool pointerInside: true
        property real phase: 0
        function strokeSelectionPath(c) {
            c.lineWidth=1.5; c.setLineDash([5,5]); c.lineDashOffset=phase
            c.strokeStyle="#111111"; c.stroke()
            c.lineDashOffset=phase+5; c.strokeStyle="#ffffff"; c.stroke(); c.setLineDash([])
        }
                                onPaint: {
                                    const points = document.selectionPath
                                    const c = getContext("2d")
                                    c.reset()
                                    // Define the reference's solid initial pen
                                    // independently of prior dash commands.
                                    c.setLineDash([])
                                    c.clearRect(0, 0, width, height)
                                    if (!points || points.length < 1) return
                                    c.translate(-x, -y)
                                    c.beginPath()
                                    c.moveTo(points[0].x * zoom, points[0].y * zoom)
                                    for (let i = 1; i < points.length; ++i)
                                        c.lineTo(points[i].x * zoom, points[i].y * zoom)
                                    if (document.polygonLassoActive && pointerInside)
                                        c.lineTo(pointerX, pointerY)
                                    if (document.hasSelection) {
                                        c.closePath()
                                        strokeSelectionPath(c)
                                    } else {
                                        c.strokeStyle = "#f5f7f9"
                                        c.lineWidth = 1
                                        c.stroke()
                                    }
                                    if (document.polygonLassoActive) {
                                        for (let i = 0; i < points.length; ++i) {
                                            c.beginPath()
                                            c.arc(points[i].x * zoom, points[i].y * zoom, 3, 0,
                                                  Math.PI * 2)
                                            c.fillStyle = "#f5f7f9"
                                            c.fill()
                                            c.strokeStyle = "#172028"
                                            c.lineWidth = 1
                                            c.stroke()
                                        }
                                    }
                                }

        onPhaseChanged: if(visible) requestPaint()
        onPointerXChanged: if(visible) requestPaint()
        onPointerYChanged: if(visible) requestPaint()
        onPointerInsideChanged: if(visible) requestPaint()
        onZoomChanged: if(visible) requestPaint()
        onVisibleChanged: if(visible) requestPaint()
        Connections { target: legacy.document; function onSelectionChanged() {if(legacy.visible)legacy.requestPaint()} }
    }
    Flickable {
        id: view
        anchors.fill: parent; clip: true; interactive: false
        contentWidth: board.width; contentHeight: board.height
        contentX: panX; contentY: panY
        Item {
            id: board
            width: document.width*fixture.zoom; height: document.height*fixture.zoom
            rotation: viewRotation; transformOrigin: Item.TopLeft
            LassoGuide {
                objectName: "lassoTestGuide"
                document: fixture.document; viewportItem: view; zoom: fixture.zoom
                phase: fixture.phase
                opacity: fixture.guideOpacity
                pointerX: fixture.pointer.x*zoom; pointerY: fixture.pointer.y*zoom
                pointerInside: fixture.pointerInside
                visible: !referenceVisible && guideNeeded && points.length>0
            }
            LegacyGuide {
                document: fixture.document; viewportItem: view; zoom: fixture.zoom
                phase: fixture.phase
                opacity: fixture.guideOpacity
                pointerX: fixture.pointer.x*zoom; pointerY: fixture.pointer.y*zoom
                pointerInside: fixture.pointerInside
                visible: referenceVisible && document.selectionEdgesVisible && document.lassoPreviewActive
                    && (document.polygonLassoActive || document.selectionShape!==4 || !document.hasSelection)
                    && document.selectionPath.length>0
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
    if (!require(root, "create the lasso guide fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *guide = root->findChild<QQuickItem *>("lassoTestGuide");
    if (!require(guide, "guide exists"))
        return false;
    const auto cachedPointCount = [&] {
        const auto value = guide->property("points");
        return value.metaType() == QMetaType::fromType<QJSValue>()
                   ? value.value<QJSValue>().property("length").toInt()
                   : int(value.toList().size());
    };
    const auto frame = [&] {
        _window->update();
        QTest::qWait(50);
        return _window->grabWindow();
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
        // A viewport-sized Canvas and a cropped Canvas acquire different
        // subpixel texture origins when rotated, even on the software backend.
        const bool filtered =
            guide->property("gpuGuide").toBool() || root->property("viewRotation").toReal() != 0;
        return compareOverlayReference(actual, reference, bounds, filtered,
                                       "/tmp/photocrap-lasso-guide");
    };
    QSignalSpy pointBounds(guide, SIGNAL(pointBoundsChanged()));
    QSignalSpy guidePaints(guide, SIGNAL(painted())), snapshots(guide, SIGNAL(pointsChanged())),
        prefixes(guide, SIGNAL(fixedPathChanged())),
        markerModels(guide, SIGNAL(visiblePointsChanged()));
    if (!require(pointBounds.isValid() && guidePaints.isValid() && snapshots.isValid() &&
                     prefixes.isValid() && markerModels.isValid(),
                 "observe guide paints and point snapshots"))
        return false;
    for (int bits : {8, 16}) {
        if (!require(document.newDocument(512, 320, false, bits),
                     "create the lasso reference document"))
            return false;
        document.setTool("polygonalLasso");
        for (const auto point : {QPointF(80, 70), QPointF(180, 70), QPointF(240, 220)})
            document.addPolygonLassoPoint(point.x(), point.y());
        const auto original = document.layerImage(0);
        const auto history  = document.historyIndex();
        frame();
        if (!require(matchesReference(), "bounded polygon guide matches the old drawing"))
            return false;
        const bool gpu = guide->property("gpuGuide").toBool();
        if (!require(gpu == expectedGpu,
                     "solid guides actually use CurveRenderer on eligible hardware"))
            return false;
        if (!require(gpu ? !guide->property("rasterActive").toBool()
                         : guide->property("rasterActive").toBool(),
                     "solid GPU guides have no raster context; the fallback paints"))
            return false;
        if (!require(expectedGpu || guide->property("fixedPath").toString().isEmpty(),
                     "software rendering does not prepare unused SVG geometry"))
            return false;
        root->setProperty("pointer", QPointF(210, 110));
        frame();
        if (!require(matchesReference(), "an acute provisional join keeps its full miter"))
            return false;
        root->setProperty("pointer", QPointF(181, 71));
        frame();
        if (!require(!guide->property("gpuGuide").toBool() &&
                         guide->property("rasterActive").toBool() && matchesReference(),
                     "an extremely acute live endpoint uses the original raster join"))
            return false;
        root->setProperty("pointer", QPointF(210, 110));
        document.addPolygonLassoPoint(181, 71);
        frame();
        if (!require(!guide->property("gpuGuide").toBool() &&
                         guide->property("rasterActive").toBool() && matchesReference(),
                     "extremely acute fixed joins keep raster miters and marker drawing order"))
            return false;
        document.removeLastPolygonLassoPoint();
        root->setProperty("pointer", QPointF(340, 130));
        frame();
        if (!require(
                guide->property("gpuGuide").toBool() == gpu &&
                    guide->property("rasterActive").toBool() != gpu && matchesReference(),
                "removing the acute point restores the expected renderer without a stale raster"))
            return false;
        guidePaints.clear();
        snapshots.clear();
        prefixes.clear();
        markerModels.clear();
        pointBounds.clear();
        for (int phase : {1, 3, 6}) {
            root->setProperty("phase", phase);
            frame();
        }
        if (!require(guidePaints.isEmpty() && snapshots.isEmpty(),
                     "solid guides do not repaint or serialize points for dash phases"))
            return false;
        for (const auto point : {QPointF(300, 100), QPointF(110, 60), QPointF(300, 170)}) {
            root->setProperty("pointer", point);
            frame();
        }
        if (!require((gpu ? guidePaints.isEmpty() : guidePaints.count() >= 3) &&
                         snapshots.isEmpty() && prefixes.isEmpty() && markerModels.isEmpty() &&
                         pointBounds.isEmpty() && document.historyIndex() == history &&
                         document.layerImage(0) == original,
                     "polygon hover preserves fixed points and marker models, with no GPU raster "
                     "paints"))
            return false;
        if (!require(cachedPointCount() == 3 && matchesReference(),
                     "moving the polygon tail preserves the guide and its joins"))
            return false;
        root->setProperty("pointerInside", false);
        frame();
        if (!require(matchesReference(), "leaving the image removes the provisional edge"))
            return false;
        root->setProperty("pointerInside", true);
        frame();
        guidePaints.clear();
        document.addPolygonLassoPoint(300, 170);
        frame();
        if (!require((gpu ? guidePaints.isEmpty() : !guidePaints.isEmpty()) &&
                         !snapshots.isEmpty() && matchesReference(),
                     "placing a polygon point refreshes the fixed guide"))
            return false;
        document.removeLastPolygonLassoPoint();
        frame();
        if (!require(matchesReference(), "removing a polygon point restores its previous guide"))
            return false;
        for (qreal zoom : {.5, 2., 8., 64.}) {
            root->setProperty("zoom", zoom);
            root->setProperty("panX", zoom > 1 ? 100 * zoom : 0);
            root->setProperty("panY", zoom == 64 ? 70 * zoom - 40 : zoom > 1 ? 60 * zoom : 0);
            frame();
            if (!require(
                    guide->width() <= 512 && guide->height() <= 320 && matchesReference(),
                    "guide layers stay bounded and preserve pixels through zoom and scrolling"))
                return false;
        }
        root->setProperty("panY", 250. * 64);
        frame();
        if (!require(guide->height() == 0 && !guide->property("gpuGuide").toBool() &&
                         !guide->property("rasterActive").toBool() && matchesReference(),
                     "an entirely offscreen guide allocates neither a GPU layer nor a Canvas"))
            return false;
        root->setProperty("zoom", 1.);
        root->setProperty("panX", 0);
        root->setProperty("panY", 0);
        frame();
        guidePaints.clear();
        root->setProperty("viewRotation", 12);
        frame();
        if (!require(guidePaints.isEmpty() && guide->width() <= 610 && guide->height() <= 610,
                     "rotation moves the local guide texture without repainting unchanged bounds"))
            return false;
        if (!require(matchesReference(), "rotated guides preserve their thin strokes and markers"))
            return false;
        root->setProperty("guideOpacity", .37);
        frame();
        if (!require(matchesReference(), "stroke and markers composite before inherited opacity"))
            return false;
        root->setProperty("guideOpacity", 1.);
        root->setProperty("viewRotation", 0);
        document.finishPolygonLasso();
        frame();
        if (!require(!guide->isVisible() && document.historyIndex() == history + 1 &&
                         document.hasSelection(),
                     "finishing the polygon hides its guide and creates one selection edit"))
            return false;
        document.setTool("lasso");
        document.beginStroke(80, 80);
        document.continueStroke(150, 80);
        frame();
        if (!require(guide->isVisible() && matchesReference(),
                     "the initial freehand line keeps its open guide"))
            return false;
        document.continueStroke(180, 160);
        frame();
        if (!require(!guide->isVisible() && cachedPointCount() == 0,
                     "a shared freehand contour releases the QML point snapshot"))
            return false;
        document.cancelStroke();
        document.setSelectionMode(1);
        document.setTool("lasso");
        document.beginStroke(80, 70);
        document.continueStroke(180, 70);
        document.continueStroke(240, 220);
        frame();
        guidePaints.clear();
        snapshots.clear();
        root->setProperty("phase", 2.25);
        frame();
        if (!require(
                guide->isVisible() && !guide->property("gpuGuide").toBool() &&
                    guide->property("rasterActive").toBool() && !guidePaints.isEmpty() &&
                    snapshots.isEmpty() && matchesReference(),
                "combined freehand candidates keep animated dashes without reconverting points"))
            return false;
        document.cancelStroke();
        document.setSelectionMode(0);
        document.beginStroke(80, 70);
        document.continueStroke(180, 70);
        document.continueStroke(240, 220);
        document.setToolModifiers(false, true, false);
        document.endStroke();
        frame();
        if (!require(document.polygonLassoActive() && document.hasSelection() && matchesReference(),
                     "Alt transition to polygon mode preserves the animated closing candidate"))
            return false;
        document.setSelectionEdgesVisible(false);
        frame();
        if (!require(!guide->isVisible(), "hiding selection edges clears all guide layers"))
            return false;
        const auto hiddenPaints = guidePaints.count();
        root->setProperty("pointer", QPointF(210, 110));
        root->setProperty("phase", 7.5);
        frame();
        if (!require(guidePaints.count() == hiddenPaints, "hidden guide changes do not paint"))
            return false;
        document.setSelectionEdgesVisible(true);
        frame();
        if (!require(matchesReference(), "showing edges restores the current guide"))
            return false;
        document.cancelPolygonLasso();
        frame();
        if (!require(!guide->isVisible() && document.layerImage(0) == original &&
                         document.channelBits() == bits,
                     "cancelling a guide preserves pixels and native precision"))
            return false;
        document.setToolModifiers(false, false, false);
    }
    qInfo("Lasso guide checks passed");
    return true;
}
