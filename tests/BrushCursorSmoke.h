#pragma once
#include "OverlayReference.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtMath>
#include <QtTest>
#include <cmath>
#include <memory>

inline bool runBrushCursorSmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Brush cursor check failed: %s", _message);
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
    property point pointer: Qt.point(200,160)
    property real diameter: 80
    property real roundness: 1
    property real angle: 0
    property int sign: 0
    property real panX: 0
    property real boardX: 0
    property real viewRotation: 0
    property bool showCursor: true
    property bool referenceVisible: false
    property real cursorOpacity: 1
    Rectangle { anchors.fill: parent; color: "#5a7896" }
component LegacyBrushCursor: Canvas {
    id: root
    required property var viewportItem
    property real zoom: 1
    property real pointerX: 0
    property real pointerY: 0
    property real diameter: 24
    property real roundness: 1
    property real angle: 0
    property int selectionSign: 0
    enabled: false

    readonly property rect viewportBounds: {
        if (!parent || !viewportItem)
            return Qt.rect(0, 0, 0, 0)
        const scrollX = viewportItem.contentX, scrollY = viewportItem.contentY
        const scale = zoom, rotation = parent.rotation
        const originX = parent.x, originY = parent.y
        const sourceWidth = parent.width, sourceHeight = parent.height
        const w = viewportItem.width, h = viewportItem.height
        const points = [viewportItem.mapToItem(parent, 0, 0),
                        viewportItem.mapToItem(parent, w, 0),
                        viewportItem.mapToItem(parent, w, h),
                        viewportItem.mapToItem(parent, 0, h)]
        // Include canvas margins: a brush near the image edge keeps its whole
        // footprint whenever that footprint is still inside the viewport.
        const left = Math.floor(Math.min.apply(null, points.map(p => p.x)))
        const top = Math.floor(Math.min.apply(null, points.map(p => p.y)))
        const right = Math.ceil(Math.max.apply(null, points.map(p => p.x)))
        const bottom = Math.ceil(Math.max.apply(null, points.map(p => p.y)))
        return Qt.rect(left, top, Math.max(0, right-left), Math.max(0, bottom-top))
    }
    readonly property real fullSize: Math.max(12, diameter+12)
    readonly property real clipLeft: Math.max(0, viewportBounds.x-(pointerX-fullSize/2))
    readonly property real clipTop: Math.max(0, viewportBounds.y-(pointerY-fullSize/2))
    readonly property real clipRight: Math.max(0, pointerX+fullSize/2-viewportBounds.x-viewportBounds.width)
    readonly property real clipBottom: Math.max(0, pointerY+fullSize/2-viewportBounds.y-viewportBounds.height)
    readonly property real localCenterX: fullSize/2-clipLeft
    readonly property real localCenterY: fullSize/2-clipTop
    x: pointerX-fullSize/2+clipLeft
    y: pointerY-fullSize/2+clipTop
    width: Math.max(0, fullSize-clipLeft-clipRight)
    height: Math.max(0, fullSize-clipTop-clipBottom)

    onLocalCenterXChanged: if (visible) requestPaint()
    onLocalCenterYChanged: if (visible) requestPaint()
    onWidthChanged: if (visible) requestPaint()
    onHeightChanged: if (visible) requestPaint()
    onDiameterChanged: if (visible) requestPaint()
    onRoundnessChanged: if (visible) requestPaint()
    onAngleChanged: if (visible) requestPaint()
    onSelectionSignChanged: if (visible) requestPaint()
    onVisibleChanged: if (visible) requestPaint()
    onPaint: {
        const c = getContext("2d")
        c.reset()
        c.clearRect(0, 0, width, height)
        c.translate(localCenterX, localCenterY)
        c.save()
        const ratio = Math.max(.01, Math.min(1, roundness))
        c.rotate(angle*Math.PI/180)
        c.scale(1, ratio)
        const radius = diameter/2
        c.beginPath()
        c.arc(0, 0, radius, 0, Math.PI*2)
        c.lineWidth = 2/ratio
        c.strokeStyle = "rgba(0,0,0,.75)"
        c.stroke()
        c.lineWidth = 1/ratio
        c.strokeStyle = "rgba(255,255,255,.95)"
        c.stroke()
        c.restore()
        if (selectionSign !== 0) {
            // Keep the sign upright and inside the footprint's local bounds.
            const px = radius*.5, py = -radius*.5
            c.beginPath()
            c.moveTo(px-3, py)
            c.lineTo(px+3, py)
            if (selectionSign > 0) {
                c.moveTo(px, py-3)
                c.lineTo(px, py+3)
            }
            c.lineWidth = 3
            c.strokeStyle = "rgba(0,0,0,.85)"
            c.stroke()
            c.lineWidth = 1
            c.strokeStyle = "#ffffff"
            c.stroke()
        }
    }
}

    Flickable {
        id: view
        anchors.fill: parent
        contentWidth: board.width; contentHeight: board.height
        contentX: panX
        clip: true; interactive: false
        Item {
            id: board
            objectName: "brushTestBoard"
            x: fixture.boardX
            width: 16384*64; height: 16384*64
            rotation: viewRotation; transformOrigin: Item.TopLeft
            BrushCursor {
                objectName: "brushTestCursor"
                viewportItem: view
                pointerX: fixture.pointer.x; pointerY: fixture.pointer.y
                diameter: fixture.diameter
                roundness: fixture.roundness
                angle: fixture.angle
                selectionSign: fixture.sign
                visible: showCursor && !referenceVisible
                opacity: cursorOpacity
            }
            LegacyBrushCursor {
                viewportItem: view
                pointerX: fixture.pointer.x; pointerY: fixture.pointer.y
                diameter: fixture.diameter
                roundness: fixture.roundness
                angle: fixture.angle
                selectionSign: fixture.sign
                visible: showCursor && referenceVisible
                opacity: cursorOpacity
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
    if (!require(root, "create the brush cursor fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *cursor = root->findChild<QQuickItem *>("brushTestCursor");
    auto *board  = root->findChild<QQuickItem *>("brushTestBoard");
    if (!require(cursor && board, "brush cursor components exist"))
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
        return compareOverlayReference(actual, reference, bounds, hardware, "/tmp/photocrap-brush");
    };
    const auto pixel = [&](const QImage &_frame, QPointF _point) {
        const auto   scene = board->mapToScene(_point);
        const qreal  ratio = qreal(_frame.width()) / _window->width();
        const QPoint at(qFloor(scene.x() * ratio), qFloor(scene.y() * ratio));
        return _frame.rect().contains(at) ? _frame.pixelColor(at) : QColor{};
    };
    const auto near = [](QColor _a, QColor _b) {
        return qAbs(_a.red() - _b.red()) <= 2 && qAbs(_a.green() - _b.green()) <= 2 &&
               qAbs(_a.blue() - _b.blue()) <= 2;
    };
    const QColor base("#5a7896");
    const auto   markedNear = [&](const QImage &_frame, QPointF _point) {
        for (int y = -2; y <= 2; ++y)
            for (int x = -2; x <= 2; ++x) {
                const auto value = pixel(_frame, _point + QPointF(x + .5, y + .5));
                if (value.isValid() && !near(value, base))
                    return true;
            }
        return false;
    };
    const auto first = frame();
    if (!require(cursor->width() < 100 && cursor->height() < 100 && markedNear(first, {240, 160}) &&
                     markedNear(first, {200, 120}) && near(pixel(first, {200, 160}), base),
                 "round footprint keeps its border and transparent centre in a local surface"))
        return false;
    if (!require(hardware
                     ? visualItem("brushCursorShader") && cursor->property("gpuReady").toBool() &&
                           !visualItem("brushCursorRaster")
                     : visualItem("brushCursorRaster") != nullptr,
                 "the brush uses a compiled shader on RHI and a bounded Canvas on software"))
        return false;
    if (!require(matchesReference(), "round brush matches the independent Canvas reference"))
        return false;
    frame();
    QSignalSpy paints(cursor, SIGNAL(painted()));
    if (!require(paints.isValid(), "observe real brush cursor paints"))
        return false;
    root->setProperty("pointer", QPointF(240, 160));
    const auto moved = frame();
    if (!require(paints.isEmpty() && markedNear(moved, {280, 160}) &&
                     near(pixel(moved, {160, 160}), base),
                 "unclipped pointer motion avoids painting and clears the former position"))
        return false;
    root->setProperty("angle", 30);
    root->setProperty("roundness", .25);
    const auto    ellipse = frame();
    const qreal   radians = qDegreesToRadians(30.);
    const QPointF major(240 + 40 * std::cos(radians), 160 + 40 * std::sin(radians));
    const QPointF minor(240 - 10 * std::sin(radians), 160 + 10 * std::cos(radians));
    if (!require((hardware ? paints.isEmpty() : !paints.isEmpty()) && markedNear(ellipse, major) &&
                     markedNear(ellipse, minor) && near(pixel(ellipse, {280, 160}), base),
                 "angle and roundness update the elliptical outline without filling its interior"))
        return false;
    if (!require(matchesReference(), "rotated elliptical brush preserves the raster reference"))
        return false;
    paints.clear();
    root->setProperty("viewRotation", 12);
    const auto rotated = frame();
    if (!require(paints.isEmpty() && markedNear(rotated, major),
                 "canvas rotation moves the cached footprint through the scene graph"))
        return false;
    if (!require(matchesReference(), "view rotation preserves the elliptical cursor reference"))
        return false;
    root->setProperty("cursorOpacity", .37);
    frame();
    if (!require(matchesReference(), "cursor opacity composes the outline only once"))
        return false;
    root->setProperty("cursorOpacity", 1.);
    root->setProperty("viewRotation", 0);
    root->setProperty("angle", 0);
    root->setProperty("roundness", 1);
    root->setProperty("sign", 1);
    const auto plus = frame();
    if (!require(markedNear(plus, {260, 140}) && !near(pixel(plus, {260.5, 142.5}), base),
                 "quick selection add sign stays visible inside the cursor bounds"))
        return false;
    if (!require(matchesReference(), "quick selection plus matches the raster reference"))
        return false;
    root->setProperty("angle", 70.);
    root->setProperty("roundness", .15);
    frame();
    if (!require(matchesReference(), "the add sign stays upright for a flattened angled brush"))
        return false;
    root->setProperty("angle", 0.);
    root->setProperty("roundness", 1.);
    root->setProperty("sign", -1);
    const auto minus = frame();
    if (!require(markedNear(minus, {260, 140}) && near(pixel(minus, {260, 142.5}), base),
                 "quick selection subtract sign drops the vertical leg"))
        return false;
    if (!require(matchesReference(), "quick selection minus matches the raster reference"))
        return false;
    root->setProperty("sign", 0);
    root->setProperty("diameter", 120);
    root->setProperty("pointer", QPointF(120, 160));
    const auto unclipped = frame();
    root->setProperty("pointer", QPointF(-20, 160));
    const auto clipped = frame();
    if (!require(cursor->x() == 0 && cursor->width() < 50 && markedNear(clipped, {40, 160}),
                 "a partially clipped brush allocates only its visible strip"))
        return false;
    if (!require(matchesReference(), "clipped footprint preserves the raster reference"))
        return false;
    paints.clear();
    root->setProperty("pointer", QPointF(-25, 160));
    frame();
    if (!require((hardware ? paints.isEmpty() : !paints.isEmpty()) && matchesReference(),
                 "moving a clipped footprint updates parameters without raster painting on GPU"))
        return false;
    for (int y = 98; y < 223; ++y)
        for (int x = 2; x < 43; ++x)
            if (!require(
                    near(pixel(clipped, {x + .5, y + .5}), pixel(unclipped, {x + 140.5, y + .5})),
                    "viewport clipping preserves the footprint's pixels"))
                return false;
    root->setProperty("diameter", 32000);
    root->setProperty("pointer", QPointF(-15500, 160));
    const auto huge = frame();
    if (!require(cursor->width() <= 512 && cursor->height() <= 320 &&
                     markedNear(huge, {500, 160}) && near(pixel(huge, {250, 160}), base),
                 "a 500-pixel brush at 64x stays bounded to the viewport"))
        return false;
    if (!require(matchesReference(), "huge brush stays bounded and preserves the visible arc"))
        return false;
    root->setProperty("diameter", 80);
    root->setProperty("pointer", QPointF(480, 160));
    root->setProperty("panX", 64);
    frame();
    if (!require(cursor->property("viewportBounds").toRectF().left() == 64,
                 "scrolling refreshes the local viewport coordinates"))
        return false;
    if (!require(matchesReference(), "scrolling preserves brush position and clipping"))
        return false;
    root->setProperty("panX", 0);
    root->setProperty("boardX", 40);
    frame();
    if (!require(cursor->property("viewportBounds").toRectF().left() == -40 && cursor->width() < 80,
                 "document repositioning refreshes clipping even without zoom or scrolling"))
        return false;
    if (!require(matchesReference(), "image margins retain the unclipped part of the cursor"))
        return false;
    root->setProperty("boardX", 0);
    root->setProperty("pointer", QPointF(240.25, 160.75));
    root->setProperty("angle", 73.5);
    root->setProperty("roundness", .01);
    frame();
    if (!require(matchesReference(), "minimum roundness preserves transformed stroke widths"))
        return false;
    root->setProperty("diameter", 1.);
    root->setProperty("roundness", 1.);
    frame();
    if (!require(matchesReference(), "a one-pixel brush retains its subpixel outline"))
        return false;
    root->setProperty("pointer", QPointF(240, 160));
    root->setProperty("showCursor", false);
    const auto hidden = frame();
    if (!require(near(pixel(hidden, {280, 160}), base),
                 "hiding the brush removes the footprint without changing the background"))
        return false;
    root->setProperty("diameter", 80.);
    root->setProperty("angle", 0.);
    root->setProperty("roundness", .5);
    root->setProperty("sign", 1);
    root->setProperty("showCursor", true);
    const auto restored = frame();
    return require(markedNear(restored, {280, 160}) && matchesReference(),
                   "restoring the cursor uses settings changed while it was hidden");
}
