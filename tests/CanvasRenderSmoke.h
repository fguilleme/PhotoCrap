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
    canvas.setTextureCacheMiB(0);
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
    const QImage unselected = frame();
    document.setTool("select");
    document.beginStroke(200, 150);
    document.continueStroke(800, 350);
    document.endStroke();
    document.invertSelection();
    const QImage before          = frame();
    auto         uploads         = canvas.imageTextureUploads();
    const auto   geometryUploads = canvas.selectionGeometryUploads();
    const auto   overlayUploads  = canvas.overlayTextureUploads();
    const bool   shaderSelection =
        QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi());
    canvas.setSelectionPhase(3);
    const QImage after = frame();
    if (!require(before != after && canvas.imageTextureUploads() == uploads,
                 "marching ants animate without uploading the photo again"))
        return false;
    if (!require(near(pixel(after, 400 * initialScale, 250 * initialScale),
                      pixel(unselected, 400 * initialScale, 250 * initialScale)),
                 "selection outlines do not fill their interior"))
        return false;
    if (shaderSelection) {
        if (!require(canvas.selectionShaderActive() &&
                         canvas.selectionGeometryUploads() == geometryUploads &&
                         canvas.overlayTextureUploads() == overlayUploads &&
                         canvas.overlayPixelSize().isEmpty(),
                     "shader selection animation changes no image texture or geometry"))
            return false;
        canvas.setSelectionPhase(3.25);
        if (!require(frame() != after && canvas.selectionGeometryUploads() == geometryUploads &&
                         canvas.overlayTextureUploads() == overlayUploads,
                     "fractional dash phases animate smoothly without rebuilding geometry"))
            return false;
        canvas.setSelectionPhase(3);
        frame();
        canvas.setViewportRect({50, 0, 750, 256});
        const auto pannedSelection = frame();
        for (int y = 0; y < 150; ++y)
            for (int x = 225; x < 350; ++x)
                if (!require(
                        near(pixel(pannedSelection, x + .5, y + .5), pixel(after, x + .5, y + .5)),
                        "clipped contours preserve their phase while panning"))
                    return false;
        canvas.setViewportRect({0, 0, 1150, 400});
        frame();
        const auto transformedGeometry = canvas.selectionGeometryUploads();
        canvas.setRotation(12);
        const auto rotatedSelection = frame();
        canvas.setSelectionPhase(4);
        if (!require(frame() != rotatedSelection &&
                         canvas.selectionGeometryUploads() == transformedGeometry,
                     "rotated contours animate using the inherited scene transform"))
            return false;
        canvas.setRotation(0);
        canvas.setSelectionPhase(3);
        frame();
        uploads = canvas.imageTextureUploads();
    }
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
    if (!require(shaderSelection ? canvas.overlayPixelSize().isEmpty() &&
                                       canvas.selectionVertexCount() <= 1000000
                                 : canvas.overlayPixelSize().width() > 0 &&
                                       canvas.overlayPixelSize().width() <= qCeil(800 * ratio) &&
                                       canvas.overlayPixelSize().height() <= qCeil(300 * ratio),
                 "high zoom keeps selection storage bounded to the visible contour"))
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
    canvas.setSize({2176, 256});
    canvas.setViewportRect(canvas.boundingRect());
    canvas.setTextureCacheMiB(4);
    for (int bits : {8, 16}) {
        QImage stripes(2176, 256,
                       bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                  : QImage::Format_ARGB32_Premultiplied);
        stripes.fill(Qt::red);
        QPainter paint(&stripes);
        paint.fillRect(QRect(1024, 0, 1024, 256), Qt::green);
        paint.fillRect(QRect(2048, 0, 128, 256), Qt::blue);
        paint.end();
        const auto path = directory.filePath(QString("cache-%1.png").arg(bits));
        if (!require(stripes.save(path) && document.openFile(QUrl::fromLocalFile(path)),
                     "create a cached navigation reference"))
            return false;
        canvas.setX(40);
        canvas.setViewportRect(canvas.boundingRect());
        frame();
        auto previous = canvas.imageTextureUploads();
        canvas.setViewportRect({0, 0, 800, 256});
        frame();
        canvas.setX(40 - 1100);
        canvas.setViewportRect({1100, 0, 800, 256});
        if (!require(near(pixel(frame(), 1500.5, 100.5), Qt::green) &&
                         canvas.imageTextureUploads() == previous &&
                         canvas.residentTextureBytes() <= 4 * 1048576,
                     "cached 8/16-bit navigation reuses textures within its budget"))
            return false;
        canvas.setTextureCacheMiB(0);
        frame();
        canvas.setX(40);
        canvas.setViewportRect({0, 0, 800, 256});
        if (!require(near(pixel(frame(), 100.5, 100.5), Qt::red) &&
                         canvas.imageTextureUploads() == previous + 1,
                     "disabling the reserve evicts offscreen textures"))
            return false;
        canvas.setTextureCacheMiB(4);
        canvas.setX(40 - 1100);
        canvas.setViewportRect({1100, 0, 800, 256});
        frame();
        previous = canvas.imageTextureUploads();
        stroke(100, 100, Qt::black, 32);
        frame();
        if (!require(canvas.imageTextureUploads() == previous,
                     "editing a cached offscreen tile leaves visible textures intact"))
            return false;
        canvas.setX(40);
        canvas.setViewportRect({0, 0, 800, 256});
        if (!require(near(pixel(frame(), 100.5, 100.5), Qt::black) &&
                         canvas.imageTextureUploads() == previous + 1,
                     "returning to an edited cached tile uploads fresh pixels"))
            return false;
        canvas.setTextureCacheMiB(1);
        if (!require(near(pixel(frame(), 100.5, 100.5), Qt::black) &&
                         canvas.residentTextureBytes() > 1048576 &&
                         canvas.residentTextureBytes() < 2 * 1048576,
                     "visible content stays complete when its footprint exceeds the reserve"))
            return false;
        if (!require(document.composite().isDetached(),
                     "navigation snapshots do not retain the full composite"))
            return false;
        canvas.setTextureCacheMiB(4);
    }
    canvas.setTextureCacheMiB(0);
    // Remove marks are non-destructive and can coexist with animated selection.
    canvas.setSize({512, 320});
    canvas.setX(40);
    canvas.setViewportRect(canvas.boundingRect());
    for (int bits : {8, 16}) {
        if (!require(document.newDocument(512, 320, false, bits),
                     "create a removal overlay reference"))
            return false;
        const QColor base(40, 110, 210);
        document.setColor(base);
        document.fillWithColor(true);
        document.setSelectionEdgesVisible(true);
        document.setTool("select");
        document.beginStroke(400, 200);
        document.continueStroke(460, 260);
        document.endStroke();
        document.invertSelection();
        document.setTool("remove");
        document.setRemoveAfterEachStroke(false);
        document.setRemoveSubtract(false);
        document.setBrushSize(64);
        frame();
        const auto beforeMarks = canvas.imageTextureUploads();
        const int  history     = document.historyIndex();
        document.beginStroke(100, 100);
        document.endStroke();
        if (bits == 16) {
            if (!require(
                    !document.removeHasPending() && document.channelBits() == 16 &&
                        document.historyIndex() == history &&
                        near(pixel(frame(), 100.5, 100.5), base),
                    "unsupported 16-bit removal remains rejected without an overlay or conversion"))
                return false;
            document.setRemoveAfterEachStroke(true);
            continue;
        }
        QImage expected(1, 1, QImage::Format_ARGB32_Premultiplied);
        expected.fill(base);
        QPainter blend(&expected);
        blend.fillRect(expected.rect(), QColor(235, 70, 180, 110));
        blend.end();
        const auto marked = frame();
        if (!require(near(pixel(marked, 100.5, 100.5), expected.pixelColor(0, 0)) &&
                         near(pixel(marked, 200.5, 100.5), base) &&
                         canvas.imageTextureUploads() == beforeMarks &&
                         document.historyIndex() == history,
                     "removal marking preserves native pixels and matches alpha composition"))
            return false;
        document.setRemoveSubtract(true);
        document.setBrushSize(20);
        document.beginStroke(100, 100);
        document.endStroke();
        const auto holed = frame();
        if (!require(near(pixel(holed, 100.5, 100.5), base) &&
                         near(pixel(holed, 125.5, 100.5), expected.pixelColor(0, 0)),
                     "subtracted removal marks preserve holes without double blending"))
            return false;
        if (shaderSelection) {
            if (!require(canvas.removeOverlayGpuActive() && canvas.overlayPixelSize().isEmpty(),
                         "hardware removal marking needs no image texture"))
                return false;
            const auto geometry = canvas.regionGeometryUploads(),
                       textures = canvas.overlayTextureUploads();
            canvas.setSelectionPhase(canvas.selectionPhase() + .25);
            frame();
            if (!require(canvas.regionGeometryUploads() == geometry &&
                             canvas.overlayTextureUploads() == textures,
                         "selection animation does not rebuild removal geometry"))
                return false;
            canvas.setViewportRect({50, 50, 300, 200});
            frame();
            canvas.setRotation(12);
            const auto rotated = frame();
            if (!require(near(pixel(rotated, 125.5, 100.5), expected.pixelColor(0, 0)),
                         "shared rotated clipping preserves the removal overlay"))
                return false;
            canvas.setRotation(0);
            canvas.setViewportRect(canvas.boundingRect());
        }
        document.cancelRemoval();
        if (!require(near(pixel(frame(), 125.5, 100.5), base) && !canvas.removeOverlayGpuActive() &&
                         document.historyIndex() == history,
                     "cancelling removal clears its overlay without editing the image"))
            return false;
        document.setRemoveSubtract(false);
        document.setRemoveAfterEachStroke(true);
    }
    QImage removal(512, 320, QImage::Format_ARGB32_Premultiplied);
    removal.fill(QColor(170, 170, 170));
    {
        QPainter painter(&removal);
        painter.fillRect(118, 94, 12, 12, Qt::red);
    }
    const auto removalPath = directory.filePath("remove-completion.png");
    if (!require(removal.save(removalPath) && document.openFile(QUrl::fromLocalFile(removalPath)),
                 "open the removal completion reference"))
        return false;
    document.setTool("remove");
    document.setRemoveAfterEachStroke(false);
    document.setRemoveSubtract(false);
    document.setBrushSize(48);
    document.beginStroke(124, 100);
    document.endStroke();
    document.setBrushSize(8);
    document.beginStroke(140, 100, true);
    document.endStroke();
    frame();
    const int  removalHistory = document.historyIndex();
    const auto markedRegion   = document.removeRegion();
    document.applyRemoval();
    if (!require(document.removeBusy() && document.removeRegion() == markedRegion &&
                     document.historyIndex() == removalHistory &&
                     document.layerImage(0).pixelColor(124, 100) == QColor(Qt::red),
                 "removal leaves pixels and history intact until its worker finishes assembly"))
        return false;
    QElapsedTimer removalWait;
    removalWait.start();
    while (document.removeBusy() && removalWait.elapsed() < 5000)
        QTest::qWait(10);
    const auto removed = frame();
    if (!require(!document.removeBusy() && !document.removeHasPending() &&
                     !canvas.removeOverlayGpuActive() &&
                     document.historyIndex() == removalHistory + 1 &&
                     near(pixel(removed, 124.5, 100.5), QColor(170, 170, 170)) &&
                     near(pixel(removed, 140.5, 100.5), QColor(170, 170, 170)),
                 "the asynchronous removal replaces the marks and preserves their hole"))
        return false;
    document.undo();
    if (!require(near(pixel(frame(), 124.5, 100.5), QColor(Qt::red)),
                 "undo restores the canvas after asynchronous removal assembly"))
        return false;
    for (int bits : {8, 16}) {
        QImage magic(512, 320,
                     bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                : QImage::Format_ARGB32_Premultiplied);
        magic.fill(bits == 16 ? QColor::fromRgba64(47003, 16001, 8007, 40003)
                              : QColor(180, 60, 30, 170));
        {
            QPainter painter(&magic);
            painter.fillRect(142, 100, 12, 12, QColor(30, 70, 180));
        }
        const auto magicPath = directory.filePath(QString("magic-%1.png").arg(bits));
        if (!require(magic.save(magicPath) && document.openFile(QUrl::fromLocalFile(magicPath)),
                     "open a native precision magic eraser reference"))
            return false;
        document.setTool("ellipticalSelect");
        document.beginStroke(96, 80);
        document.continueStroke(176, 160);
        document.endStroke();
        document.modifySelection("feather", 5);
        QElapsedTimer selectionWait;
        selectionWait.start();
        while (document.selectionBusy() && selectionWait.elapsed() < 5000)
            QTest::qWait(10);
        if (!require(!document.selectionBusy(), "prepare soft magic eraser coverage"))
            return false;
        document.setTool("magicEraser");
        document.setMagicEraserTolerance(0);
        document.setMagicEraserContiguous(false);
        document.setMagicEraserAntialias(true);
        document.setBrushOpacity(.37);
        const auto beforeMagic = document.layerImage(0);
        const int  history     = document.historyIndex();
        frame();
        document.beginStroke(112, 112);
        if (!require(document.pixelEditBusy() && document.historyIndex() == history &&
                         document.layerImage(0) == beforeMagic,
                     "magic erasing leaves native pixels untouched until its worker completes"))
            return false;
        QElapsedTimer eraseWait;
        eraseWait.start();
        while (document.pixelEditBusy() && eraseWait.elapsed() < 5000)
            QTest::qWait(10);
        const auto erased = frame();
        if (!require(!document.pixelEditBusy() && document.channelBits() == bits &&
                         document.historyIndex() == history + 1,
                     "soft magic erasing commits once at native precision"))
            return false;
        for (const auto point : {QPoint(100, 112), QPoint(112, 112), QPoint(148, 108)}) {
            const int checker = (point.x() % 24 < 12) == (point.y() % 24 < 12) ? 238 : 214;
            QImage    expected(1, 1, QImage::Format_ARGB32_Premultiplied);
            expected.fill(QColor(checker, checker, checker));
            QPainter painter(&expected);
            painter.fillRect(expected.rect(), document.layerImage(0).pixelColor(point));
            painter.end();
            if (!require(
                    near(pixel(erased, point.x() + .5, point.y() + .5), expected.pixelColor(0, 0)),
                    "the canvas displays native soft erasing and checkerboard transparency"))
                return false;
        }
        document.undo();
        frame();
        if (!require(document.layerImage(0) == beforeMagic,
                     "undo restores native pixels after soft magic erasing"))
            return false;
    }
    QImage donor(512, 320, QImage::Format_ARGB32_Premultiplied);
    donor.fill(QColor(180, 40, 40));
    for (int y = 0; y < donor.height(); ++y)
        for (int x = 256; x < donor.width(); ++x)
            donor.setPixelColor(x, y, QColor(30 + x % 100, 40 + y % 180, 100 + (x + y) % 90, 128));
    const auto donorPath = directory.filePath("patch-preview.png");
    if (!require(donor.save(donorPath), "create a transparent patterned donor"))
        return false;
    for (int mode : {0, 1}) {
        if (!require(document.openFile(QUrl::fromLocalFile(donorPath)),
                     "open patch preview reference"))
            return false;
        canvas.setSize({512, 320});
        canvas.setViewportRect(canvas.boundingRect());
        document.setSelectionEdgesVisible(true);
        document.setTool("select");
        document.setSelectionMode(0);
        document.beginStroke(96, 96);
        document.continueStroke(160, 160);
        document.endStroke();
        document.setSelectionMode(2);
        document.beginStroke(120, 120);
        document.continueStroke(136, 136);
        document.endStroke();
        document.setSelectionMode(0);
        document.setPatchMode(mode);
        document.setTool("patch");
        const auto before  = frame();
        const auto photo   = canvas.imageTextureUploads();
        const int  history = document.historyIndex();
        document.beginStroke(128, 100);
        document.continueStroke(368, 100);
        frame();
        const auto textures = canvas.patchTextureUploads(),
                   geometry = canvas.patchGeometryUploads();
        document.continueStroke(384, 100);
        const auto preview = frame();
        const auto image   = document.patchPreviewImage();
        const auto target  = document.patchTargetRect();
        if (!require(document.patchDragging() && !image.isNull() &&
                         document.historyIndex() == history &&
                         canvas.imageTextureUploads() == photo,
                     "donor preview does not alter native pixels or history"))
            return false;
        const QPoint offset(mode == 1 ? 256 : 0, 0);
        for (const auto &p : {QPoint(106, 106), QPoint(150, 110), QPoint(110, 150)}) {
            const auto at = p + offset;
            const int  u =
                qBound(0, qFloor((at.x() + .5 - target.x()) * image.width() / target.width()),
                       image.width() - 1);
            const int v =
                qBound(0, qFloor((at.y() + .5 - target.y()) * image.height() / target.height()),
                       image.height() - 1);
            QImage expected(1, 1, QImage::Format_ARGB32_Premultiplied);
            expected.fill(pixel(before, at.x() + .5, at.y() + .5));
            QPainter paint(&expected);
            paint.drawImage(QRect(0, 0, 1, 1), image, QRect(u, v, 1, 1));
            paint.end();
            if (!require(near(pixel(preview, at.x() + .5, at.y() + .5), expected.pixelColor(0, 0)),
                         "source/destination patch texture and alpha match the raster reference"))
                return false;
        }
        const auto hole = QPoint(128, 128) + offset;
        if (!require(near(pixel(preview, hole.x() + .5, hole.y() + .5),
                          pixel(before, hole.x() + .5, hole.y() + .5)),
                     "patch geometry leaves selection holes untouched"))
            return false;
        if (shaderSelection) {
            if (!require(canvas.patchOverlayGpuActive() && canvas.overlayPixelSize().isEmpty() &&
                             (mode == 0 ? canvas.patchGeometryUploads() == geometry &&
                                              canvas.patchTextureUploads() == textures + 1
                                        : canvas.patchGeometryUploads() == geometry + 1 &&
                                              canvas.patchTextureUploads() == textures),
                         "source drags reuse geometry; destination drags reuse identical donor "
                         "textures"))
                return false;
            const auto patchGeometry = canvas.patchGeometryUploads(),
                       patchTextures = canvas.patchTextureUploads();
            canvas.setSelectionPhase(canvas.selectionPhase() + .25);
            frame();
            if (!require(canvas.patchGeometryUploads() == patchGeometry &&
                             canvas.patchTextureUploads() == patchTextures,
                         "selection animation leaves donor resources unchanged"))
                return false;
            canvas.setViewportRect({50, 50, 400, 200});
            frame();
            const auto at              = QPoint(106, 106) + offset;
            const auto expectedRotated = pixel(preview, at.x() + .5, at.y() + .5);
            canvas.setRotation(12);
            const auto rotated = frame();
            if (!require(near(pixel(rotated, at.x() + .5, at.y() + .5), expectedRotated),
                         "patch UV coordinates survive viewport clipping and rotation"))
                return false;
            canvas.setRotation(0);
            canvas.setViewportRect(canvas.boundingRect());
        }
        document.cancelStroke();
        const auto cancelled = frame();
        const auto at        = QPoint(106, 106) + offset;
        if (!require(!document.patchDragging() && !canvas.patchOverlayGpuActive() &&
                         document.historyIndex() == history &&
                         near(pixel(cancelled, at.x() + .5, at.y() + .5),
                              pixel(before, at.x() + .5, at.y() + .5)),
                     "cancelling a patch clears its texture and restores the selection"))
            return false;
        document.beginStroke(128, 100);
        document.continueStroke(384, 100);
        frame();
        document.endStroke();
        if (!require(document.patchBusy() && document.patchDragging() &&
                         !document.patchPreviewImage().isNull() &&
                         document.historyIndex() == history,
                     "release retains the donor while the repair worker owns immutable inputs"))
            return false;
        QElapsedTimer repairWait;
        repairWait.start();
        while ((document.patchBusy() || document.patchDragging()) && repairWait.elapsed() < 5000)
            QTest::qWait(10);
        const auto finished = frame();
        if (!require(!document.patchBusy() && !document.patchDragging() &&
                         document.patchPreviewImage().isNull() && !canvas.patchOverlayGpuActive() &&
                         document.historyIndex() == history + 1 &&
                         near(pixel(finished, at.x() + .5, at.y() + .5),
                              document.layerImage(0).pixelColor(at)) &&
                         near(pixel(finished, hole.x() + .5, hole.y() + .5),
                              pixel(before, hole.x() + .5, hole.y() + .5)),
                     "the repaired composite replaces the donor, preserves holes and commits once"))
            return false;
        document.undo();
        if (!require(near(pixel(frame(), at.x() + .5, at.y() + .5),
                          pixel(before, at.x() + .5, at.y() + .5)),
                     "undo restores the canvas after an asynchronous repair"))
            return false;
    }
    QImage moving(512, 320, QImage::Format_ARGB32_Premultiplied);
    moving.fill(QColor(165, 165, 165));
    {
        QPainter painter(&moving);
        painter.fillRect(102, 102, 12, 12, Qt::red);
    }
    const auto movingPath = directory.filePath("move-completion.png");
    if (!require(moving.save(movingPath), "create a content move display reference"))
        return false;
    for (const int mode : {0, 1}) {
        if (!require(document.openFile(QUrl::fromLocalFile(movingPath)), "open the move reference"))
            return false;
        document.setTool("select");
        document.setSelectionMode(0);
        document.beginStroke(96, 96);
        document.continueStroke(160, 160);
        document.endStroke();
        document.setSelectionMode(2);
        document.beginStroke(120, 120);
        document.continueStroke(136, 136);
        document.endStroke();
        document.setSelectionMode(0);
        document.setTool("contentAwareMove");
        document.setContentMoveMode(mode);
        document.setContentMoveStructure(7);
        document.setContentMoveColor(0);
        const int history = document.historyIndex();
        document.beginStroke(108, 108);
        document.continueStroke(364, 108);
        frame();
        document.endStroke();
        if (!require(document.contentMoveBusy() && document.patchDragging() &&
                         !document.patchPreviewImage().isNull() &&
                         document.historyIndex() == history,
                     "the move donor stays visible while final assembly runs on the worker"))
            return false;
        QElapsedTimer wait;
        wait.start();
        while ((document.contentMoveBusy() || document.patchDragging()) && wait.elapsed() < 5000)
            QTest::qWait(10);
        const auto finished = frame();
        if (!require(!document.contentMoveBusy() && !document.patchDragging() &&
                         !canvas.patchOverlayGpuActive() &&
                         document.historyIndex() == history + 1 &&
                         near(pixel(finished, 364.5, 108.5), QColor(Qt::red)) &&
                         near(pixel(finished, 108.5, 108.5),
                              mode == 0 ? QColor(165, 165, 165) : QColor(Qt::red)) &&
                         near(pixel(finished, 384.5, 128.5), QColor(165, 165, 165)),
                     "Move/Extend composites replace their donor and keep selection holes"))
            return false;
        document.undo();
        if (!require(near(pixel(frame(), 364.5, 108.5), QColor(165, 165, 165)),
                     "undo restores the canvas after asynchronous move assembly"))
            return false;
    }
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
