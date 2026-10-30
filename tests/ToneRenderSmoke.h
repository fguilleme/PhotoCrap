#pragma once
#include "CanvasItem.h"
#include "Document.h"
#include "filters/BlackWhiteFilter.h"
#include "filters/BrightnessContrastFilter.h"
#include "filters/ChannelMixerFilter.h"
#include "filters/ColorBalanceFilter.h"
#include "filters/ColorFilter.h"
#include "filters/ColorLookupFilter.h"
#include "filters/CurvesFilter.h"
#include "filters/ExposureFilter.h"
#include "filters/GradientMapFilter.h"
#include "filters/HueSaturationFilter.h"
#include "filters/InvertFilter.h"
#include "filters/LevelsFilter.h"
#include "filters/LightFilter.h"
#include "filters/PhotoFilterFilter.h"
#include "filters/PosterizeFilter.h"
#include "filters/SelectiveColorFilter.h"
#include "filters/ThresholdFilter.h"
#include "filters/VibranceFilter.h"
#include <QPainter>
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
        for (int x = 0; x < original.width(); ++x) {
            QRgb pixel = qRgb(x % 256, y, (x + y) % 256);
            if (y < 32)
                pixel = qRgb(x % 256, x % 256, x % 256);
            else if (y < 64)
                pixel = qRgb(x % 256, 0, 0);
            else if (y < 96)
                pixel = qRgb(0, x % 256, 0);
            else if (y < 128)
                pixel = qRgb(0, 0, x % 256);
            else if (y < 160)
                pixel = QColor::fromHsv(x % 360, 255, 180).rgb();
            else if (y < 192)
                pixel = QColor::fromHsv(x % 360, 80, 160).rgb();
            if (x == 0)
                pixel = qRgb(0, 0, 0);
            else if (x == 13)
                pixel = qRgb(255, 255, 255);
            original.setPixel(x, y, pixel);
        }
    if (!require(original.save(path), "create an opaque RGB reference"))
        return false;
    const auto frame = [&] {
        _window->update();
        QTest::qWait(40);
        return _window->grabWindow();
    };
    const auto check = [&](const QImage &_frame, const QImage &_expected, bool _dense = false) {
        const qreal ratio = qreal(_frame.width()) / _window->width();
        for (int y = 0; y < _expected.height(); y += 37)
            for (int x = 0; x < (_dense ? _expected.width() : 500); x += _dense ? 1 : 13) {
                const QPointF scene =
                    canvas.mapToScene({(x + .5) * canvas.width() / _expected.width(),
                                       (y + .5) * canvas.height() / _expected.height()});
                const QPoint point(qFloor(scene.x() * ratio), qFloor(scene.y() * ratio));
                if (!_frame.rect().contains(point)) {
                    qWarning() << "Tone sample outside frame" << point << _frame.size();
                    return false;
                }
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
    const auto composed = [&](const QImage &_filtered, double _opacity, int _mask,
                              double _density) {
        QImage    masked = _filtered.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        const int alpha  = qRound(255 - (255 - _mask) * _density / 100.);
        for (int y = 0; y < masked.height(); ++y) {
            auto *row = reinterpret_cast<QRgb *>(masked.scanLine(y));
            for (int x = 0; x < masked.width(); ++x)
                row[x] = qRgba(qRound(qRed(row[x]) * alpha / 255.),
                               qRound(qGreen(row[x]) * alpha / 255.),
                               qRound(qBlue(row[x]) * alpha / 255.), alpha);
        }
        QImage   result = original.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&result);
        painter.setOpacity(_opacity);
        painter.drawImage(0, 0, masked);
        painter.end();
        return result;
    };
    const auto paintMask = [](Document &_document) {
        _document.setEditingMask(true);
        _document.fillMaskWithColor(Qt::white);
        _document.setTool("brush");
        _document.setBrushSize(128);
        _document.setBrushHardness(.4);
        _document.setColor(Qt::black);
        _document.beginStroke(20, 80);
        _document.continueStroke(_document.width() - 20, 160);
        _document.endStroke();
        _document.setColor(QColor(128, 128, 128));
        _document.beginStroke(_document.width() / 2, 200);
        _document.endStroke();
        _document.setEditingMask(false);
    };
    const auto &types = PhotoCrap::Filters::gpuTonePreviewTypes();
    if (!require(types.size() == 18, "the render test covers the complete GPU filter registry"))
        return false;
    for (const QString &type : types) {
        qInfo() << "Checking GPU filter:" << type;
        if (!require(document.openFile(QUrl::fromLocalFile(path)), "open reference photo"))
            return false;
        document.addAdjustmentLayer(type);
        frame();
        QVariantMap parameters;
        const auto  reference = [&] {
            if (type == "levels")
                return PhotoCrap::Filters::applyLevels(original, parameters);
            if (type == "light")
                return PhotoCrap::Filters::applyLight(original, parameters);
            if (type == "color")
                return PhotoCrap::Filters::applyColor(original, parameters);
            if (type == "colorBalance")
                return PhotoCrap::Filters::applyColorBalance(original, parameters);
            if (type == "blackWhite")
                return PhotoCrap::Filters::applyBlackWhite(original, parameters);
            if (type == "gradientMap")
                return PhotoCrap::Filters::applyGradientMap(original, parameters);
            if (type == "photoFilter")
                return PhotoCrap::Filters::applyPhotoFilter(original, parameters);
            if (type == "channelMixer")
                return PhotoCrap::Filters::applyChannelMixer(original, parameters);
            if (type == "posterize")
                return PhotoCrap::Filters::applyPosterize(original, parameters);
            if (type == "threshold")
                return PhotoCrap::Filters::applyThreshold(original, parameters);
            if (type == "colorLookup")
                return PhotoCrap::Filters::applyColorLookup(original, parameters);
            if (type == "selectiveColor")
                return PhotoCrap::Filters::applySelectiveColor(original, parameters);
            if (type == "brightnessContrast")
                return PhotoCrap::Filters::applyBrightnessContrast(original, parameters);
            if (type == "exposure")
                return PhotoCrap::Filters::applyExposure(original, parameters);
            if (type == "hueSaturation")
                return PhotoCrap::Filters::applyHueSaturation(original, parameters);
            if (type == "invert")
                return PhotoCrap::Filters::applyInvert(original, parameters);
            if (type == "vibrance")
                return PhotoCrap::Filters::applyVibrance(original, parameters);
            return PhotoCrap::Filters::applyCurves(original, parameters);
        };
        auto preview = [&](double _value) {
            if (type == "levels") {
                parameters = {{"gamma", _value}};
                document.previewLayerParameter("gamma", _value);
            } else if (type == "light") {
                parameters = {{"exposure", (_value - 1.) * 1.2},   {"contrast", (_value - 1.) * 30},
                              {"highlights", (_value - 1.) * -70}, {"shadows", (_value - 1.) * 60},
                              {"whites", (_value - 1.) * 40},      {"blacks", (_value - 1.) * -40}};
                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                    document.previewLayerParameter(it.key(), it.value());
            } else if (type == "color" || type == "colorBalance") {
                parameters = type == "color" ? QVariantMap{{"temperature", (_value - 1.) * 60},
                                                           {"tint", (_value - 1.) * -40},
                                                           {"vibrance", (_value - 1.) * 70},
                                                           {"saturation", (_value - 1.) * -30}}
                                             : QVariantMap{{"cyanRed", (_value - 1.) * 60},
                                                           {"magentaGreen", (_value - 1.) * -40},
                                                           {"yellowBlue", (_value - 1.) * 30}};
                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                    document.previewLayerParameter(it.key(), it.value());
            } else if (type == "blackWhite") {
                parameters = {{"reds", 40 + (_value - 1.) * 60},
                              {"yellows", 60 - (_value - 1.) * 40},
                              {"greens", 40 + (_value - 1.) * 20},
                              {"cyans", 60 + (_value - 1.) * 50},
                              {"blues", 20 - (_value - 1.) * 30},
                              {"magentas", 80 + (_value - 1.) * 10},
                              {"tintEnabled", true},
                              {"tintColor", "#ba7351"}};
                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                    document.previewLayerParameter(it.key(), it.value());
            } else if (type == "gradientMap") {
                parameters = {
                    {"shadowColor", QColor::fromHsv(qRound(_value * 60) % 360, 180, 80).name()},
                    {"highlightColor",
                     QColor::fromHsv(qRound(210 + _value * 30) % 360, 120, 240).name()}};
                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                    document.previewLayerParameter(it.key(), it.value());
            } else if (type == "photoFilter") {
                parameters = {
                    {"color", "#ec8a00"}, {"density", _value * 30.}, {"preserveLuminosity", true}};
                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                    document.previewLayerParameter(it.key(), it.value());
            } else if (type == "channelMixer") {
                parameters = {{"redRed", 100 + (_value - 1.) * 40},
                              {"redGreen", (_value - 1.) * -60},
                              {"greenBlue", (_value - 1.) * 50},
                              {"blueRed", (_value - 1.) * -30},
                              {"redConstant", (_value - 1.) * 10}};
                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                    document.previewLayerParameter(it.key(), it.value());
            } else if (type == "posterize" || type == "threshold") {
                const QString key = type == "posterize" ? "levels" : "level";
                parameters        = {
                    {key, type == "posterize" ? qRound(_value * 10) : qRound(_value * 80)}};
                document.previewLayerParameter(key, parameters.value(key));
            } else if (type == "colorLookup") {
                parameters = {
                    {"preset", 5}, {"strength", _value * 40.}, {"preserveLuminosity", true}};
                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                    document.previewLayerParameter(it.key(), it.value());
            } else if (type == "selectiveColor") {
                parameters = PhotoCrap::Filters::defaultSelectiveColorParameters();
                parameters.insert("redsCyanRed", (_value - 1.) * 45.);
                parameters.insert("cyansMagentaGreen", (_value - 1.) * -55.);
                parameters.insert("neutralsYellowBlue", (_value - 1.) * 35.);
                parameters.insert("blacksBlack", (_value - 1.) * 15.);
                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                    document.previewLayerParameter(it.key(), it.value());
            } else if (type == "hueSaturation") {
                parameters = {{"hue", (_value - 1.) * 90},
                              {"saturation", (_value - 1.) * 60},
                              {"lightness", (_value - 1.) * -25}};
                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                    document.previewLayerParameter(it.key(), it.value());
            } else if (type == "invert") {
                parameters = {{"_maskDensity", _value * 30}};
                document.previewLayerParameter("_maskDensity", _value * 30);
            } else if (type == "brightnessContrast" || type == "exposure" || type == "vibrance") {
                if (type == "brightnessContrast")
                    parameters = {{"brightness", (_value - 1.) * 40},
                                  {"contrast", (_value - 1.) * -30}};
                else if (type == "exposure")
                    parameters = {{"exposure", (_value - 1.) * 1.5},
                                  {"offset", (_value - 1.) * .1},
                                  {"gamma", _value}};
                else
                    parameters = {{"vibrance", (_value - 1.) * 70},
                                  {"saturation", (_value - 1.) * -40}};
                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                    document.previewLayerParameter(it.key(), it.value());
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
        const QImage expected = reference();
        if (!require(check(first, expected), "GPU/CPU preview matches the reference pixels"))
            return false;
        if (hardware) {
            const auto uploads = canvas.imageTextureUploads();
            const auto tables  = canvas.toneTableUploads();
            for (const double value : {1.2, 2.1, .6}) {
                preview(value);
                const auto image = frame();
                const auto cpu   = reference();
                if (!require(check(image, cpu) && canvas.imageTextureUploads() == uploads,
                             "changing parameters updates the shader without uploading the photo"))
                    return false;
            }
            const bool stableTable =
                type == "blackWhite" || type == "color" || type == "photoFilter" ||
                type == "channelMixer" || type == "colorLookup" || type == "selectiveColor" ||
                type == "vibrance" || type == "hueSaturation" || type == "invert";
            if (!require(stableTable ? canvas.toneTableUploads() == tables
                                     : canvas.toneTableUploads() >= tables + 3,
                         "identical palettes are reused; changed mappings update their LUT"))
                return false;
            if (type == "color" || type == "colorBalance") {
                const QStringList controls =
                    type == "color" ? QStringList{"temperature", "tint", "vibrance", "saturation"}
                                    : QStringList{"cyanRed", "magentaGreen", "yellowBlue"};
                for (const QString &key : controls)
                    for (double endpoint : {-100., 100.}) {
                        parameters = {{key, endpoint}};
                        for (const QString &control : controls)
                            document.previewLayerParameter(control, parameters.value(control, 0.));
                        if (!require(check(frame(), reference()) &&
                                         canvas.imageTextureUploads() == uploads,
                                     "each color control matches the CPU at both endpoints"))
                            return false;
                    }
            }
            if (type == "light") {
                for (const QString &key : {QStringLiteral("exposure"), QStringLiteral("contrast"),
                                           QStringLiteral("highlights"), QStringLiteral("shadows"),
                                           QStringLiteral("whites"), QStringLiteral("blacks")}) {
                    for (double endpoint : {-1., 1.}) {
                        parameters = {{key, endpoint * (key == "exposure" ? 5. : 100.)}};
                        for (const QString &control :
                             {QStringLiteral("exposure"), QStringLiteral("contrast"),
                              QStringLiteral("highlights"), QStringLiteral("shadows"),
                              QStringLiteral("whites"), QStringLiteral("blacks")})
                            document.previewLayerParameter(control, parameters.value(control, 0.));
                        if (!require(check(frame(), reference()) &&
                                         canvas.imageTextureUploads() == uploads,
                                     "each Light control matches the CPU at both endpoints"))
                            return false;
                    }
                }
            }
            if (type == "blackWhite") {
                const QVariantMap defaults{{"reds", 40},  {"yellows", 60}, {"greens", 40},
                                           {"cyans", 60}, {"blues", 20},   {"magentas", 80}};
                for (auto it = defaults.cbegin(); it != defaults.cend(); ++it)
                    for (double endpoint : {-200., 300.}) {
                        parameters = defaults;
                        parameters.insert(it.key(), endpoint);
                        parameters.insert("tintEnabled", true);
                        parameters.insert("tintColor", "#5375bb");
                        for (auto entry = parameters.cbegin(); entry != parameters.cend(); ++entry)
                            document.previewLayerParameter(entry.key(), entry.value());
                        if (!require(
                                check(frame(), reference()) &&
                                    canvas.imageTextureUploads() == uploads,
                                "each Black and White family matches the CPU at both endpoints"))
                            return false;
                    }
                parameters.insert("tintEnabled", false);
                document.previewLayerParameter("tintEnabled", false);
                if (!require(check(frame(), reference()),
                             "disabling tint restores neutral grayscale"))
                    return false;
                for (const QString &tint : {QStringLiteral("#000000"), QStringLiteral("#ffffff"),
                                            QStringLiteral("invalid")}) {
                    parameters.insert("tintEnabled", true);
                    parameters.insert("tintColor", tint);
                    document.previewLayerParameter("tintEnabled", true);
                    document.previewLayerParameter("tintColor", tint);
                    if (!require(check(frame(), reference()),
                                 "tint palettes match QColor including invalid-color fallback"))
                        return false;
                }
            }
            if (type == "gradientMap") {
                for (const QString &shadow : {QStringLiteral("#000000"), QStringLiteral("#ffffff"),
                                              QStringLiteral("#ff00ff")}) {
                    parameters = {{"shadowColor", shadow}, {"highlightColor", "#00ff00"}};
                    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                        document.previewLayerParameter(it.key(), it.value());
                    if (!require(check(frame(), reference()) &&
                                     canvas.imageTextureUploads() == uploads,
                                 "gradient palettes use the CPU's rounded luminance"))
                        return false;
                }
            }
            if (type == "photoFilter") {
                for (const QString &color : {QStringLiteral("#ec8a00"), QStringLiteral("#00b5ff"),
                                             QStringLiteral("#000000"), QStringLiteral("#ffffff"),
                                             QStringLiteral("invalid")})
                    for (bool preserve : {false, true})
                        for (double density : {0., 25., 100.}) {
                            parameters = {{"color", color},
                                          {"density", density},
                                          {"preserveLuminosity", preserve},
                                          {"hue", 220.}};
                            for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                                document.previewLayerParameter(it.key(), it.value());
                            if (!require(check(frame(), reference()) &&
                                             canvas.imageTextureUploads() == uploads &&
                                             canvas.toneTableUploads() == tables,
                                         "Photo Filter matches CPU density, luminosity and legacy "
                                         "hue fallback"))
                                return false;
                        }
            }
            if (type == "channelMixer") {
                const QVariantMap defaults{
                    {"redRed", 100}, {"redGreen", 0},     {"redBlue", 0},    {"redConstant", 0},
                    {"greenRed", 0}, {"greenGreen", 100}, {"greenBlue", 0},  {"greenConstant", 0},
                    {"blueRed", 0},  {"blueGreen", 0},    {"blueBlue", 100}, {"blueConstant", 0},
                    {"monoRed", 40}, {"monoGreen", 40},   {"monoBlue", 20},  {"monoConstant", 0}};
                for (auto control = defaults.cbegin(); control != defaults.cend(); ++control)
                    for (double endpoint : {-200., 200.}) {
                        parameters = defaults;
                        parameters.insert(control.key(), endpoint);
                        parameters.insert("monochrome", control.key().startsWith("mono"));
                        for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                            document.previewLayerParameter(it.key(), it.value());
                        if (!require(check(frame(), reference()) &&
                                         canvas.imageTextureUploads() == uploads &&
                                         canvas.toneTableUploads() == tables,
                                     "each mixer coefficient and constant matches the CPU in RGB "
                                     "and monochrome"))
                            return false;
                    }
            }
            if (type == "posterize" || type == "threshold") {
                const QString    key = type == "posterize" ? "levels" : "level";
                const QList<int> controls =
                    type == "posterize"
                        ? QList<int>{2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
                                     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32}
                        : QList<int>{0, 1, 2, 64, 127, 128, 129, 192, 253, 254, 255};
                for (int value : controls) {
                    parameters = {{key, value}};
                    document.previewLayerParameter(key, value);
                    if (!require(check(frame(), reference(), true) &&
                                     canvas.imageTextureUploads() == uploads,
                                 "posterization plateaus and threshold transitions match the CPU"))
                        return false;
                }
            }
            if (type == "colorLookup") {
                for (int preset = 0; preset < 6; ++preset)
                    for (double strength : {0., 35., 100.})
                        for (bool preserve : {false, true}) {
                            parameters = {{"preset", preset},
                                          {"strength", strength},
                                          {"preserveLuminosity", preserve}};
                            for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                                document.previewLayerParameter(it.key(), it.value());
                            if (!require(check(frame(), reference()) &&
                                             canvas.imageTextureUploads() == uploads &&
                                             canvas.toneTableUploads() == tables,
                                         "all six color styles match the CPU with strength and "
                                         "luminosity options"))
                                return false;
                        }
            }
            if (type == "selectiveColor") {
                for (const char *range : PhotoCrap::Filters::SelectiveColorRangeNames)
                    for (const char *component : {"CyanRed", "MagentaGreen", "YellowBlue", "Black"})
                        for (double endpoint : {-100., 100.})
                            for (bool relative : {false, true}) {
                                parameters = PhotoCrap::Filters::defaultSelectiveColorParameters();
                                parameters.insert("relative", relative);
                                parameters.insert(QLatin1String(range) + QLatin1String(component),
                                                  endpoint);
                                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                                    document.previewLayerParameter(it.key(), it.value());
                                if (!require(check(frame(), reference()) &&
                                                 canvas.imageTextureUploads() == uploads &&
                                                 canvas.toneTableUploads() == tables,
                                             "all nine selective families and four controls match "
                                             "in relative and absolute modes"))
                                    return false;
                            }
            }
            if (type == "brightnessContrast" || type == "vibrance") {
                for (double first : {-100., 0., 100.})
                    for (double second : {-100., 0., 100.}) {
                        parameters = type == "brightnessContrast"
                                         ? QVariantMap{{"brightness", first}, {"contrast", second}}
                                         : QVariantMap{{"vibrance", first}, {"saturation", second}};
                        for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                            document.previewLayerParameter(it.key(), it.value());
                        if (!require(check(frame(), reference(), true) &&
                                         canvas.imageTextureUploads() == uploads,
                                     "legacy brightness and vibrance keep endpoint and "
                                     "negative-factor semantics"))
                            return false;
                    }
            }
            if (type == "exposure") {
                for (double exposure : {-5., 0., 5.})
                    for (double offset : {-1., 0., 1.})
                        for (double gamma : {.1, 1., 3.}) {
                            parameters = {
                                {"exposure", exposure}, {"offset", offset}, {"gamma", gamma}};
                            for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                                document.previewLayerParameter(it.key(), it.value());
                            if (!require(check(frame(), reference(), true) &&
                                             canvas.imageTextureUploads() == uploads,
                                         "legacy exposure EV, offset and gamma match the CPU at "
                                         "their limits"))
                                return false;
                        }
            }
            if (type == "hueSaturation") {
                for (double hue : {-180., -120., -60., -.5, 0., .5, 60., 120., 180.})
                    for (double saturation : {-100., 0., 100.})
                        for (double lightness : {-100., -35., 0., 35., 100.}) {
                            parameters = {
                                {"hue", hue}, {"saturation", saturation}, {"lightness", lightness}};
                            for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                                document.previewLayerParameter(it.key(), it.value());
                            if (!require(check(frame(), reference(), true) &&
                                             canvas.imageTextureUploads() == uploads &&
                                             canvas.toneTableUploads() == tables,
                                         "HSL hue wrap, saturation and lightness match the CPU "
                                         "without texture uploads"))
                                return false;
                        }
            }
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
        const auto finalExpected = reference();
        if (!require(check(frame(), finalExpected), "the final canvas matches the CPU reference"))
            return false;
        if (hardware) {
            // Every shader must compose with reduced opacity and uniform masks.
            const QList<double> opacities =
                type == "levels" ? QList<double>{0., .01, .37, .5, .99, 1.} : QList<double>{.37};
            for (double opacity : opacities)
                for (int mask : {0, 128, 255}) {
                    document.setLayerOpacity(document.activeLayer(), opacity);
                    document.setEditingMask(true);
                    document.fillMaskWithColor(QColor(mask, mask, mask));
                    document.setEditingMask(false);
                    preview(.8);
                    document.previewLayerParameter("_maskDensity", 0.);
                    document.previewLayerParameter("_maskFeather", 12.);
                    if (!require(
                            QTest::qWaitFor([&] { return document.gpuTonePreviewActive(); }, 5000),
                            "uniform masks and reduced opacity keep GPU eligibility"))
                        return false;
                    frame();
                    const auto photoUploads = canvas.imageTextureUploads();
                    const auto tableUploads = canvas.toneTableUploads();
                    const auto baseKey      = document.gpuTonePreviewBase().cacheKey();
                    for (double density : {0., 37., 100.}) {
                        document.previewLayerParameter("_maskDensity", density);
                        if (!require(
                                check(frame(), composed(reference(), opacity, mask, density),
                                      true) &&
                                    canvas.imageTextureUploads() == photoUploads &&
                                    canvas.toneTableUploads() == tableUploads &&
                                    document.gpuTonePreviewBase().cacheKey() == baseKey,
                                "mask density composes like the CPU without uploading textures"))
                            return false;
                    }
                    document.finishLayerPreview();
                    if (!require(
                            QTest::qWaitFor([&] { return !document.gpuTonePreviewActive(); },
                                            5000) &&
                                check(frame(), composed(reference(), opacity, mask, 100.), true),
                            "masked final rendering replaces the shader with the CPU result"))
                        return false;
                }
            document.setLayerOpacity(document.activeLayer(), type == "levels" ? 1. : .63);
            paintMask(document);
            preview(.8);
            document.previewLayerParameter("_maskFeather", 0.);
            document.previewLayerParameter("_maskDensity", 37.);
            if (!require(QTest::qWaitFor([&] { return document.gpuTonePreviewActive(); }, 5000) &&
                             !document.gpuTonePreviewMask().isNull(),
                         "painted masks keep GPU eligibility"))
                return false;
            frame();
            const auto photoUploads = canvas.imageTextureUploads();
            const auto tableUploads = canvas.toneTableUploads();
            const auto maskUploads  = canvas.maskTextureUploads();
            const auto baseKey      = document.gpuTonePreviewBase().cacheKey();
            for (double density : {0., 37., 100.}) {
                document.previewLayerParameter("_maskDensity", density);
                const auto expected = document.composite();
                if (!require(check(frame(), expected, true) &&
                                 canvas.imageTextureUploads() == photoUploads &&
                                 canvas.toneTableUploads() == tableUploads &&
                                 canvas.maskTextureUploads() == maskUploads,
                             "painted mask density matches the CPU without uploading any textures"))
                    return false;
            }
            for (double feather : {2., 8.}) {
                const auto previous = document.gpuTonePreviewMask().cacheKey();
                document.previewLayerParameter("_maskFeather", feather);
                if (!require(
                        QTest::qWaitFor(
                            [&] { return document.gpuTonePreviewMask().cacheKey() != previous; },
                            5000),
                        "feathered mask arrives asynchronously"))
                    return false;
                const auto expected = document.composite();
                if (!require(check(frame(), expected, true) &&
                                 document.gpuTonePreviewBase().cacheKey() == baseKey &&
                                 canvas.imageTextureUploads() == photoUploads &&
                                 canvas.toneTableUploads() == tableUploads,
                             "feathering uploads only the mask and keeps the photo"))
                    return false;
            }
            if (type == "levels") {
                const auto stableMask = document.gpuTonePreviewMask();
                for (double feather : {2., 4., 8.})
                    document.previewLayerParameter("_maskFeather", feather);
                QTest::qWait(150);
                if (!require(document.gpuTonePreviewMask() == stableMask,
                             "returning to a cached feather rejects an older running result"))
                    return false;
            }
            const auto blendPhoto = canvas.imageTextureUploads();
            const auto blendTable = canvas.toneTableUploads();
            const auto blendMask  = canvas.maskTextureUploads();
            for (int blend : PhotoCrap::Filters::gpuToneBlendModes()) {
                document.previewLayerBlend(document.activeLayer(), blend);
                const auto blended = document.composite();
                if (!require(document.gpuTonePreviewActive() && check(frame(), blended, true) &&
                                 canvas.imageTextureUploads() == blendPhoto &&
                                 canvas.toneTableUploads() == blendTable &&
                                 canvas.maskTextureUploads() == blendMask,
                             "supported RGB blends match the CPU without uploading textures")) {
                    qWarning() << "Failed blend/filter:" << blend << type;
                    return false;
                }
            }
            const auto expected = document.composite();
            document.finishLayerPreview();
            if (!require(QTest::qWaitFor([&] { return !document.gpuTonePreviewActive(); }, 5000) &&
                             check(frame(), expected, true),
                         "painted mask final rendering matches the CPU"))
                return false;
        }
    }
    if (hardware) {
        // Mask/photo tile padding must agree at 1024 pixels and across display LODs.
        QImage wide(1280, 256, QImage::Format_ARGB32);
        wide.fill(QColor(40, 110, 210));
        const auto widePath = directory.filePath("wide.png");
        if (!require(wide.save(widePath) && document.openFile(QUrl::fromLocalFile(widePath)),
                     "open tiled mask reference"))
            return false;
        const qreal ratio = _window->effectiveDevicePixelRatio();
        _window->setMinimumWidth(qCeil(1280 / ratio + 80));
        canvas.setSize({1280 / ratio, 256 / ratio});
        canvas.setViewportRect(canvas.boundingRect());
        document.setPreviewSize(1280, 256);
        document.addAdjustmentLayer("levels");
        paintMask(document);
        document.previewLayerParameter("gamma", 2.);
        if (!require(QTest::qWaitFor([&] { return document.gpuTonePreviewActive(); }, 5000),
                     "tiled mask preview starts"))
            return false;
        const auto expected = document.composite();
        if (!require(check(frame(), expected, true), "mask padding matches across adjacent tiles"))
            return false;
        const auto uploaded = canvas.maskTextureUploads();
        canvas.setViewportRect({1100 / ratio, 0, 180 / ratio, 256 / ratio});
        frame();
        canvas.setViewportRect(canvas.boundingRect());
        if (!require(check(frame(), expected, true) && canvas.maskTextureUploads() > uploaded,
                     "evicted mask tiles reload with the photo"))
            return false;
        canvas.setSize({0, 0});
        frame();
        canvas.setSize({1280 / ratio, 256 / ratio});
        canvas.setViewportRect(canvas.boundingRect());
        if (!require(check(frame(), expected, true), "mask survives an empty canvas geometry"))
            return false;
        canvas.setSize({320 / ratio, 64 / ratio});
        canvas.setViewportRect(canvas.boundingRect());
        const QSize displaySize(qRound(canvas.width() * _window->effectiveDevicePixelRatio()),
                                qRound(canvas.height() * _window->effectiveDevicePixelRatio()));
        if (!require(
                check(frame(),
                      expected.scaled(displaySize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                      true),
                "mask coordinates follow the photo at a reduced display level"))
            return false;
        document.cancelLayerPreview();
    }
    qInfo() << "Tone render checks passed; filters:" << types.size()
            << "; blend modes:" << PhotoCrap::Filters::gpuToneBlendModes().size()
            << "; hardware:" << hardware << "; API:" << _window->rendererInterface()->graphicsApi()
            << "; photo textures:" << canvas.imageTextureUploads()
            << "; LUT textures:" << canvas.toneTableUploads()
            << "; mask textures:" << canvas.maskTextureUploads();
    return true;
}
