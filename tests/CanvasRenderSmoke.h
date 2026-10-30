#pragma once
#include "CanvasItem.h"
#include "Document.h"
#include <QPainter>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QtTest>

inline bool runCanvasRenderSmoke(QQuickWindow *_window)
{
    auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Canvas render check failed: %s", _message);
        return _ok;
    };
    Document   document;
    CanvasItem canvas(_window->contentItem());
    canvas.setZ(1000);
    canvas.setPosition({40, 120});
    constexpr qreal initialScale = .4;
    canvas.setSize({2176 * initialScale, 640 * initialScale});
    canvas.setViewportRect({0, 0, 1150, 400});
    canvas.setDocument(&document);
    QTemporaryDir directory;
    const auto    frame = [&] {
        _window->update();
        QTest::qWait(40);
        return _window->grabWindow();
    };
    const auto pixel = [&](const QImage &_frame, qreal _x, qreal _y) {
        const QPointF scene = canvas.mapToScene({_x, _y});
        const qreal   ratio = qreal(_frame.width()) / _window->width();
        const QPoint  point(qFloor(scene.x() * ratio), qFloor(scene.y() * ratio));
        return _frame.rect().contains(point) ? _frame.pixelColor(point) : QColor{};
    };
    const auto near = [](const QColor &_a, const QColor &_b) {
        return qAbs(_a.red() - _b.red()) <= 2 && qAbs(_a.green() - _b.green()) <= 2 &&
               qAbs(_a.blue() - _b.blue()) <= 2;
    };
    for (const int bits : {8, 16}) {
        QImage source(2176, 640, bits == 16 ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
        for (int y = 0; y < source.height(); ++y)
            for (int x = 0; x < source.width(); ++x) {
                const int    alpha = x < 256 ? 0 : x < 512 ? 128 : 255;
                const QColor color = bits == 16 ? QColor::fromRgba64(ushort(x * 29), ushort(y * 79),
                                                                     26000, ushort(alpha * 257))
                                                : QColor(x / 12, y / 4, 64, alpha);
                source.setPixelColor(x, y, color);
            }
        const auto path = directory.filePath(QString::number(bits) + ".png");
        if (!require(source.save(path) && document.openFile(QUrl::fromLocalFile(path)),
                     "load a tiled 8/16-bit reference image"))
            return false;
        const QImage rendered = frame();
        if (!require(!rendered.isNull() && canvas.imageTextureUploads() >= 2,
                     "the canvas creates backend-compatible image tiles"))
            return false;
        if (!require(document.composite().isDetached(),
                     "the canvas does not retain a full composite forcing future edit copies"))
            return false;
        const QImage displayed = document.displayComposite().convertToFormat(QImage::Format_ARGB32);
        for (const QPoint point :
             {QPoint(100, 100), QPoint(280, 101), QPoint(700, 100), QPoint(1023, 100),
              QPoint(1024, 100), QPoint(2047, 100), QPoint(2048, 100), QPoint(2100, 100)}) {
            const qreal x       = (point.x() + .5) * initialScale;
            const qreal y       = (point.y() + .5) * initialScale;
            const int   checker = (qFloor(x) % 24 < 12) == (qFloor(y) % 24 < 12) ? 238 : 214;
            QImage      expected(1, 1, QImage::Format_ARGB32_Premultiplied);
            expected.fill(QColor(checker, checker, checker));
            QPainter painter(&expected);
            painter.fillRect(expected.rect(), displayed.pixelColor(point));
            painter.end();
            if (!near(pixel(rendered, x, y), expected.pixelColor(0, 0))) {
                qWarning() << "Pixel mismatch" << bits << point << pixel(rendered, x, y)
                           << expected.pixelColor(0, 0);
                rendered.save("/tmp/photocrap-canvas-render-failure.png");
            }
            if (!require(near(pixel(rendered, x, y), expected.pixelColor(0, 0)),
                         "photo colours, transparency and tile boundaries match the CPU reference"))
                return false;
        }
    }
    document.setTool("select");
    document.beginStroke(200, 150);
    document.continueStroke(800, 350);
    document.endStroke();
    document.invertSelection();
    const QImage before  = frame();
    const auto   uploads = canvas.imageTextureUploads();
    canvas.setSelectionPhase(3);
    const QImage after = frame();
    if (!require(before != after && canvas.imageTextureUploads() == uploads,
                 "marching ants animate without uploading the photo again"))
        return false;
    const qreal ratio = _window->effectiveDevicePixelRatio();
    if (!require(canvas.overlayPixelSize().width() <= qCeil(1150 * ratio) &&
                     canvas.overlayPixelSize().height() <= qCeil(400 * ratio),
                 "selection backing storage is bounded to the visible viewport"))
        return false;
    document.setSelectionEdgesVisible(false);
    const QImage hidden = frame();
    if (!require(hidden != after && canvas.imageTextureUploads() == uploads,
                 "hiding selection edges leaves the image texture intact"))
        return false;
    constexpr qreal zoomedScale = initialScale * 1.1;
    canvas.setSize({2176 * zoomedScale, 640 * zoomedScale});
    frame();
    if (!require(canvas.imageTextureUploads() == uploads,
                 "zooming within a display level reuses image textures"))
        return false;
    canvas.setViewportRect({50, 0, 1100, 400});
    frame();
    if (!require(canvas.imageTextureUploads() == uploads,
                 "panning within resident tiles reuses image textures"))
        return false;
    document.setDisplayChannel(1);
    const QImage channel = frame();
    const auto   gray    = pixel(channel, 750.5 * zoomedScale, 100.5 * zoomedScale);
    if (!require(qAbs(gray.red() - gray.green()) <= 1 && qAbs(gray.red() - gray.blue()) <= 1,
                 "channel views still display in grayscale"))
        return false;
    document.setDisplayChannel(0);
    document.addLayerMask(true);
    document.fillMaskWithColor(QColor(83, 83, 83));
    document.toggleMaskOnlyView(document.activeLayer());
    const QImage mask = frame();
    if (!require(near(pixel(mask, 500, 100), QColor(83, 83, 83)),
                 "isolated mask view uses the refreshed photo texture"))
        return false;
    canvas.setSize({2176, 640});
    canvas.setViewportRect({0, 0, 800, 300});
    frame();
    const auto beforeNewTile = canvas.imageTextureUploads();
    canvas.setX(40 - 1100);
    canvas.setViewportRect({1100, 0, 800, 300});
    const auto panned = frame();
    if (!require(canvas.imageTextureUploads() > beforeNewTile &&
                     near(pixel(panned, 1500.5, 100.5), QColor(83, 83, 83)),
                 "panning into an unloaded tile creates the correct texture"))
        return false;
    const auto beforeReturn = canvas.imageTextureUploads();
    canvas.setX(40);
    canvas.setViewportRect({0, 0, 800, 300});
    frame();
    if (!require(canvas.imageTextureUploads() > beforeReturn,
                 "offscreen textures are evicted and can be loaded again"))
        return false;
    document.setSelectionEdgesVisible(true);
    canvas.setSize({2176 * 4, 640 * 4});
    frame();
    if (!require(canvas.overlayPixelSize().width() > 0 &&
                     canvas.overlayPixelSize().width() <= qCeil(800 * ratio) &&
                     canvas.overlayPixelSize().height() <= qCeil(300 * ratio),
                 "high zoom does not allocate a document-sized selection surface"))
        return false;
    canvas.setDocument(nullptr);
    frame();
    canvas.setDocument(&document);
    if (!require(!frame().isNull(), "the canvas can release and recreate its scene graph content"))
        return false;
    qInfo() << "Canvas render checks passed; graphics API:"
            << _window->rendererInterface()->graphicsApi()
            << "; photo texture creations:" << canvas.imageTextureUploads();
    return true;
}
