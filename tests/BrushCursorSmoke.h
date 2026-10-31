#pragma once
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
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
    QQmlComponent component(qmlEngine(_window));
    component.setData(R"(
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
    Rectangle { anchors.fill: parent; color: "#5a7896" }
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
                visible: showCursor
            }
        }
    }
}
)",
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
    frame();
    QSignalSpy paints(cursor, SIGNAL(painted()));
    if (!require(paints.isValid(), "observe real brush cursor paints"))
        return false;
    root->setProperty("pointer", QPointF(240, 160));
    const auto moved = frame();
    if (!require(paints.isEmpty() && markedNear(moved, {280, 160}) &&
                     near(pixel(moved, {160, 160}), base),
                 "unclipped pointer motion reuses its raster and clears the former position"))
        return false;
    root->setProperty("angle", 30);
    root->setProperty("roundness", .25);
    const auto    ellipse = frame();
    const qreal   radians = qDegreesToRadians(30.);
    const QPointF major(240 + 40 * std::cos(radians), 160 + 40 * std::sin(radians));
    const QPointF minor(240 - 10 * std::sin(radians), 160 + 10 * std::cos(radians));
    if (!require(!paints.isEmpty() && markedNear(ellipse, major) && markedNear(ellipse, minor) &&
                     near(pixel(ellipse, {280, 160}), base),
                 "angle and roundness update the elliptical outline without filling its interior"))
        return false;
    paints.clear();
    root->setProperty("viewRotation", 12);
    const auto rotated = frame();
    if (!require(paints.isEmpty() && markedNear(rotated, major),
                 "canvas rotation moves the cached footprint through the scene graph"))
        return false;
    root->setProperty("viewRotation", 0);
    root->setProperty("angle", 0);
    root->setProperty("roundness", 1);
    root->setProperty("sign", 1);
    const auto plus = frame();
    if (!require(markedNear(plus, {260, 140}) && !near(pixel(plus, {260.5, 142.5}), base),
                 "quick selection add sign stays visible inside the cursor bounds"))
        return false;
    root->setProperty("sign", -1);
    const auto minus = frame();
    if (!require(markedNear(minus, {260, 140}) && near(pixel(minus, {260, 142.5}), base),
                 "quick selection subtract sign drops the vertical leg"))
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
    root->setProperty("diameter", 80);
    root->setProperty("pointer", QPointF(480, 160));
    root->setProperty("panX", 64);
    frame();
    if (!require(cursor->property("viewportBounds").toRectF().left() == 64,
                 "scrolling refreshes the local viewport coordinates"))
        return false;
    root->setProperty("panX", 0);
    root->setProperty("boardX", 40);
    frame();
    if (!require(cursor->property("viewportBounds").toRectF().left() == -40 && cursor->width() < 80,
                 "document repositioning refreshes clipping even without zoom or scrolling"))
        return false;
    root->setProperty("boardX", 0);
    root->setProperty("pointer", QPointF(240, 160));
    root->setProperty("showCursor", false);
    const auto hidden = frame();
    return require(near(pixel(hidden, {280, 160}), base),
                   "hiding the brush removes the footprint without changing the background");
}
