#pragma once
#include "CanvasItem.h"
#include <QElapsedTimer>
#include <QEventLoop>
#include <QPainter>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <algorithm>
inline bool runDisplayPyramidSmoke(QQuickWindow *)
{
    auto  holder = std::make_unique<QQuickWindow>();
    auto *window = holder.get();
    window->resize(640, 480);
    window->show();
    QTest::qWait(100);
    const auto require = [](bool ok, const char *message) {
        if (!ok)
            qWarning("Pyramid check failed: %s", message);
        return ok;
    };
    const bool hardware =
        !qEnvironmentVariableIsSet("PHOTOCRAP_TEST_GL_LEGACY") &&
        QSGRendererInterface::isApiRhiBased(window->rendererInterface()->graphicsApi());
    Document   document;
    CanvasItem canvas;
    canvas.setParentItem(window->contentItem());
    canvas.setPosition({40, 120});
    canvas.setZ(1000);
    canvas.setDocument(&document);
    canvas.setGpuStackEnabled(true);
    canvas.setTextureCacheMiB(256);
    const auto frame = [&] {
        window->update();
        QTest::qWait(70);
        return window->grabWindow();
    };
    QTemporaryDir directory;
    for (int bits : {8, 16}) {
        const auto cpuBefore = canvas.displayLevelSourcePixels();
        QImage     original(1057, 545,
                            bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                       : QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < original.height(); ++y)
            for (int x = 0; x < original.width(); ++x)
                original.setPixelColor(
                    x, y, QColor::fromRgbF((x % 79) / 78., (y % 61) / 60., ((x + y) % 107) / 106.));
        const auto path = directory.filePath(QString("pyramid-%1.png").arg(bits));
        if (!require(original.save(path) && document.openFile(QUrl::fromLocalFile(path)),
                     "load odd-sized native pixels"))
            return false;
        const auto  reference = document.composite();
        const qreal ratio     = window->effectiveDevicePixelRatio();
        const auto  check     = [&](int level) {
            const QSize size(reference.width() >> level, reference.height() >> level);
            canvas.setSize({size.width() / ratio, size.height() / ratio});
            canvas.setViewportRect(canvas.boundingRect());
            frame();
            const auto screenshot = frame();
            const auto expected =
                reference.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                    .convertToFormat(QImage::Format_ARGB32_Premultiplied);
            int        maximum = 0;
            double     total   = 0;
            int        samples = 0;
            const auto origin  = canvas.mapToScene({0, 0}) * ratio;
            for (int y = 0; y < size.height(); ++y)
                for (int x = 0; x < size.width(); ++x) {
                    const auto actual =
                        screenshot.pixelColor(qRound(origin.x()) + x, qRound(origin.y()) + y);
                    const auto wanted = expected.pixelColor(x, y);
                    const int  delta  = qMax(qAbs(actual.red() - wanted.red()),
                                             qMax(qAbs(actual.green() - wanted.green()),
                                                  qAbs(actual.blue() - wanted.blue())));
                    maximum           = qMax(maximum, delta);
                    total += delta;
                    ++samples;
                }
            qInfo() << "GPU pyramid" << bits << level << "maximum" << maximum << "mean"
                    << total / samples << "bytes" << canvas.stackTextureBytes();
            return require(canvas.gpuStackActive() == hardware,
                           "hardware pyramid or software fallback selected") &&
                   require(maximum <= 2, "full reduced image matches the global CPU sampling grid");
        };
        const auto pipelines = canvas.stackPipelineCreations();
        if (!check(1))
            return false;
        const auto firstLevelPipelines = canvas.stackPipelineCreations();
        for (int level : {2, 3, 4})
            if (!check(level))
                return false;
        if (hardware) {
            if (!require(firstLevelPipelines <= pipelines + 3 &&
                             canvas.stackPipelineCreations() == firstLevelPipelines,
                         "all tiles and zoom levels reuse compatible reduction pipelines"))
                return false;
            const auto uploads = canvas.stackTextureUploads(), passes = canvas.stackPasses(),
                       reductions = canvas.gpuReductionPasses();
            if (!require(reductions > 0 && canvas.displayLevelSourcePixels() == cpuBefore,
                         "reduction ran on GPU without CPU scaling"))
                return false;
            if (!check(2) || !check(4))
                return false;
            if (!require(canvas.stackTextureUploads() == uploads &&
                             canvas.stackPasses() == passes &&
                             canvas.gpuReductionPasses() == reductions,
                         "zooming reuses native textures and completed GPU levels"))
                return false;
            QList<double> timings;
            for (int iteration = 0; iteration < 10; ++iteration) {
                QCoreApplication::processEvents();
                QEventLoop loop;
                bool       swapped = false;
                QObject::connect(
                    window, &QQuickWindow::frameSwapped, &loop,
                    [&] {
                        swapped = true;
                        loop.quit();
                    },
                    Qt::QueuedConnection);
                QTimer::singleShot(3000, &loop, &QEventLoop::quit);
                QElapsedTimer timer;
                timer.start();
                const int level = iteration % 2 ? 2 : 4;
                canvas.setSize(
                    {(reference.width() >> level) / ratio, (reference.height() >> level) / ratio});
                canvas.setViewportRect(canvas.boundingRect());
                window->update();
                loop.exec();
                if (!require(swapped, "cached zoom submits a frame"))
                    return false;
                timings.append(timer.nsecsElapsed() / 1e6);
            }
            std::sort(timings.begin(), timings.end());
            qInfo() << "Cached zoom frame submission ms" << bits << "median" << timings[5]
                    << "maximum" << timings.last();
            if (!check(4))
                return false;
            const auto   parameters         = canvas.stackUniformUpdates();
            const auto   previousReductions = canvas.gpuReductionPasses();
            const auto   sample             = canvas.mapToScene({5.5 / ratio, 5.5 / ratio}) * ratio;
            const QPoint pixel(qFloor(sample.x()), qFloor(sample.y()));
            const auto   oldPixel = frame().pixelColor(pixel);
            document.setTool("pencil");
            document.setColor(Qt::magenta);
            document.setBrushSize(48);
            document.beginStroke(87, 87);
            document.endStroke();
            frame();
            const auto changed = frame();
            if (!require(canvas.gpuReductionPasses() > previousReductions,
                         "painting redraws the GPU pyramid") ||
                !require(changed.pixelColor(pixel) != oldPixel,
                         "the reduced image shows the new brush stroke") ||
                !require(canvas.stackUniformUpdates() == parameters,
                         "the redraw reuses fixed reduction parameters"))
                return false;
            document.undo();
            if (!check(4))
                return false;
            canvas.setTextureCacheMiB(0);
            frame();
            frame();
            if (!require(!canvas.gpuStackActive(),
                         "small budget falls back without losing the image"))
                return false;
            canvas.setTextureCacheMiB(256);
            if (!check(2))
                return false;
            window->setPersistentSceneGraph(false);
            window->setPersistentGraphics(false);
            window->hide();
            QTest::qWait(100);
            window->releaseResources();
            if (!QTest::qWaitFor([&] { return canvas.stackTextureBytes() == 0; }, 1000)) {
                // releaseResources is a platform-dependent hint; destroying the
                // window forces the scene-graph lifecycle.
                canvas.setParentItem(nullptr);
                holder.reset();
                QTest::qWait(100);
                holder = std::make_unique<QQuickWindow>();
                window = holder.get();
                window->resize(640, 480);
                canvas.setParentItem(window->contentItem());
            }
            if (!require(canvas.stackTextureBytes() == 0,
                         "context release drops tile and pyramid accounting"))
                return false;
            window->show();
            frame();
            if (!check(2))
                return false;
        }
    }
    canvas.setDocument(nullptr);
    frame();
    frame();
    return require(canvas.stackTextureBytes() == 0,
                   "removing the document releases all GPU levels");
}
