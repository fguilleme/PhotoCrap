#pragma once
#include "CanvasItem.h"
#include "Document.h"
#include "filters/CurvesFilter.h"
#include "filters/LevelsFilter.h"
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QtTest>

inline bool runToneRenderSmoke(QQuickWindow *_window)
{
    auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Tone render check failed: %s", _message);
        return _ok;
    };
    const bool hardware =
        QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi());
    const auto *applicationCanvas = _window->findChild<CanvasItem *>("imageCanvas");
    if (!require(applicationCanvas && applicationCanvas->gpuTonePreviewEnabled() == hardware,
                 "the application enables GPU previews only on a hardware backend"))
        return false;
    Document   document;
    CanvasItem canvas(_window->contentItem());
    canvas.setZ(1000);
    canvas.setPosition({40, 120});
    canvas.setSize({512, 256});
    canvas.setViewportRect({0, 0, 512, 256});
    canvas.setDocument(&document);
    canvas.setGpuTonePreviewEnabled(hardware);
    document.setPreviewSize(512, 256);
    QTemporaryDir directory;
    const QString path = directory.filePath("tone.png");
    QImage        original(512, 256, QImage::Format_ARGB32);
    for (int y = 0; y < original.height(); ++y)
        for (int x = 0; x < original.width(); ++x)
            original.setPixel(x, y, qRgb(x % 256, y, (x + y) % 256));
    if (!require(original.save(path), "create an opaque RGB reference"))
        return false;
    const auto frame = [&] {
        _window->update();
        QTest::qWait(40);
        return _window->grabWindow();
    };
    const auto check = [&](const QImage &_frame, const QImage &_expected) {
        const qreal ratio = qreal(_frame.width()) / _window->width();
        for (int y = 20; y < 240; y += 37)
            for (int x = 10; x < 500; x += 13) {
                const QPointF scene = canvas.mapToScene({x + .5, y + .5});
                const QPoint  point(qFloor(scene.x() * ratio), qFloor(scene.y() * ratio));
                if (!_frame.rect().contains(point))
                    return false;
                const auto actual = _frame.pixelColor(point), expected = _expected.pixelColor(x, y);
                if (qAbs(actual.red() - expected.red()) > 2 ||
                    qAbs(actual.green() - expected.green()) > 2 ||
                    qAbs(actual.blue() - expected.blue()) > 2) {
                    qWarning() << "Tone pixel mismatch" << QPoint(x, y) << actual << expected;
                    _frame.save("/tmp/photocrap-tone-render-failure.png");
                    return false;
                }
            }
        return true;
    };
    for (const QString &type : {QStringLiteral("levels"), QStringLiteral("curves")}) {
        if (!require(document.openFile(QUrl::fromLocalFile(path)), "open reference photo"))
            return false;
        document.addAdjustmentLayer(type);
        frame();
        QVariantMap parameters;
        auto        preview = [&](double _value) {
            if (type == "levels") {
                parameters = {{"gamma", _value}};
                document.previewLayerParameter("gamma", _value);
            } else {
                const QVariantList points{
                    QVariantMap{{"x", 0}, {"y", 0}}, QVariantMap{{"x", 80}, {"y", 20 * _value}},
                    QVariantMap{{"x", 180}, {"y", 220}}, QVariantMap{{"x", 255}, {"y", 255}}};
                parameters.insert("points", points);
                document.previewLayerParameter("points", points);
            }
        };
        const int revision = document.revision();
        preview(.8);
        if (hardware) {
            if (!require(QTest::qWaitFor([&] { return document.gpuTonePreviewActive(); }, 5000),
                         "the hardware backend activates the GPU preview"))
                return false;
        } else if (!require(QTest::qWaitFor([&] { return document.revision() > revision; }, 5000),
                            "software backend retains the CPU preview"))
            return false;
        const auto   first    = frame();
        const QImage expected = type == "levels"
                                    ? PhotoCrap::Filters::applyLevels(original, parameters)
                                    : PhotoCrap::Filters::applyCurves(original, parameters);
        if (!require(check(first, expected), "GPU/CPU preview matches the reference pixels"))
            return false;
        if (hardware) {
            const auto uploads = canvas.imageTextureUploads();
            const auto tables  = canvas.toneTableUploads();
            for (const double value : {1.2, 2.1, .6}) {
                preview(value);
                const auto image = frame();
                const auto cpu   = type == "levels"
                                       ? PhotoCrap::Filters::applyLevels(original, parameters)
                                       : PhotoCrap::Filters::applyCurves(original, parameters);
                if (!require(
                        check(image, cpu) && canvas.imageTextureUploads() == uploads,
                        "changing tone parameters updates the LUT without uploading the photo"))
                    return false;
            }
            if (!require(canvas.toneTableUploads() >= tables + 3,
                         "each change uploads its small lookup table"))
                return false;
            if (type == "curves") {
                parameters.insert("graySample", QVariantList{120, 145, 110});
                document.previewLayerParameter("graySample", parameters.value("graySample"));
                if (!require(check(frame(), PhotoCrap::Filters::applyCurves(original, parameters)),
                             "per-channel curve calibration uses the same lookup table"))
                    return false;
            }
        }
        document.finishLayerPreview();
        if (!require(QTest::qWaitFor([&] { return !document.gpuTonePreviewActive(); }, 5000),
                     "the native CPU final render replaces the GPU preview"))
            return false;
        const auto finalExpected = type == "levels"
                                       ? PhotoCrap::Filters::applyLevels(original, parameters)
                                       : PhotoCrap::Filters::applyCurves(original, parameters);
        if (!require(check(frame(), finalExpected), "the final canvas matches the CPU reference"))
            return false;
    }
    qInfo() << "Tone render checks passed; hardware:" << hardware
            << "; API:" << _window->rendererInterface()->graphicsApi()
            << "; photo textures:" << canvas.imageTextureUploads()
            << "; LUT textures:" << canvas.toneTableUploads();
    return true;
}
