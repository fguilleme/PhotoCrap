#pragma once
#include "CanvasItem.h"
#include <QElapsedTimer>
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
    canvas.setPosition({40, 120});
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
    const auto check = [&](const QString &type, int bits, int radius) {
        // Compute the native reference after committing the gesture, so it is
        // independent of the CPU's intentionally reduced drag-preview path.
        const QImage reference = document.composite();
        frame();
        const QImage result = frame();
        if (!require(!result.isNull() && canvas.gpuStackActive() == hardware,
                     "the complete spatial stack renders or falls back in software"))
            return false;
        const double ratio   = double(result.width()) / window->width();
        int          maximum = 0;
        for (int y : {0, 1, 35, qMin(95, reference.height() - 1), qMin(511, reference.height() - 1),
                      qMin(512, reference.height() - 1), reference.height() - 1})
            for (int x : {0, 1, 15, 255, 510, 511, 512, 513, qMin(1023, reference.width() - 1),
                          qMin(1024, reference.width() - 1), reference.width() - 1}) {
                const QPointF scene = canvas.mapToScene({x + .5, y + .5});
                const QPoint  point(qFloor(scene.x() * ratio), qFloor(scene.y() * ratio));
                if (!require(result.rect().contains(point),
                             "reference sample stays within the window"))
                    return false;
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
        qInfo() << "Spatial reference" << type << bits << radius << "maximum display difference"
                << maximum;
        return true;
    };
    for (int bits : {8, 16}) {
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
