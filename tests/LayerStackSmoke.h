#pragma once
#include "CanvasItem.h"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <cmath>

inline bool runLayerStackSmoke(QQuickWindow *window)
{
    const auto require = [](bool ok, const char *message) {
        if (!ok)
            qWarning("Layer stack check failed: %s", message);
        return ok;
    };
    Document   document;
    CanvasItem canvas(window->contentItem());
    canvas.setZ(1000);
    canvas.setPosition({40, 120});
    canvas.setSize({600, 128});
    canvas.setDocument(&document);
    canvas.setGpuStackEnabled(true);
    const bool hardware =
        !qEnvironmentVariableIsSet("PHOTOCRAP_TEST_GL_LEGACY") &&
        QSGRendererInterface::isApiRhiBased(window->rendererInterface()->graphicsApi());
    const bool streaming = qEnvironmentVariableIsSet("PHOTOCRAP_TEST_STACK_STREAMING") ||
                           qEnvironmentVariableIsSet("PHOTOCRAP_TEST_STACK_SHARED_SCRATCH");
    canvas.setTextureCacheMiB(
        256); // Both the reference fixture and main QML canvas share the budget.

    const auto frame = [&] {
        window->update();
        QTest::qWait(60);
        return window->grabWindow();
    };
    for (int bits : {8, 16}) {
        if (!document.newDocument(1200, 128, false, bits))
            return false;
        document.setColor(QColor(45, 98, 187));
        document.fillWithColor(true);
        document.addLayer();
        document.setColor(QColor(210, 56, 117));
        document.fillWithColor(true);
        document.setLayerOpacity(document.activeLayer(), .63);
        document.addLayerMask();
        document.setEditingMask(true);
        document.setColor(QColor(127, 127, 127));
        document.fillWithColor(true);
        document.setEditingMask(false);
        for (int mode = 0; mode < 27; ++mode) {
            document.setLayerBlend(document.activeLayer(), mode);
            frame();
            const auto result   = frame();
            const auto expected = document.composite();
            const bool eligible = mode != 18 || document.gpuLayerStack().valid;
            if (result.isNull() || canvas.gpuStackActive() != (hardware && eligible))
                qWarning() << "Stack activation" << bits << mode << document.gpuLayerStack().valid
                           << streaming;
            if (!require(!result.isNull() && canvas.gpuStackActive() == (hardware && eligible),
                         "hardware stack or software fallback is active"))
                return false;
            QImage dissolveReference;
            if (mode == 1) {
                canvas.setGpuStackEnabled(false);
                frame();
                dissolveReference = frame();
                canvas.setGpuStackEnabled(true);
                frame();
            }
            const double ratio = double(result.width()) / window->width();
            for (int x : {10, 255, 511, 512, 1023, 1024, 1190}) {
                const QPointF point = canvas.mapToScene({(x + .5) * .5, 64.5});
                const QColor  actual =
                    result.pixelColor(qFloor(point.x() * ratio), qFloor(point.y() * ratio));
                const QColor reference =
                    mode == 1 ? dissolveReference.pixelColor(qFloor(point.x() * ratio),
                                                             qFloor(point.y() * ratio))
                              : expected.pixelColor(x, 64);
                if (qAbs(actual.red() - reference.red()) > 2 ||
                    qAbs(actual.green() - reference.green()) > 2 ||
                    qAbs(actual.blue() - reference.blue()) > 2) {
                    qWarning() << "Stack difference" << bits << mode << x << actual << reference;
                    return false;
                }
            }
        }
        if (hardware) {
            const auto uploads = canvas.stackTextureUploads();
            const auto passes  = canvas.stackPasses();
            frame();
            frame();
            if (!require(canvas.stackTextureUploads() == uploads && canvas.stackPasses() == passes,
                         "unchanged stacks reuse source textures and completed intermediates"))
                return false;
            const auto snapshot = canvas.stackSnapshotPreparations();
            canvas.setSize({600, 129});
            frame();
            canvas.setSize({600, 128});
            frame();
            if (!require(canvas.stackSnapshotPreparations() == snapshot,
                         "geometry-only frames reuse the stack snapshot"))
                return false;
            if (!require(canvas.stackTextureUploads() == uploads && canvas.stackPasses() == passes,
                         "geometry-only frames reuse the rendered GPU tiles"))
                return false;
            document.setLayerBlend(document.activeLayer(), 0);
            frame();
            frame();
            if (!require(canvas.stackSnapshotPreparations() > snapshot,
                         "layer edits refresh the stack snapshot"))
                return false;
            const auto before = canvas.stackTextureUploads();
            const auto beforePixel =
                document.layerImage(document.activeLayer()).pixelColor(510, 64);
            const auto beforeHistory = document.historyIndex();
            document.setTool("pencil");
            document.setBrushSize(8);
            document.setColor(Qt::green);
            document.beginStroke(510, 64);
            document.endStroke();
            if (!require(document.historyIndex() == beforeHistory + 1 &&
                             document.layerImage(document.activeLayer()).pixelColor(510, 64) !=
                                 beforePixel,
                         "the test stroke changes the editable plane"))
                return false;
            frame();
            frame();
            const quint64 uploadBound =
                streaming ? 2 * document.gpuLayerStack().layers.size() * 3 : 2;
            if (canvas.stackTextureUploads() <= before ||
                canvas.stackTextureUploads() - before > uploadBound)
                qWarning() << "Regional stack uploads" << bits << before
                           << canvas.stackTextureUploads() << canvas.gpuStackActive();
            if (!require(canvas.stackTextureUploads() > before &&
                             canvas.stackTextureUploads() - before <= uploadBound,
                         "a local edit uploads only the two touched tile regions"))
                return false;
        }
        document.addLayer();
        document.setColor(Qt::green);
        document.fillWithColor(true);
        document.setClippingMask(true);
        for (bool visible : {true, false}) {
            document.setLayerVisible(1, visible);
            frame();
            const auto    result    = frame();
            const auto    reference = document.composite().pixelColor(100, 64);
            const QPointF point     = canvas.mapToScene({50.25, 64.5});
            const double  ratio     = double(result.width()) / window->width();
            const auto    actual =
                result.pixelColor(qFloor(point.x() * ratio), qFloor(point.y() * ratio));
            if (!require(qAbs(actual.red() - reference.red()) <= 2 &&
                             qAbs(actual.green() - reference.green()) <= 2 &&
                             qAbs(actual.blue() - reference.blue()) <= 2,
                         "clipping uses the masked base alpha and hidden bases break the chain"))
                return false;
        }
        document.addAdjustmentLayer("invert");
        document.setLayerBlend(document.activeLayer(), 1);
        frame();
        frame();
        if (!require(!canvas.gpuStackActive(),
                     "unsupported adjustment switches the complete stack to CPU"))
            return false;
    }
    for (int bits : {8, 16}) {
        if (!document.newDocument(1200, 128, false, bits))
            return false;
        document.addAdjustmentLayer("invert");
        document.setLayerBlend(document.activeLayer(), 1);
        document.addLayer();
        document.setColor(QColor(45, 98, 187));
        document.fillWithColor(true);
        const int cover = document.activeLayer();
        document.addLayerMask();
        document.beginLayerPreview();
        document.previewLayerParameter("_maskFeather", 7);
        document.finishLayerPreview("White mask");
        document.setEditingMask(false);
        document.addLayer();
        document.setColor(QColor(57, 213, 113, 143));
        document.fillWithColor(true);
        document.setClippingMask(true);
        frame();
        const auto rendered = frame(), reference = document.composite();
        if (!require(document.gpuLayerStack().valid && canvas.gpuStackActive() == hardware,
                     "an opaque cover keeps hidden unsupported effects off the GPU stack"))
            return false;
        const double ratio = double(rendered.width()) / window->width();
        for (int x : {10, 255, 511, 512, 1023, 1024, 1190}) {
            const auto point = canvas.mapToScene({(x + .5) * .5, 64.5});
            const auto actual =
                rendered.pixelColor(qFloor(point.x() * ratio), qFloor(point.y() * ratio));
            const auto expected = reference.pixelColor(x, 64);
            if (!require(qAbs(actual.red() - expected.red()) <= 2 &&
                             qAbs(actual.green() - expected.green()) <= 2 &&
                             qAbs(actual.blue() - expected.blue()) <= 2,
                         "opaque culling and its clipped overlay match the CPU"))
                return false;
        }
        document.setLayerOpacity(cover, .5);
        frame();
        frame();
        if (!require(!document.gpuLayerStack().valid && !canvas.gpuStackActive(),
                     "fractional cover opacity restores the complete CPU fallback"))
            return false;
        document.setLayerOpacity(cover, 1.);
        frame();
        frame();
        if (!require(document.gpuLayerStack().valid && canvas.gpuStackActive() == hardware,
                     "restoring opacity reconstructs the eligible GPU stack"))
            return false;
    }
    for (int bits : {8, 16}) {
        if (!document.newDocument(1200, 128, false, bits))
            return false;
        document.addLayer();
        document.setShapeNewLayer(false);
        document.setShapeStrokeEnabled(false);
        document.setShapeFillColor(Qt::red);
        document.setTool("rectangle");
        document.beginStroke(50, 20);
        document.continueStroke(90, 90);
        document.endStroke();
        frame();
        frame();
        document.setTool("move");
        document.beginStroke(60, 64);
        for (int offset : {520, 1020}) {
            const auto uploads = canvas.stackTextureUploads();
            document.continueStroke(60 + offset, 64);
            frame();
            const auto   rendered  = frame();
            const auto   reference = document.composite();
            const double ratio     = double(rendered.width()) / window->width();
            for (int x : {60, 580, 1080}) {
                const QPointF point = canvas.mapToScene({(x + .5) * .5, 64.5});
                const QColor  actual =
                    rendered.pixelColor(qFloor(point.x() * ratio), qFloor(point.y() * ratio));
                const QColor expected = reference.pixelColor(x, 64);
                if (!require(
                        qAbs(actual.red() - expected.red()) <= 2 &&
                            qAbs(actual.green() - expected.green()) <= 2 &&
                            qAbs(actual.blue() - expected.blue()) <= 2,
                        "sparse layer movement clears old footprints and displays the new tile"))
                    return false;
            }
            // The first GPU region only sees unchanged border pixels on the
            // second move, even though their storage tile changes version.
            const int affected =
                streaming ? 2 * int(document.gpuLayerStack().layers.size()) * 3 : 2;
            if (!require(canvas.gpuStackActive() == hardware &&
                             (!hardware || (canvas.stackTextureUploads() > uploads &&
                                            canvas.stackTextureUploads() - uploads <= affected)),
                         "partial border keys retain untouched GPU stack textures"))
                return false;
        }
        document.endStroke();
    }
    canvas.setSize({800, 128});
    for (int bits : {8, 16}) {
        if (!document.newDocument(800, 128, false, bits))
            return false;
        document.setTool("text");
        document.setTextSize(28);
        document.setColor(Qt::black);
        document.beginStroke(20, 20);
        document.endStroke();
        document.setText("Text");
        document.finishTextEdit();
        frame();
        document.setTool("move");
        document.beginStroke(30, 30);
        for (int offset : {600, 0}) {
            document.continueStroke(30 + offset, 30);
            frame();
            const auto   rendered  = frame();
            const auto   reference = document.composite();
            const double ratio     = double(rendered.width()) / window->width();
            for (int origin : {0, 600})
                for (int y = 10; y < 80; y += 3)
                    for (int x = origin + 10; x < origin + 150; x += 3) {
                        const auto   point    = canvas.mapToScene({x + .5, y + .5});
                        const QColor actual   = rendered.pixelColor(qFloor(point.x() * ratio),
                                                                    qFloor(point.y() * ratio));
                        const QColor expected = reference.pixelColor(x, y);
                        if (!require(qAbs(actual.red() - expected.red()) <= 2 &&
                                         qAbs(actual.green() - expected.green()) <= 2 &&
                                         qAbs(actual.blue() - expected.blue()) <= 2 &&
                                         canvas.gpuStackActive() == hardware,
                                     "regional text displays native glyphs and clears prior "
                                     "positions")) {
                            qWarning() << bits << x << y << actual << expected
                                       << canvas.gpuStackActive() << hardware;
                            return false;
                        }
                    }
        }
        document.endStroke();
    }
    // Native channel tables compose multiple adjustments without an 8-bit
    // proxy, including partially transparent input and nonuniform masks.
    QTemporaryDir directory;
    for (int bits : {8, 16}) {
        QImage source(1057, 64, bits == 16 ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
        for (int y = 0; y < source.height(); ++y)
            for (int x = 0; x < source.width(); ++x)
                source.setPixelColor(x, y,
                                     QColor::fromRgba64((x * 73 + y * 101) % 65536,
                                                        (x * 193 + y * 47) % 65536,
                                                        (x * 317 + y * 79) % 65536,
                                                        17000 + (x * 33 + y * 89) % 48536));
        const auto path = directory.filePath(QString::number(bits) + ".png");
        if (!source.save(path))
            return false;
        canvas.setSize({600, 64});
        for (const auto &type :
             {"curves", "levels", "brightnessContrast", "exposure", "colorBalance", "posterize",
              "invert", "light", "color", "blackWhite", "gradientMap", "photoFilter",
              "channelMixer", "threshold", "colorLookup", "selectiveColor", "vibrance",
              "hueSaturation"}) {
            if (!document.openFile(QUrl::fromLocalFile(path)))
                return false;
            document.addAdjustmentLayer(type);
            document.beginLayerPreview();
            document.previewLayerParameter("exposure", .8);
            document.previewLayerParameter("contrast", 25);
            document.previewLayerParameter("cyanRed", 17);
            document.previewLayerParameter("levels", 5);
            document.previewLayerParameter("temperature", 25);
            document.previewLayerParameter("saturation", 22);
            document.previewLayerParameter("vibrance", 30);
            document.previewLayerParameter("redBlue", 21);
            document.previewLayerParameter("reds", 70);
            document.previewLayerParameter("redsCyanRed", 20);
            document.previewLayerParameter("tintEnabled", true);
            document.previewLayerParameter("hue", 35);
            document.previewLayerParameter("blacks", -17);
            document.previewLayerParameter("_maskFeather", 7);
            document.finishLayerPreview("Channel table");
            document.setLayerOpacity(document.activeLayer(), .61);
            document.setEditingMask(true);
            document.setTool("pencil");
            document.setBrushSize(29);
            document.setColor(Qt::gray);
            document.beginStroke(510, 32);
            document.continueStroke(1030, 32);
            document.endStroke();
            document.setEditingMask(false);
            document.addAdjustmentLayer("invert");
            const auto stack = document.gpuLayerStack();
            if (!require(stack.valid && !stack.layers.last().toneTable.isNull(),
                         "native channel adjustments stay in the GPU stack"))
                return false;
            const auto key = stack.layers.last().toneTable.cacheKey();
            if (!require(document.gpuLayerStack().layers.last().toneTable.cacheKey() == key,
                         "unchanged tables are reused"))
                return false;
            frame();
            const auto actual = frame();
            if (!require(canvas.gpuStackActive() == hardware,
                         "channel stack uses hardware when available"))
                return false;
            if (hardware) {
                const auto uploads = canvas.stackTextureUploads();
                const auto passes  = canvas.stackPasses();
                frame();
                frame();
                if (!require(canvas.stackTextureUploads() == uploads &&
                                 canvas.stackPasses() == passes,
                             "unchanged tonal stacks neither upload nor redraw native tiles"))
                    return false;
            }
            canvas.setGpuStackEnabled(false);
            frame();
            const auto   expected = frame();
            const double ratio    = double(actual.width()) / window->width();
            for (int y : {4, 20, 32, 48, 60})
                for (int x : {3, 89, 254, 255, 256, 510, 511, 512, 769, 1023, 1024, 1050}) {
                    const auto p = canvas.mapToScene({(x + .5) * 600. / 1057., y + .5}) * ratio;
                    const auto a = actual.pixelColor(qFloor(p.x()), qFloor(p.y()));
                    const auto b = expected.pixelColor(qFloor(p.x()), qFloor(p.y()));
                    if (qAbs(a.red() - b.red()) > 2 || qAbs(a.green() - b.green()) > 2 ||
                        qAbs(a.blue() - b.blue()) > 2) {
                        qWarning() << "Native channel stack difference" << bits << type << x << y
                                   << a << b;
                        return false;
                    }
                }
            canvas.setGpuStackEnabled(true);
        }
    }
    for (int bits : {8, 16}) {
        if (!document.newDocument(1200, 128, true, bits))
            return false;
        document.setColor(QColor(27, 93, 179, 105));
        document.fillWithColor(true);
        document.addLayer();
        document.setColor(QColor(197, 42, 111, 173));
        document.fillWithColor(true);
        document.setLayerOpacity(document.activeLayer(), .61);
        for (int mode : {1, 18, 23, 24, 25, 26}) {
            document.setLayerBlend(document.activeLayer(), mode);
            canvas.setGpuStackEnabled(true);
            frame();
            const auto actual = frame();
            if (!require(
                    canvas.gpuStackActive() == (hardware && mode != 18),
                    "transparent HSL/Fondu stay on GPU; hard-mix keeps its exact CPU fallback"))
                return false;
            canvas.setGpuStackEnabled(false);
            frame();
            const auto   expected = frame();
            const double ratio    = double(actual.width()) / window->width();
            for (int x : {10, 255, 511, 512, 1023, 1190}) {
                const auto point = canvas.mapToScene({(x + .5) * .5, 64.5}) * ratio;
                const auto a     = actual.pixelColor(qFloor(point.x()), qFloor(point.y()));
                const auto b     = expected.pixelColor(qFloor(point.x()), qFloor(point.y()));
                if (!require(qAbs(a.red() - b.red()) <= 2 && qAbs(a.green() - b.green()) <= 2 &&
                                 qAbs(a.blue() - b.blue()) <= 2,
                             "native HSL blending retains translucent source and backdrop"))
                    return false;
            }
        }
    }
    canvas.setGpuStackEnabled(true);
    // Every native source coordinate, plus either side of the hard-mix seam.
    for (int bits : {8, 16}) {
        QTemporaryDir directory;
        if (!directory.isValid())
            return false;
        const int  maximum = bits == 16 ? 65535 : 255;
        const auto format =
            bits == 16 ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied;
        QImage base(1057, 195, format), source(base.size(), format);
        for (int y = 0; y < base.height(); ++y)
            for (int x = 0; x < base.width(); ++x) {
                const int  i = (x + (y % 65) * 1057) % (maximum + 1), offset = y / 65 - 1;
                const auto c = [&](int multiplier) { return (i * multiplier) % (maximum + 1); };
                if (bits == 16) {
                    reinterpret_cast<QRgba64 *>(source.scanLine(y))[x] =
                        QRgba64::fromRgba64(c(1), c(53), c(109), 65535);
                    reinterpret_cast<QRgba64 *>(base.scanLine(y))[x] =
                        QRgba64::fromRgba64(qBound(0, maximum - c(1) + offset, maximum),
                                            qBound(0, maximum - c(53) + offset, maximum),
                                            qBound(0, maximum - c(109) + offset, maximum), 65535);
                } else {
                    reinterpret_cast<QRgb *>(source.scanLine(y))[x] = qRgb(c(1), c(53), c(109));
                    reinterpret_cast<QRgb *>(base.scanLine(y))[x] =
                        qRgb(qBound(0, maximum - c(1) + offset, maximum),
                             qBound(0, maximum - c(53) + offset, maximum),
                             qBound(0, maximum - c(109) + offset, maximum));
                }
            }
        const auto baseFile   = QUrl::fromLocalFile(directory.filePath("base.png"));
        const auto sourceFile = QUrl::fromLocalFile(directory.filePath("source.png"));
        if (!base.save(baseFile.toLocalFile()) || !source.save(sourceFile.toLocalFile()) ||
            !document.openFile(baseFile) || !document.importLayer(sourceFile))
            return false;
        canvas.setSize({600, 120});
        for (int mode : {1, 18}) {
            document.setMaskEnabled(false);
            document.setLayerBlend(document.activeLayer(), mode);
            for (double opacity : {.4, .63, 1., .5}) {
                if (opacity == .5) {
                    document.addLayerMask();
                    document.setMaskEnabled(true);
                    document.setEditingMask(true);
                    document.setColor(Qt::gray);
                    document.fillWithColor(true);
                    document.setEditingMask(false);
                }
                document.setLayerOpacity(document.activeLayer(), opacity);
                canvas.setGpuStackEnabled(true);
                frame();
                const auto actual = frame();
                if (!require(canvas.gpuStackActive() == (hardware && (mode != 18 || opacity != .5)),
                             "opaque hard-mix/Fondu use GPU; attenuated hard-mix falls back"))
                    return false;
                canvas.setGpuStackEnabled(false);
                frame();
                const auto   expected = frame();
                const double ratio    = double(actual.width()) / window->width();
                for (int y = 1; y < 195; y += 3)
                    for (int x = 1; x < 1057; x += 7) {
                        const auto point =
                            canvas.mapToScene({(x + .5) * 600 / 1057., (y + .5) * 120 / 195.}) *
                            ratio;
                        const auto a = actual.pixelColor(qFloor(point.x()), qFloor(point.y()));
                        const auto b = expected.pixelColor(qFloor(point.x()), qFloor(point.y()));
                        if (qAbs(a.red() - b.red()) > 2 || qAbs(a.green() - b.green()) > 2 ||
                            qAbs(a.blue() - b.blue()) > 2) {
                            qWarning() << "Discontinuous blend difference" << bits << mode
                                       << opacity << x << y << a << b;
                            return false;
                        }
                    }
            }
        }
    }
    canvas.setGpuStackEnabled(true);
    // Different tile results must survive reuse of the same native scratch and
    // horizontal reduction targets, including after zoom changes and cache hits.
    for (int bits : {8, 16}) {
        if (!document.newDocument(2049, 257, false, bits))
            return false;
        document.setTool("gradient");
        document.setColor(QColor(12, 56, 198));
        document.setSecondaryColor(QColor(224, 140, 21));
        document.beginStroke(0, 0);
        document.continueStroke(2048, 256);
        document.endStroke();
        document.addAdjustmentLayer("colorBalance");
        document.beginLayerPreview();
        document.previewLayerParameter("cyanRed", 17);
        document.finishLayerPreview("Balance");
        for (int width : {600, 300, 150, 600, 300}) {
            canvas.setSize({qreal(width), 128});
            canvas.setGpuStackEnabled(true);
            frame();
            const auto actual = frame();
            if (!require(canvas.gpuStackActive() == hardware,
                         "zoomed stacks remain on available hardware"))
                return false;
            if (hardware) {
                auto budget         = document.gpuLayerStack();
                budget.streamInputs = streaming;
                budget.shareScratch =
                    qEnvironmentVariableIsSet("PHOTOCRAP_TEST_STACK_SHARED_SCRATCH");
                const double scale = qMin(
                    canvas.width() * window->effectiveDevicePixelRatio() / budget.size.width(),
                    canvas.height() * window->effectiveDevicePixelRatio() / budget.size.height());
                budget.displayLevel = qMax(0, int(std::floor(std::log2(1. / scale))));
                const auto estimate =
                    PhotoCrap::Render::stackWorkingBytes(budget, QRect(QPoint{}, budget.size));
                if (!require(canvas.stackTextureBytes() <= quint64(estimate),
                             "actual retained stack resources respect the conservative estimate "
                             "after zoom changes"))
                    return false;
            }
            canvas.setGpuStackEnabled(false);
            frame();
            const auto   expected = frame();
            const double ratio    = double(actual.width()) / window->width();
            for (int y : {9, 31, 61, 97, 120})
                for (int x = 4; x < width - 4; x += 13) {
                    const auto point = canvas.mapToScene({x + .5, y + .5}) * ratio;
                    const auto a     = actual.pixelColor(qFloor(point.x()), qFloor(point.y()));
                    const auto b     = expected.pixelColor(qFloor(point.x()), qFloor(point.y()));
                    if (!require(qAbs(a.red() - b.red()) <= 2 && qAbs(a.green() - b.green()) <= 2 &&
                                     qAbs(a.blue() - b.blue()) <= 2,
                                 "shared scratch preserves independent tile colors at each zoom"))
                        return false;
                }
        }
    }
    canvas.setGpuStackEnabled(true);
    if (hardware && qEnvironmentVariableIsSet("PHOTOCRAP_OPTIMIZATION_PROBES")) {
        canvas.setSize({600, 128});
        frame();
        frame();
        const auto uploads = canvas.stackTextureUploads(), passes = canvas.stackPasses();
        document.beginLayerPreview();
        QVector<double> timings;
        for (int i = 0; i < 20; ++i) {
            QSignalSpy    swapped(window, &QQuickWindow::frameSwapped);
            const auto    beforePasses = canvas.stackPasses();
            QElapsedTimer timer;
            timer.start();
            document.previewLayerParameter("cyanRed", i - 10);
            do {
                swapped.clear();
                window->update();
                const int remaining = qMax(1, 5000 - int(timer.elapsed()));
                if (!require(!swapped.isEmpty() || swapped.wait(remaining),
                             "latency probe observes a presented frame"))
                    return false;
                if (!require(timer.elapsed() < 5000,
                             "latency probe actually renders the changed table"))
                    return false;
            } while (canvas.stackPasses() == beforePasses);
            timings.append(timer.nsecsElapsed() / 1e6);
        }
        document.finishLayerPreview("Latency probe");
        frame();
        std::sort(timings.begin(), timings.end());
        const auto    api     = window->rendererInterface()->graphicsApi();
        const QString backend = api == QSGRendererInterface::Metal        ? "metal"
                                : api == QSGRendererInterface::OpenGL     ? "opengl"
                                : api == QSGRendererInterface::Vulkan     ? "vulkan"
                                : api == QSGRendererInterface::Direct3D11 ? "d3d11"
                                : api == QSGRendererInterface::Direct3D12 ? "d3d12"
                                                                          : "unknown";
        QJsonObject   probe{
            {"kind", "native-stack-frame"},
            {"backend", backend},
            {"bits", document.channelBits()},
            {"graphics_api", int(window->rendererInterface()->graphicsApi())},
            {"dpr", window->effectiveDevicePixelRatio()},
            {"samples", timings.size()},
            {"median_ms", (timings[(timings.size() - 1) / 2] + timings[timings.size() / 2]) / 2},
            {"p95_ms",
             timings[qMin(timings.size() - 1, qsizetype((timings.size() * 95 + 99) / 100 - 1))]},
            {"uploads", double(canvas.stackTextureUploads() - uploads)},
            {"passes", double(canvas.stackPasses() - passes)},
            {"pipeline_creations", double(canvas.stackPipelineCreations())},
            {"stack_bytes", double(canvas.stackTextureBytes())},
            {"graphics_memory", QJsonObject::fromVariantMap(canvas.graphicsMemoryUsage())}};
        qInfo().noquote() << "PHOTOCRAP_PROBE"
                          << QJsonDocument(probe).toJson(QJsonDocument::Compact);
    }
    for (const int bits : {8, 16}) {
        canvas.setSize({600, 128});
        if (!document.newDocument(600, 128, false, bits))
            return false;
        if (bits == 8) {
            document.setColor(QColor(72, 113, 189));
            document.fillWithColor(true);
            document.setColor(QColor(224, 84, 42));
            document.setBrushSize(35);
            document.beginStroke(512, 64);
            document.endStroke();
        }
        document.addFrequencySeparation(7);
        document.setLayerVisible(0, false);
        const auto stack = document.gpuLayerStack();
        if (!require(stack.valid && stack.layers.size() == 1 && stack.halo == 0,
                     "an untouched frequency pair supplies one native source"))
            return false;
        if (bits == 16 && !require(stack.layers.first().image.size() == stack.size &&
                                       stack.layers.first().image.isUniform(),
                                   "a uniform native plane can be uploaded as a single texel"))
            return false;
        canvas.setGpuStackEnabled(true);
        frame();
        const auto actual = frame();
        if (!require(!actual.isNull() && canvas.gpuStackActive() == hardware,
                     "an untouched frequency pair stays on the stack renderer"))
            return false;
        canvas.setGpuStackEnabled(false);
        frame();
        const auto expected = frame();
        if (expected.isNull())
            return false;
        const double ratio = double(actual.width()) / window->width();
        for (const int x : {7, 498, 511, 512, 524, 587}) {
            const auto   point = canvas.mapToScene({x + .5, 64.5}) * ratio;
            const QPoint pixel(qFloor(point.x()), qFloor(point.y()));
            const QColor a = actual.pixelColor(pixel), b = expected.pixelColor(pixel);
            if (!require(qAbs(a.red() - b.red()) <= 2 && qAbs(a.green() - b.green()) <= 2 &&
                             qAbs(a.blue() - b.blue()) <= 2,
                         "native frequency source matches the CPU composite"))
                return false;
        }
        canvas.setGpuStackEnabled(true);
        document.setLayerOpacity(document.activeLayer(), .5);
        frame();
        frame();
        if (!require(!document.gpuLayerStack().valid && !canvas.gpuStackActive(),
                     "an edited frequency pair uses the CPU compositor"))
            return false;
    }
    for (const bool translucent : {false, true})
        for (const int bits : {8, 16}) {
            QImage source(1057, 71, bits == 16 ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
            for (int y = 0; y < source.height(); ++y)
                for (int x = 0; x < source.width(); ++x)
                    source.setPixelColor(x, y,
                                         QColor::fromRgba64(ushort(x * 73 + y * 211),
                                                            ushort(x * 193 + y * 47),
                                                            ushort(x * 317 + y * 79),
                                                            translucent ? ushort(x * 135 + y * 311)
                                                                        : ushort(65535)));
            const auto path = directory.filePath("noise-" + QString::number(bits) + ".png");
            if (!source.save(path) || !document.openFile(QUrl::fromLocalFile(path)))
                return false;
            canvas.setSize({600, 71});
            document.addAdjustmentLayer("noise");
            document.addLayerMask();
            document.fillMaskWithColor(QColor(173, 173, 173));
            document.setEditingMask(false);
            document.setLayerOpacity(document.activeLayer(), .63);
            for (const double amount : {0., .4, 13.5, 100.}) {
                document.beginLayerPreview();
                document.previewLayerParameter("amount", amount);
                document.finishLayerPreview("Noise amount");
                const auto stack = document.gpuLayerStack();
                if (!require(stack.valid && stack.layers.last().tonal &&
                                 stack.layers.last().toneTable.isNull(),
                             "noise stays in the GPU stack without a lookup texture"))
                    return false;
                canvas.setGpuStackEnabled(true);
                frame();
                const auto actual = frame();
                if (!require(!actual.isNull() && canvas.gpuStackActive() == hardware,
                             "noise uses hardware when available"))
                    return false;
                canvas.setGpuStackEnabled(false);
                frame();
                const auto expected = frame();
                if (expected.isNull())
                    return false;
                const double ratio = double(actual.width()) / window->width();
                for (const int y : {0, 7, 34, 69})
                    for (const int x : {0, 41, 510, 511, 512, 513, 1023, 1024, 1052}) {
                        const auto p = canvas.mapToScene({(x + .5) * 600. / 1057., y + .5}) * ratio;
                        const auto a = actual.pixelColor(qFloor(p.x()), qFloor(p.y()));
                        const auto b = expected.pixelColor(qFloor(p.x()), qFloor(p.y()));
                        if (qAbs(a.red() - b.red()) > 2 || qAbs(a.green() - b.green()) > 2 ||
                            qAbs(a.blue() - b.blue()) > 2) {
                            qWarning() << "Noise stack difference" << bits << translucent << amount
                                       << x << y << a << b;
                            return false;
                        }
                    }
            }
        }
    for (const int bits : {8, 16}) {
        QImage source(1057, 527, bits == 16 ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
        for (int y = 0; y < source.height(); ++y)
            for (int x = 0; x < source.width(); ++x)
                source.setPixelColor(
                    x, y,
                    QColor::fromRgba64(ushort(x * 311 + y * 733), ushort(x * 193 + y * 947),
                                       ushort(x * 317 + y * 279),
                                       ushort(8000 + (x * 135 + y * 311) % 57536)));
        const auto path = directory.filePath("mosaic-" + QString::number(bits) + ".png");
        if (!source.save(path) || !document.openFile(QUrl::fromLocalFile(path)))
            return false;
        canvas.setSize({600, 300});
        document.addAdjustmentLayer("mosaic");
        for (const int cell : {1, 2, 17, 200}) {
            document.beginLayerPreview();
            document.previewLayerParameter("cellSize", cell);
            document.finishLayerPreview("Mosaic cell size");
            if (cell == 17) {
                document.addLayerMask();
                document.fillMaskWithColor(QColor(173, 173, 173));
                document.setEditingMask(false);
                document.setLayerOpacity(document.activeLayer(), .63);
            }
            const auto stack = document.gpuLayerStack();
            if (!require(stack.valid && stack.halo == (cell == 1 ? 0 : cell + 2) &&
                             stack.layers.last().toneTable.isNull() == (cell == 1),
                         "mosaic has a native coordinate table and a sampling halo"))
                return false;
            const auto tableKey = stack.layers.last().toneTable.cacheKey();
            if (!require(document.gpuLayerStack().layers.last().toneTable.cacheKey() == tableKey,
                         "mosaic keeps the coordinate table between frames"))
                return false;
            canvas.setGpuStackEnabled(true);
            frame();
            const auto actual = frame();
            if (!require(!actual.isNull() && canvas.gpuStackActive() == hardware,
                         "mosaic uses hardware when available"))
                return false;
            canvas.setGpuStackEnabled(false);
            frame();
            const auto expected = frame();
            if (expected.isNull())
                return false;
            const double ratio = double(actual.width()) / window->width();
            for (const int y : {0, 9, 255, 510, 511, 512, 523})
                for (const int x : {0, 41, 510, 511, 512, 513, 1023, 1024, 1052}) {
                    const auto p =
                        canvas.mapToScene({(x + .5) * 600. / 1057., (y + .5) * 300. / 527.}) *
                        ratio;
                    const auto a = actual.pixelColor(qFloor(p.x()), qFloor(p.y()));
                    const auto b = expected.pixelColor(qFloor(p.x()), qFloor(p.y()));
                    if (qAbs(a.red() - b.red()) > 2 || qAbs(a.green() - b.green()) > 2 ||
                        qAbs(a.blue() - b.blue()) > 2) {
                        qWarning() << "Mosaic stack difference" << bits << cell << x << y << a << b;
                        return false;
                    }
                }
        }
    }
    if (hardware && qEnvironmentVariableIsSet("PHOTOCRAP_TEST_STACK_SHARED_SCRATCH")) {
        canvas.setGpuStackEnabled(true);
        canvas.setSize({128, 64});
        if (!document.newDocument(128, 64, false, 16))
            return false;
        document.addAdjustmentLayer("mosaic");
        const auto setCell = [&](int cell) {
            document.beginLayerPreview();
            document.previewLayerParameter("cellSize", cell);
            document.finishLayerPreview("Mosaic cell size");
            frame();
            frame();
        };
        setCell(1);
        auto uploads = canvas.stackTextureUploads();
        setCell(17);
        if (!require(canvas.gpuStackActive() && canvas.stackTextureUploads() == uploads + 1,
                     "adding a lookup texture keeps the shared workspace and source upload"))
            return false;
        uploads = canvas.stackTextureUploads();
        setCell(1);
        if (!require(canvas.gpuStackActive() && canvas.stackTextureUploads() == uploads,
                     "removing a lookup texture keeps the shared workspace and source upload"))
            return false;
        const auto compactBytes = canvas.stackTextureBytes();
        document.setActiveLayer(0);
        document.setTool("pencil");
        document.setColor(Qt::red);
        document.setBrushSize(8);
        document.beginStroke(24, 32);
        document.endStroke();
        frame();
        frame();
        if (!require(canvas.gpuStackActive() &&
                         canvas.stackTextureBytes() == compactBytes + (128 * 64 - 1) * 16,
                     "painting allocates only the larger streamed color slot"))
            return false;
        document.undo();
        frame();
        frame();
        if (!require(canvas.gpuStackActive() && canvas.stackTextureBytes() == compactBytes,
                     "undo restores the one-texel streamed color slot"))
            return false;
    }
    if (hardware && !streaming) {
        canvas.setSize({600, 128});
        canvas.setGpuStackEnabled(false);
        frame();
        const auto pipelines = canvas.stackPipelineCreations();
        canvas.setGpuStackEnabled(true);
        if (!document.newDocument(1200, 128, false, 16))
            return false;
        frame();
        frame();
        const auto uploads = canvas.stackTextureUploads();
        document.addAdjustmentLayer("invert");
        frame();
        const auto result = frame();
        if (!require(canvas.stackPipelineCreations() == pipelines + 1,
                     "three retained tiles share a compatible graphics pipeline"))
            return false;
        if (!require(canvas.gpuStackActive() && canvas.stackTextureUploads() == uploads + 1,
                     "three retained tiles share one lookup texture upload"))
            return false;
        const double ratio = double(result.width()) / window->width();
        for (const int x : {4, 511, 512, 1024, 1195}) {
            const auto point = canvas.mapToScene({(x + .5) * .5, 64.5}) * ratio;
            const auto pixel = result.pixelColor(qFloor(point.x()), qFloor(point.y()));
            if (!require(pixel.red() <= 2 && pixel.green() <= 2 && pixel.blue() <= 2,
                         "each tile sees the uploaded shared lookup table"))
                return false;
        }
    }
    qInfo() << "Layer stack checks passed; hardware:" << hardware;
    return true;
}
