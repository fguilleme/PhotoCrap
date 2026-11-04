#pragma once
#include "CanvasItem.h"
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QtTest>

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
        for (int mode = 0; mode < 23; ++mode) {
            if (mode == 1 || mode == 18)
                continue;
            document.setLayerBlend(document.activeLayer(), mode);
            frame();
            const auto result   = frame();
            const auto expected = document.composite();
            if (!require(!result.isNull() && canvas.gpuStackActive() == hardware,
                         "hardware stack or software fallback is active"))
                return false;
            const double ratio = double(result.width()) / window->width();
            for (int x : {10, 255, 511, 512, 1023, 1024, 1190}) {
                const QPointF point = canvas.mapToScene({(x + .5) * .5, 64.5});
                const QColor  actual =
                    result.pixelColor(qFloor(point.x() * ratio), qFloor(point.y() * ratio));
                const QColor reference = expected.pixelColor(x, 64);
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
            document.setLayerBlend(document.activeLayer(), 0);
            frame();
            frame();
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
            if (canvas.stackTextureUploads() <= before || canvas.stackTextureUploads() - before > 2)
                qWarning() << "Regional stack uploads" << bits << before
                           << canvas.stackTextureUploads() << canvas.gpuStackActive();
            if (!require(canvas.stackTextureUploads() > before &&
                             canvas.stackTextureUploads() - before <= 2,
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
        document.addAdjustmentLayer("noise");
        frame();
        frame();
        if (!require(!canvas.gpuStackActive(),
                     "unsupported adjustment switches the complete stack to CPU"))
            return false;
    }
    for (int bits : {8, 16}) {
        if (!document.newDocument(1200, 128, false, bits))
            return false;
        document.addAdjustmentLayer("noise");
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
            const int affected = 2;
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
    qInfo() << "Layer stack checks passed; hardware:" << hardware;
    return true;
}
