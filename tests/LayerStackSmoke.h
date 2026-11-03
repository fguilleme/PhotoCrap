#pragma once
#include "CanvasItem.h"
#include <QQuickWindow>
#include <QSGRendererInterface>
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
    qInfo() << "Layer stack checks passed; hardware:" << hardware;
    return true;
}
