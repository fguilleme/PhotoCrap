#pragma once
#include "CanvasItem.h"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtConcurrent>
#include <QtTest>
inline bool runSpatialRenderSmoke(QQuickWindow *window)
{
    const auto require = [](bool ok, const char *message) {
        if (!ok)
            qWarning("Spatial render check failed: %s", message);
        return ok;
    };
    Document   document;
    CanvasItem canvas(window->contentItem());
    canvas.setPosition({qreal(qMax(0, qMin(40, window->width() - 1056))), 120});
    canvas.setSize({1056, 96});
    canvas.setZ(1000);
    canvas.setDocument(&document);
    canvas.setGpuStackEnabled(true);
    canvas.setTextureCacheMiB(128);
    QTemporaryDir directory;
    const bool    hardware =
        !qEnvironmentVariableIsSet("PHOTOCRAP_TEST_GL_LEGACY") &&
        QSGRendererInterface::isApiRhiBased(window->rendererInterface()->graphicsApi());
    const auto frame = [&] {
        window->update();
        QTest::qWait(60);
        return window->grabWindow();
    };
    const bool profiling      = qEnvironmentVariableIsSet("PHOTOCRAP_OPTIMIZATION_PROBES");
    const auto profilePreview = [&](const QString &_type, int _bits) {
        const auto    baseline   = frame();
        const auto    displayKey = document.displayComposite().cacheKey();
        const auto    passes     = canvas.stackPasses();
        QSignalSpy    swapped(window, &QQuickWindow::frameSwapped);
        QElapsedTimer time;
        time.start();
        document.previewLayerParameter(_type == "motionBlur" ? "distance" : "radius", 19);
        do {
            swapped.clear();
            window->update();
            const int remaining = qMax(1, 5000 - int(time.elapsed()));
            if (!require((!swapped.isEmpty() || swapped.wait(remaining)) && time.elapsed() < 5000,
                         "profile observes a changed filter frame"))
                return false;
        } while (hardware ? canvas.stackPasses() == passes
                          : document.displayComposite().cacheKey() == displayKey);
        const double latency = time.nsecsElapsed() / 1e6;
        const auto   draft   = window->grabWindow();
        const auto   screen  = document.displayComposite();
        const auto   memory  = canvas.graphicsMemoryUsage();
        document.finishLayerPreview();
        const auto reference = document.composite();
        const auto ratio     = double(draft.width()) / window->width();
        const auto viewport  = canvas.mapRectToScene(canvas.boundingRect());
        const auto crop      = QRectF(viewport.topLeft() * ratio, viewport.size() * ratio)
                                   .toAlignedRect()
                                   .intersected(draft.rect());
        if (!require(!crop.isEmpty() && !reference.isNull() && draft.size() == baseline.size() &&
                         draft.copy(crop) != baseline.copy(crop),
                     "the timed filter frame changed visible pixels"))
            return false;
        int    maxError = 0, samples = 0;
        qint64 sum = 0;
        for (int y = crop.top(); y <= crop.bottom(); y += 4)
            for (int x = crop.left(); x <= crop.right(); x += 4) {
                const auto a = draft.pixel(x, y);
                const int  sx =
                    qBound(0, qFloor((x + .5) / ratio - viewport.left()), reference.width() - 1);
                const int sy =
                    qBound(0, qFloor((y + .5) / ratio - viewport.top()), reference.height() - 1);
                const auto b  = reference.pixel(sx, sy);
                const int  r  = qAbs(qRed(a) - qRed(b));
                const int  g  = qAbs(qGreen(a) - qGreen(b));
                const int  bl = qAbs(qBlue(a) - qBlue(b));
                maxError      = qMax(maxError, qMax(r, qMax(g, bl)));
                sum += r + g + bl;
                samples += 3;
            }
        const QJsonObject probe{{"kind", "spatial-preview"},
                                {"filter", _type},
                                {"bits", _bits},
                                {"graphics_api", int(window->rendererInterface()->graphicsApi())},
                                {"present_ms", latency},
                                {"max_rgb_error", maxError},
                                {"mean_rgb_error", double(sum) / samples},
                                {"channel_samples", samples},
                                {"cpu_preview_bytes", double(screen.sizeInBytes())},
                                {"graphics_memory", QJsonObject::fromVariantMap(memory)}};
        qInfo().noquote() << "PHOTOCRAP_PROBE"
                          << QJsonDocument(probe).toJson(QJsonDocument::Compact);
        return true;
    };
    const auto check = [&](const QString &type, int bits, int radius) {
        // Compute the native reference after committing the gesture, so it is
        // independent of the CPU's intentionally reduced drag-preview path.
        const QImage     reference = document.composite();
        const auto       origin    = canvas.position();
        const auto       restore   = qScopeGuard([&] { canvas.setPosition(origin); });
        QVector<QPointF> views{origin};
        const QPointF    right(qMin(origin.x(), window->width() - canvas.width()), origin.y());
        if (right != origin)
            views.append(right);
        QSet<QPoint> required, inspected;
        int          maximum = 0;
        for (const auto &view : views) {
            canvas.setPosition(view);
            frame();
            const QImage result = frame();
            if (!require(!result.isNull() && canvas.gpuStackActive() == hardware,
                         "the complete spatial stack renders or falls back in software"))
                return false;
            const double ratio = double(result.width()) / window->width();
            for (int y :
                 {0, 1, 35, qMin(95, reference.height() - 1), qMin(511, reference.height() - 1),
                  qMin(512, reference.height() - 1), reference.height() - 1})
                for (int x : {0, 1, 15, 255, 510, 511, 512, 513, qMin(1023, reference.width() - 1),
                              qMin(1024, reference.width() - 1), reference.width() - 1}) {
                    const QPointF scene = canvas.mapToScene({x + .5, y + .5});
                    const QPoint  point(qFloor(scene.x() * ratio), qFloor(scene.y() * ratio));
                    required.insert(QPoint(x, y));
                    if (!result.rect().contains(point))
                        continue;
                    inspected.insert(QPoint(x, y));
                    const auto actual   = result.pixelColor(point);
                    auto       expected = reference.pixelColor(x, y);
                    if (expected.alpha() < 255) {
                        QImage    checker(1, 1, QImage::Format_ARGB32_Premultiplied);
                        const int level = (x % 24 < 12) == (y % 24 < 12) ? 238 : 214;
                        checker.fill(QColor(level, level, level));
                        QPainter painter(&checker);
                        painter.fillRect(checker.rect(), expected);
                        painter.end();
                        expected = checker.pixelColor(0, 0);
                    }
                    const int delta = qMax(qAbs(actual.red() - expected.red()),
                                           qMax(qAbs(actual.green() - expected.green()),
                                                qAbs(actual.blue() - expected.blue())));
                    maximum         = qMax(maximum, delta);
                    if (delta > 2) {
                        qWarning() << "Spatial difference" << type << bits << radius << QPoint(x, y)
                                   << actual << expected << delta;
                        canvas.setGpuStackEnabled(false);
                        frame();
                        const auto fallback = frame();
                        qWarning() << "CPU fallback sample" << fallback.pixelColor(point)
                                   << "native alpha sample" << reference.pixelColor(x, y);
                        return false;
                    }
                }
        }
        if (!require(required == inspected,
                     "all native reference samples are covered across the viewport pans"))
            return false;
        qInfo() << "Spatial reference" << type << bits << radius << "maximum display difference"
                << maximum;
        return true;
    };
    if (hardware) {
        canvas.setSize({600, 96});
        if (!require(document.newDocument(600, 96, false, 16), "create a two-tile image"))
            return false;
        frame();
        frame();
        const auto pipelines = canvas.stackPipelineCreations();
        const auto passes    = canvas.stackPasses();
        document.addAdjustmentLayer("motionBlur");
        frame();
        frame();
        if (!require(canvas.gpuStackActive() && canvas.stackPasses() >= passes + 6 &&
                         canvas.stackPipelineCreations() == pipelines + 1,
                     "two distinct tile sizes share a single spatial pipeline"))
            return false;
    }
    for (int bits : {8, 16}) {
        if (hardware) {
            canvas.setSize({96, 96});
            if (!require(document.newDocument(96, 96, false, bits), "create a one-tile image"))
                return false;
            frame();
            frame();
            const auto baseBytes = canvas.stackTextureBytes();
            document.addAdjustmentLayer("motionBlur");
            frame();
            frame();
            const auto motionBytes = canvas.stackTextureBytes();
            if (!require(canvas.gpuStackActive() &&
                             motionBytes == baseBytes + 96 * 96 * 16 + 96 + 256,
                         "motion allocates one texture and uniforms, sharing the existing quad"))
                return false;
            const auto pipelines = canvas.stackPipelineCreations();
            document.addAdjustmentLayer("boxBlur");
            frame();
            frame();
            if (!require(canvas.stackTextureBytes() ==
                             motionBytes + 96 * 96 * (bits == 16 ? 16 : 4) + 448,
                         "a separable filter adds the second spatial texture"))
                return false;
            if (!require(canvas.stackPipelineCreations() <= pipelines + (bits == 16 ? 0 : 1),
                         "the second target reuses its compatible pipeline"))
                return false;
            document.removeLayer();
            frame();
            frame();
            if (!require(canvas.stackTextureBytes() == motionBytes,
                         "removing the separable filter releases the second target"))
                return false;
            document.removeLayer();
            frame();
            frame();
            if (!require(canvas.gpuStackActive() && canvas.stackTextureBytes() == baseBytes,
                         "removing the last spatial filter releases its remaining target"))
                return false;
            document.undo();
            frame();
            frame();
            if (!require(canvas.gpuStackActive() && canvas.stackTextureBytes() == motionBytes,
                         "undo recreates the spatial target and its pass"))
                return false;
            canvas.setSize({1056, 96});
        }
        QImage source(1056, 96, bits == 16 ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
        for (int y = 0; y < source.height(); ++y)
            for (int x = 0; x < source.width(); ++x) {
                const quint32 h = quint32(x * 73856093u) ^ quint32(y * 19349663u);
                source.setPixelColor(
                    x, y,
                    bits == 16
                        ? QColor::fromRgba64(ushort(h), ushort(h >> 5), ushort(h >> 11), 65535)
                        : QColor(int(h & 255), int((h >> 8) & 255), int((h >> 16) & 255)));
            }
        const auto path = directory.filePath(QString::number(bits) + ".png");
        if (!require(source.save(path) && document.openFile(QUrl::fromLocalFile(path)),
                     "load a detailed native reference"))
            return false;
        for (const QString &type :
             {QString("boxBlur"), QString("gaussianBlur"), QString("motionBlur"),
              QString("unsharpMask"), QString("highPass")}) {
            document.addAdjustmentLayer(type);
            for (int radius : {0, 1, 8, 31, 100}) {
                document.previewLayerParameter(type == "motionBlur" ? "distance" : "radius",
                                               radius);
                if (type == "motionBlur")
                    document.previewLayerParameter("angle", 37.);
                if (type == "unsharpMask")
                    document.previewLayerParameter("amount", 85.);
                document.finishLayerPreview();
                if (!check(type, bits, radius))
                    return false;
            }
            document.addLayerMask();
            document.setEditingMask(true);
            document.setColor(QColor(127, 127, 127));
            document.fillWithColor(true);
            document.setEditingMask(false);
            document.setLayerOpacity(document.activeLayer(), .63);
            if (!check(type + " masked", bits, 100))
                return false;
            if (hardware && !qEnvironmentVariableIsSet("PHOTOCRAP_TEST_STACK_SHARED_SCRATCH")) {
                const auto bindings = canvas.stackBindingCreations();
                const auto passes   = canvas.stackPasses();
                document.setLayerOpacity(document.activeLayer(), .81);
                if (!check(type + " opacity", bits, 100) ||
                    !require(canvas.stackPasses() > passes &&
                                 canvas.stackBindingCreations() == bindings,
                             "spatial edits redraw with existing bindings"))
                    return false;
            }
            if (profiling && !profilePreview(type, bits))
                return false;
            document.removeLayer();
        }
        if (hardware) {
            document.addAdjustmentLayer("gaussianBlur");
            document.previewLayerParameter("radius", 0.);
            document.finishLayerPreview();
            frame();
            const auto original       = frame();
            auto      *pool           = QThreadPool::globalInstance();
            const int  maximumThreads = pool->maxThreadCount();
            pool->setMaxThreadCount(1);
            QSemaphore started, release;
            auto       blocker = QtConcurrent::run([&] {
                started.release();
                release.acquire();
            });
            const auto cleanup = qScopeGuard([&] {
                release.release();
                blocker.waitForFinished();
                pool->setMaxThreadCount(maximumThreads);
            });
            if (!require(started.tryAcquire(1, 5000),
                         "reserve the worker to test preview independence"))
                return false;
            QSignalSpy images(&document, &Document::imageChanged);
            const int  history = document.historyIndex();
            document.previewLayerParameter("radius", 31.);
            const auto   changed = frame();
            const double ratio   = double(changed.width()) / window->width();
            const auto   rect    = canvas.mapRectToScene(canvas.boundingRect());
            const QRect  crop(qRound(rect.x() * ratio), qRound(rect.y() * ratio),
                              qRound(rect.width() * ratio), qRound(rect.height() * ratio));
            if (!require(images.isEmpty() && changed.copy(crop) != original.copy(crop),
                         "GPU preview changes before a blocked CPU render can finish"))
                return false;
            document.cancelLayerPreview();
            if (!require(
                    document.historyIndex() == history && frame().copy(crop) == original.copy(crop),
                    "cancel immediately restores the previous GPU result without a history step"))
                return false;
            document.removeLayer();
        }
        // Both effects consume an accumulated halo, rather than independently
        // clamping at their tile edges.
        document.addAdjustmentLayer("gaussianBlur");
        document.previewLayerParameter("radius", 3.);
        document.finishLayerPreview();
        document.addAdjustmentLayer("highPass");
        document.previewLayerParameter("radius", 5.);
        document.finishLayerPreview();
        if (!check("chain", bits, 5))
            return false;
        QImage alpha(544, 544, bits == 16 ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
        for (int y = 0; y < alpha.height(); ++y)
            for (int x = 0; x < alpha.width(); ++x)
                alpha.setPixelColor(x, y,
                                    QColor::fromRgba64(ushort(x * 173), ushort(y * 199),
                                                       ushort(x * 37 + y * 67),
                                                       ushort((x + y) * 59)));
        const auto alphaPath = directory.filePath(QString("alpha%1.png").arg(bits));
        if (!require(alpha.save(alphaPath) && document.openFile(QUrl::fromLocalFile(alphaPath)),
                     "load a two-dimensional transparent reference"))
            return false;
        canvas.setSize({544, 544});
        document.addAdjustmentLayer("gaussianBlur");
        document.previewLayerParameter("radius", 7.);
        document.finishLayerPreview();
        document.addAdjustmentLayer("unsharpMask");
        document.previewLayerParameter("radius", 3.);
        document.finishLayerPreview();
        if (!check("transparent 2D chain", bits, 7))
            return false;
        canvas.setSize({1056, 96});
    }
    canvas.setDocument(nullptr);
    frame();
    frame();
    if (!require(canvas.stackTextureBytes() == 0 && !canvas.gpuStackActive(),
                 "removing the document releases all stack and spatial targets"))
        return false;
    qInfo() << "Spatial render checks passed; hardware:" << hardware;
    return true;
}
