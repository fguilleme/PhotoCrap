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
    // At the full display level, only the modified tile and its filtering halo
    // are copied/uploaded. The explicit viewport covers all three tile slots.
    _window->setMinimumWidth(1200);
    _window->resize(qMax(1200, _window->width()), _window->height());
    canvas.setX(40);
    canvas.setSize({2176, 256});
    canvas.setViewportRect(canvas.boundingRect());
    const auto stroke = [&](qreal x, qreal y, const QColor &color, int size = 8) {
        document.setTool("brush");
        document.setBrushSize(size);
        document.setBrushHardness(1.);
        document.setColor(color);
        document.beginStroke(x, y);
        document.endStroke();
    };
    for (int bits : {8, 16}) {
        if (!require(document.newDocument(2176, 256, false, bits),
                     "create a regional edit reference"))
            return false;
        frame();
        auto previous = canvas.imageTextureUploads();
        stroke(100, 100, Qt::red);
        const auto local = frame();
        if (!require(canvas.imageTextureUploads() == previous + 1 &&
                         near(pixel(local, 100.5, 100.5), Qt::red) &&
                         near(pixel(local, 1100.5, 100.5), Qt::white),
                     "a local 8/16-bit stroke replaces only its tile"))
            return false;
        previous = canvas.imageTextureUploads();
        stroke(1024, 100, Qt::green);
        const auto boundary = frame();
        if (!require(canvas.imageTextureUploads() == previous + 2 &&
                         near(pixel(boundary, 1023.5, 100.5), Qt::green) &&
                         near(pixel(boundary, 1024.5, 100.5), Qt::green),
                     "a boundary stroke refreshes both tile halos"))
            return false;
        canvas.setViewportRect({0, 0, 800, 256});
        frame();
        previous = canvas.imageTextureUploads();
        stroke(2100, 100, Qt::blue);
        frame();
        if (!require(canvas.imageTextureUploads() == previous,
                     "offscreen edits retain the resident tile"))
            return false;
        canvas.setX(40 - 2050);
        canvas.setViewportRect({2050, 0, 126, 256});
        if (!require(near(pixel(frame(), 2100.5, 100.5), Qt::blue) &&
                         canvas.imageTextureUploads() == previous + 1,
                     "panning loads the edited offscreen tile"))
            return false;
        canvas.setX(40);
        canvas.setViewportRect(canvas.boundingRect());
        frame();
        previous = canvas.imageTextureUploads();
        document.undo();
        const auto undone = frame();
        if (!require(canvas.imageTextureUploads() == previous + 3 &&
                         near(pixel(undone, 100.5, 100.5), Qt::red),
                     "undo fully refreshes the visible tiles"))
            return false;
        previous = canvas.imageTextureUploads();
        // Another consumer can compose several times between two canvas frames.
        stroke(100, 160, Qt::blue);
        (void)document.composite();
        stroke(1100, 160, Qt::red);
        (void)document.composite();
        if (!require(near(pixel(frame(), 100.5, 160.5), Qt::blue) &&
                         canvas.imageTextureUploads() == previous + 3,
                     "missing composite generations conservatively refresh all tiles"))
            return false;
        canvas.setSize({2176 * .2, 256 * .2});
        canvas.setViewportRect(canvas.boundingRect());
        frame();
        previous = canvas.imageTextureUploads();
        stroke(300, 100, Qt::blue, 32);
        if (!require(near(pixel(frame(), 300.5 * .2, 100.5 * .2), Qt::blue) &&
                         canvas.imageTextureUploads() > previous,
                     "reduced display levels retain the full refresh path"))
            return false;
        canvas.setSize({2176, 256});
        canvas.setViewportRect(canvas.boundingRect());
    }
    // Make the reduced image span three GPU tiles on both standard and HiDPI
    // windows. Measure resampling work independently from texture uploads.
    for (int bits : {8, 16})
        for (int factor : {2, 4})
            for (bool odd : {false, true}) {
                const QSize size(3072 * factor + (odd ? 3 : 0), 128 * factor + (odd ? 3 : 0));
                const qreal reducedScale = 1. / (factor * ratio);
                canvas.setX(40);
                canvas.setSize({size.width() * reducedScale, size.height() * reducedScale});
                canvas.setViewportRect(canvas.boundingRect());
                if (!require(document.newDocument(size.width(), size.height(), false, bits),
                             "create a reduced regional reference"))
                    return false;
                frame();
                auto       previous     = canvas.imageTextureUploads();
                const auto sourceBefore = canvas.displayLevelSourcePixels();
                stroke(100 * factor, 64 * factor, Qt::red, 16 * factor);
                const auto   local = frame();
                const auto   work  = canvas.displayLevelSourcePixels() - sourceBefore;
                const qint64 total = qint64(size.width()) * size.height();
                if (!require(canvas.imageTextureUploads() == previous + 1 &&
                                 near(pixel(local, (100 * factor + .5) * reducedScale,
                                            (64 * factor + .5) * reducedScale),
                                      Qt::red),
                             "reduced 8/16-bit edits replace only the affected display tile"))
                    return false;
                if (!require(odd ? work == quint64(total) : work < quint64(total / 100),
                             "aligned reductions resample only a patch; irregular grids preserve "
                             "full Qt sampling"))
                    return false;
                if (!require(document.composite().isDetached(),
                             "incremental reduction retains no native composite reference"))
                    return false;
                previous = canvas.imageTextureUploads();
                stroke(1024 * factor, 64 * factor, Qt::green, 16 * factor);
                const auto boundary = frame();
                if (!require(canvas.imageTextureUploads() == previous + 2 &&
                                 near(pixel(boundary, (1024 * factor + .5) * reducedScale,
                                            (64 * factor + .5) * reducedScale),
                                      Qt::green),
                             "reduced edits preserve both tile boundary halos"))
                    return false;
                canvas.setViewportRect({0, 0, 800 / ratio, canvas.height()});
                frame();
                previous = canvas.imageTextureUploads();
                stroke(2600 * factor, 64 * factor, Qt::blue, 16 * factor);
                frame();
                if (!require(canvas.imageTextureUploads() == previous,
                             "reduced offscreen edits preserve resident textures"))
                    return false;
                canvas.setX(40 - 2560 / ratio);
                canvas.setViewportRect({2560 / ratio, 0, 512 / ratio, canvas.height()});
                if (!require(near(pixel(frame(), (2600 * factor + .5) * reducedScale,
                                        (64 * factor + .5) * reducedScale),
                                  Qt::blue) &&
                                 canvas.imageTextureUploads() == previous + 1,
                             "panning reveals updated pixels in the cached reduced image"))
                    return false;
            }
    canvas.setX(40);
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
