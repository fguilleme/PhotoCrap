#pragma once
#include "DashedGuideItem.h"
#include "Document.h"
#include "OverlayReference.h"
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest>
#include <limits>
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
        QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi()) &&
        _window->devicePixelRatio() == qRound(_window->devicePixelRatio());
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
        // subpixel texture origins when rotated or on a fractional screen.
        // Fractional artboard dimensions also round their Canvas textures,
        // even on the software backend.
        const bool filtered = guide->property("gpuGuide").toBool() ||
                              root->property("viewRotation").toReal() != 0 ||
                              ratio != qRound(ratio) || guide->width() != qRound(guide->width()) ||
                              guide->height() != qRound(guide->height());
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
        root->setProperty("pointer", QPointF(340, 130));
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
        if (!require((!guide->property("integralScreenScale").toBool() || guidePaints.isEmpty()) &&
                         guide->width() <= 610 && guide->height() <= 610,
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
        document.beginStroke(80, 70);
        document.continueStroke(240, 70);
        document.continueStroke(240, 220);
        document.continueStroke(80, 220);
        frame();
        const bool dashedGpu =
            QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi());
        if (!require(guide->property("gpuDashedGuide").toBool() == dashedGpu &&
                         guide->property("rasterActive").toBool() != dashedGpu &&
                         matchesReference(),
                     "closed combined candidates use retained GPU dashes with a software fallback"))
            return false;
        const auto findDashed = [](auto &&self, QQuickItem *item) -> DashedGuideItem * {
            if (auto *dashed = qobject_cast<DashedGuideItem *>(item))
                return dashed;
            for (auto *child : item->childItems())
                if (auto *dashed = self(self, child))
                    return dashed;
            return nullptr;
        };
        auto      *dashed  = findDashed(findDashed, guide);
        const auto builds  = dashed ? dashed->geometryBuilds() : 0;
        const auto uploads = dashed ? dashed->geometryUploads() : 0;
        const auto updates = dashed ? dashed->parameterUpdates() : 0;
        guidePaints.clear();
        snapshots.clear();
        const auto firstDash = frame();
        for (qreal phase : {3.125, 4.5, 7.75}) {
            root->setProperty("phase", phase);
            frame();
        }
        if (!require(snapshots.isEmpty() &&
                         (dashedGpu
                              ? dashed && dashed->shaderActive() && uploads > 0 &&
                                    dashed->geometryBuilds() == builds &&
                                    dashed->geometryUploads() == uploads &&
                                    dashed->parameterUpdates() > updates && guidePaints.isEmpty()
                              : !guidePaints.isEmpty()) &&
                         firstDash != frame(),
                     "dash animation updates uniforms without rebuilding or uploading geometry"))
            return false;
        if (!require(matchesReference(), "fractional dash phase preserves the Canvas appearance"))
            return false;
        root->setProperty("zoom", .005);
        frame();
        if (!require(!guide->property("gpuDashedGuide").toBool() &&
                         guide->property("rasterActive").toBool() && matchesReference(),
                     "subpixel corners with overlapping fringes retain the raster stroke"))
            return false;
        root->setProperty("zoom", 1.);
        frame();
        for (qreal phase : {0., 1.5, 5., -1.25, 10.}) {
            root->setProperty("phase", phase);
            frame();
            if (!require(matchesReference(), "dash boundaries and closure keep their flat caps"))
                return false;
        }
        root->setProperty("zoom", 8.);
        root->setProperty("panX", 1000.);
        root->setProperty("panY", 800.);
        frame();
        frame();
        auto      *emptyGuide      = findDashed(findDashed, guide);
        const auto emptyBuilds     = emptyGuide ? emptyGuide->geometryBuilds() : 0;
        const auto emptyRenders    = emptyGuide ? emptyGuide->renderUpdates() : 0;
        const auto emptyUploads    = emptyGuide ? emptyGuide->geometryUploads() : 0;
        const auto emptyParameters = emptyGuide ? emptyGuide->parameterUpdates() : 0;
        guidePaints.clear();
        snapshots.clear();
        for (qreal phase : {1.25, 4.75, -2.5}) {
            root->setProperty("phase", phase);
            frame();
        }
        if (!require(guide->width() > 0 && guide->height() > 0 && snapshots.isEmpty() &&
                         (dashedGpu
                              ? emptyGuide && emptyGuide->supported() &&
                                    !emptyGuide->hasGeometry() && !emptyGuide->shaderActive() &&
                                    guide->property("gpuDashedGuide").toBool() &&
                                    !guide->property("rasterActive").toBool() &&
                                    guidePaints.isEmpty() &&
                                    !QQmlProperty(emptyGuide->parentItem(), "layer.enabled")
                                         .read()
                                         .toBool() &&
                                    emptyGuide->geometryBuilds() == emptyBuilds &&
                                    emptyGuide->renderUpdates() == emptyRenders &&
                                    emptyGuide->geometryUploads() == emptyUploads &&
                                    emptyGuide->parameterUpdates() == emptyParameters
                              : !guidePaints.isEmpty()),
                     "a contour surrounding the view keeps no composition layer or per-phase "
                     "render work"))
            return false;
        root->setProperty("panX", 620.);
        frame();
        auto *returnedGuide = findDashed(findDashed, guide);
        if (!require((!dashedGpu ||
                      (returnedGuide == emptyGuide && returnedGuide->hasGeometry() &&
                       returnedGuide->shaderActive() &&
                       QQmlProperty(returnedGuide->parentItem(), "layer.enabled").read().toBool() &&
                       returnedGuide->phase() == -2.5)) &&
                         matchesReference(),
                     "scrolling back reuses the item and restores the visible edge with the latest "
                     "phase"))
            return false;
        root->setProperty("panX", 1000.);
        frame();
        if (!require(matchesReference(),
                     "a surrounding contour leaves the viewport pixels unchanged"))
            return false;
        root->setProperty("phase", 10.);
        root->setProperty("panX", 0.);
        root->setProperty("panY", 0.);
        for (qreal zoom : {.5, 2., 64.}) {
            root->setProperty("zoom", zoom);
            root->setProperty("panX", zoom == 64 ? 80 * zoom - 20 : zoom > 1 ? 100 : 0);
            root->setProperty("panY", zoom == 64 ? 70 * zoom - 40 : 0);
            frame();
            if (!require(guide->width() <= 512 && guide->height() <= 320 && matchesReference(),
                         "clipped dashes retain logical width and phase at high zoom"))
                return false;
        }
        root->setProperty("panY", 250. * 64);
        frame();
        if (!require(!guide->property("gpuGuide").toBool() &&
                         !guide->property("rasterActive").toBool(),
                     "an offscreen dashed candidate releases both renderers"))
            return false;
        root->setProperty("zoom", 1.);
        root->setProperty("panX", 0);
        root->setProperty("panY", 0);
        root->setProperty("phase", 2.25);
        root->setProperty("viewRotation", 12);
        root->setProperty("guideOpacity", .37);
        frame();
        if (!require(matchesReference(),
                     "dashed passes compose before rotation and inherited opacity"))
            return false;
        root->setProperty("viewRotation", 0);
        root->setProperty("guideOpacity", 1.);
        document.cancelStroke();
        document.beginStroke(80, 70);
        document.continueStroke(240, 220);
        document.continueStroke(80, 220);
        document.continueStroke(240, 70);
        frame();
        if (!require(!guide->property("gpuDashedGuide").toBool() && matchesReference(),
                     "acute crossed corners retain QPainter's exact dash joins"))
            return false;
        document.cancelStroke();
        document.beginStroke(80, 70);
        for (const auto &p :
             {QPointF(240, 70), QPointF(240, 220), QPointF(140, 220), QPointF(140, 30),
              QPointF(280, 30), QPointF(280, 150), QPointF(80, 150)})
            document.continueStroke(p.x(), p.y());
        frame();
        if (!require(guide->property("gpuDashedGuide").toBool() == dashedGpu && matchesReference(),
                     "self-intersections preserve the global black-then-white stroke order"))
            return false;
        document.setSelectionEdgesVisible(false);
        frame();
        if (!require(!findDashed(findDashed, guide) && !guide->property("rasterActive").toBool(),
                     "hiding dashes destroys their native item and raster fallback"))
            return false;
        document.setSelectionEdgesVisible(true);
        frame();
        if (!require(matchesReference(), "showing dashes restores the latest candidate"))
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
    DashedGuideItem bounded(root);
    bounded.setSize({100, 100});
    bounded.setOpacity(0);
    const QVariantList square{QPointF(10, 10), QPointF(90, 10), QPointF(90, 90), QPointF(10, 90)};
    bounded.setPoints(square);
    frame();
    if (!require(bounded.supported(), "valid native points prepare a bounded dash mesh"))
        return false;
    QVariantList oversized;
    oversized.fill(QPointF(10, 10), 8193);
    bounded.setPoints(oversized);
    frame();
    if (!require(!bounded.supported(), "oversized input falls back before allocating a mesh"))
        return false;
    auto invalid = square;
    invalid[1]   = QPointF(std::numeric_limits<qreal>::quiet_NaN(), 10);
    bounded.setPoints(invalid);
    frame();
    if (!require(!bounded.supported(), "nonfinite input is rejected"))
        return false;
    bounded.setPoints(square);
    bounded.setZoom(2);
    frame();
    if (!require(bounded.supported(), "valid input restores the native path after a fallback"))
        return false;
    qInfo("Lasso guide checks passed");
    return true;
}
