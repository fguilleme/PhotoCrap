#pragma once
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>
#include <memory>

inline bool runMeasurementOverlaySmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Measurement overlay check failed: %s", _message);
        return _ok;
    };
    QQmlComponent component(qmlEngine(_window));
    component.setData(R"(
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
    property point measureEnd: Qt.point(20,18)
    property bool showRuler: true
    Rectangle { anchors.fill: parent; color: "#5a7896" }
    Flickable {
        id: view
        anchors.fill: parent
        contentWidth: board.width; contentHeight: board.height
        contentX: panX; contentY: panY
        clip: true; interactive: false
        Item {
            id: board
            objectName: "measurementTestBoard"
            width: 16384*16; height: 16384*16
            rotation: viewRotation; transformOrigin: Item.TopLeft
            RulerOverlay {
                objectName: "measurementTestRuler"
                viewportItem: view; zoom: 16
                startPoint: Qt.point(8,8); endPoint: measureEnd
                visible: showRuler
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
)",
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
    root->setProperty("viewRotation", 0);
    root->setProperty("measureEnd", QPointF(16380, 8));
    root->setProperty("panX", 600);
    const auto longLine = frame();
    if (!require(ruler->width() <= 512 && ruler->height() <= 12 &&
                     near(pixel(longLine, board, {824, 128}), QColor("#f5f7f9")),
                 "a document-wide ruler stays a viewport-clipped narrow strip"))
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
