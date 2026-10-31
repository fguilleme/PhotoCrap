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
#include "filters/ToneMappingTable.h"
#include "filters/VibranceFilter.h"
#include <QColorSpace>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

class GpuTonePreviewTest : public QObject
{
    Q_OBJECT
private slots:
    void cachedAdjustmentPreview_data()
    {
        QTest::addColumn<QString>("type");
        QTest::addColumn<int>("bits");
        for (const QString &type : PhotoCrap::Filters::gpuTonePreviewTypes())
            if (type != "curves" && type != "levels")
                for (int bits : {8, 16})
                    QTest::newRow(qPrintable(type + QString::number(bits))) << type << bits;
    }
    void cachedAdjustmentPreview()
    {
        QFETCH(QString, type);
        QFETCH(int, bits);
        Document doc;
        QVERIFY(doc.newDocument(512, 384, false, bits));
        doc.setColor(QColor(40, 110, 210));
        doc.fillWithColor(true);
        doc.setPreviewSize(256, 192);
        doc.setGpuTonePreviewEnabled(true);
        const auto source = doc.composite();
        doc.addAdjustmentLayer(type);
        const auto     before  = doc.composite();
        const int      history = doc.historyIndex();
        const QString  key     = type == "light"                ? "exposure"
                                 : type == "color"              ? "temperature"
                                 : type == "colorBalance"       ? "cyanRed"
                                 : type == "blackWhite"         ? "reds"
                                 : type == "photoFilter"        ? "density"
                                 : type == "channelMixer"       ? "redGreen"
                                 : type == "posterize"          ? "levels"
                                 : type == "threshold"          ? "level"
                                 : type == "colorLookup"        ? "strength"
                                 : type == "selectiveColor"     ? "redsCyanRed"
                                 : type == "brightnessContrast" ? "brightness"
                                 : type == "exposure"           ? "exposure"
                                 : type == "vibrance"           ? "vibrance"
                                 : type == "hueSaturation"      ? "hue"
                                 : type == "invert"             ? "_maskDensity"
                                                                : "shadowColor";
        const QVariant initial = type == "gradientMap"                   ? QVariant("#301050")
                                 : type == "light" || type == "exposure" ? QVariant(.8)
                                 : type == "posterize"                   ? QVariant(5)
                                                                         : QVariant(65.);
        QVariantMap    parameters;
        for (const auto &entry : doc.layers())
            if (entry.toMap().value("layerIndex").toInt() == doc.activeLayer())
                parameters = entry.toMap().value("parameters").toMap();
        parameters.insert(key, initial);
        doc.previewLayerParameter(key, initial);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewSettings().mode,
                 PhotoCrap::Filters::toneMappingSettings(type, parameters).mode);
        const auto        base = doc.gpuTonePreviewBase();
        const QVariantMap changes =
            type == "light"   ? QVariantMap{{"contrast", -40},
                                            {"highlights", -60},
                                            {"shadows", 35},
                                            {"whites", 25},
                                            {"blacks", -20}}
            : type == "color" ? QVariantMap{{"tint", -15}, {"vibrance", 60}, {"saturation", -35}}
            : type == "colorBalance" ? QVariantMap{{"magentaGreen", -15}, {"yellowBlue", 30}}
            : type == "blackWhite"
                ? QVariantMap{{"yellows", 80},         {"greens", 15},   {"cyans", 40},
                              {"blues", 50},           {"magentas", 25}, {"tintEnabled", true},
                              {"tintColor", "#ba7351"}}
            : type == "photoFilter"
                ? QVariantMap{{"color", "#4d7fc1"}, {"preserveLuminosity", false}}
            : type == "channelMixer"
                ? QVariantMap{{"blueRed", -25}, {"greenBlue", 35},   {"redConstant", -10},
                              {"monoBlue", 60}, {"monoConstant", 5}, {"monochrome", true}}
            : type == "posterize" ? QVariantMap{{"levels", 11}}
            : type == "threshold" ? QVariantMap{{"level", 100}}
            : type == "colorLookup"
                ? QVariantMap{{"preset", 4}, {"strength", 45}, {"preserveLuminosity", true}}
            : type == "selectiveColor"
                ? QVariantMap{{"redsCyanRed", -40},     {"yellowsMagentaGreen", 25},
                              {"greensYellowBlue", 20}, {"whitesBlack", 10},
                              {"neutralsCyanRed", 15},  {"blacksMagentaGreen", -10},
                              {"relative", false}}
            : type == "brightnessContrast" ? QVariantMap{{"contrast", -35}}
            : type == "exposure"           ? QVariantMap{{"offset", -.15}, {"gamma", 1.4}}
            : type == "vibrance"           ? QVariantMap{{"saturation", -25}}
            : type == "hueSaturation"      ? QVariantMap{{"saturation", 40}, {"lightness", -15}}
            : type == "invert"             ? QVariantMap{{"_maskDensity", 85}}
                                           : QVariantMap{{"highlightColor", "#c5e5b0"}};
        const auto reference = [&](const QImage &_source) {
            if (type == "light")
                return PhotoCrap::Filters::applyLight(_source, parameters);
            if (type == "color")
                return PhotoCrap::Filters::applyColor(_source, parameters);
            if (type == "colorBalance")
                return PhotoCrap::Filters::applyColorBalance(_source, parameters);
            if (type == "blackWhite")
                return PhotoCrap::Filters::applyBlackWhite(_source, parameters);
            if (type == "photoFilter")
                return PhotoCrap::Filters::applyPhotoFilter(_source, parameters);
            if (type == "channelMixer")
                return PhotoCrap::Filters::applyChannelMixer(_source, parameters);
            if (type == "posterize")
                return PhotoCrap::Filters::applyPosterize(_source, parameters);
            if (type == "threshold")
                return PhotoCrap::Filters::applyThreshold(_source, parameters);
            if (type == "colorLookup")
                return PhotoCrap::Filters::applyColorLookup(_source, parameters);
            if (type == "selectiveColor")
                return PhotoCrap::Filters::applySelectiveColor(_source, parameters);
            if (type == "brightnessContrast")
                return PhotoCrap::Filters::applyBrightnessContrast(_source, parameters);
            if (type == "exposure")
                return PhotoCrap::Filters::applyExposure(_source, parameters);
            if (type == "hueSaturation")
                return PhotoCrap::Filters::applyHueSaturation(_source, parameters);
            if (type == "invert")
                return PhotoCrap::Filters::applyInvert(_source, parameters);
            if (type == "vibrance")
                return PhotoCrap::Filters::applyVibrance(_source, parameters);
            return PhotoCrap::Filters::applyGradientMap(_source, parameters);
        };
        for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
            parameters.insert(it.key(), it.value());
            doc.previewLayerParameter(it.key(), it.value());
            QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), base.cacheKey());
            QCOMPARE(doc.gpuTonePreviewTable(),
                     PhotoCrap::Filters::toneMappingTable(type, parameters));
        }
        const auto expected = reference(base);
        QTRY_COMPARE_WITH_TIMEOUT(doc.displayComposite().pixelColor(100, 100),
                                  expected.pixelColor(100, 100), 5000);
        doc.finishLayerPreview();
        QCOMPARE(doc.historyIndex(), history + 1);
        QTRY_VERIFY_WITH_TIMEOUT(!doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.composite(), reference(source));
        QCOMPARE(doc.channelBits(), bits);
        doc.previewLayerParameter(key, type == "gradientMap" ? QVariant("#202830")
                                       : type == "invert"    ? QVariant(45.)
                                                             : QVariant(-20.));
        QVERIFY(doc.gpuTonePreviewActive());
        QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), base.cacheKey());
        doc.cancelLayerPreview();
        QVERIFY(!doc.gpuTonePreviewActive());
        QCOMPARE(doc.composite(), reference(source));
        doc.undo();
        QCOMPARE(doc.composite(), before);
    }
    void legacySelectiveColorUsesCpuFallback()
    {
        QTemporaryDir directory;
        const auto    url = QUrl::fromLocalFile(directory.filePath("legacy.pcr"));
        Document      seed;
        QVERIFY(seed.newDocument(128, 64, false));
        seed.setColor(QColor(60, 120, 180));
        seed.fillWithColor(true);
        const auto source = seed.composite();
        seed.addAdjustmentLayer("selectiveColor");
        QVERIFY(seed.saveProject(url));
        QFile file(url.toLocalFile());
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        auto        layers = root.value("layers").toArray();
        auto        layer  = layers.last().toObject();
        QVariantMap parameters{
            {"cyanRed", 15}, {"magentaGreen", -10}, {"yellowBlue", 20}, {"black", 5}};
        layer.insert("parameters", QJsonObject::fromVariantMap(parameters));
        layers.replace(layers.size() - 1, layer);
        root.insert("layers", layers);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(file.write(QJsonDocument(root).toJson()) > 0);
        file.close();
        Document doc;
        doc.setGpuTonePreviewEnabled(true);
        QVERIFY(doc.openFile(url));
        const int revision = doc.revision();
        parameters.insert("cyanRed", 45);
        doc.previewLayerParameter("cyanRed", 45);
        QTRY_VERIFY_WITH_TIMEOUT(doc.revision() > revision, 5000);
        QVERIFY(!doc.gpuTonePreviewActive());
        doc.finishLayerPreview();
        QTRY_COMPARE_WITH_TIMEOUT(doc.displayComposite(),
                                  PhotoCrap::Filters::applySelectiveColor(source, parameters),
                                  5000);
    }
    void hueAndInvertKnownValues()
    {
        QImage source(1, 1, QImage::Format_ARGB32);
        source.setPixelColor(0, 0, QColor(255, 0, 0));
        QCOMPARE(PhotoCrap::Filters::applyHueSaturation(source, {{"hue", 120}}).pixelColor(0, 0),
                 QColor(0, 255, 0));
        QCOMPARE(PhotoCrap::Filters::applyHueSaturation(source, {{"hue", -120}}).pixelColor(0, 0),
                 QColor(0, 0, 255));
        QCOMPARE(
            PhotoCrap::Filters::applyHueSaturation(source, {{"saturation", -100}}).pixelColor(0, 0),
            QColor(128, 128, 128));
        QCOMPARE(
            PhotoCrap::Filters::applyHueSaturation(source, {{"lightness", -100}}).pixelColor(0, 0),
            QColor(Qt::black));
        QCOMPARE(
            PhotoCrap::Filters::applyHueSaturation(source, {{"lightness", 100}}).pixelColor(0, 0),
            QColor(Qt::white));
        for (int gray : {0, 1, 40, 128, 254, 255}) {
            source.setPixelColor(0, 0, QColor(gray, gray, gray));
            QCOMPARE(
                PhotoCrap::Filters::applyHueSaturation(source, {{"hue", -30}}).pixelColor(0, 0),
                QColor(gray, gray, gray));
        }
        source.setPixelColor(0, 0, QColor(80, 100, 120, 170));
        QImage inverted(1, 1, QImage::Format_ARGB32);
        inverted.setPixelColor(0, 0, QColor(175, 155, 135, 170));
        inverted.setColorSpace(QColorSpace::SRgb);
        QCOMPARE(PhotoCrap::Filters::applyInvert(source, {}),
                 inverted.convertToFormat(QImage::Format_ARGB32_Premultiplied));
        QImage wide(1, 1, QImage::Format_RGBA64);
        reinterpret_cast<QRgba64 *>(wide.scanLine(0))[0] =
            QRgba64::fromRgba64(16465, 33019, 49544, 65535);
        QCOMPARE(PhotoCrap::Filters::applyInvert(wide, {}).pixelColor(0, 0).rgba64(),
                 QRgba64::fromRgba64(49070, 32516, 15991, 65535));
        // Preserve the former QColor conversion for arbitrary native 16-bit values and alpha.
        for (int alpha : {17000, 40000, 65535}) {
            const QColor original = QColor::fromRgba64(16465, 33019, 49544, alpha);
            wide.setPixelColor(0, 0, original);
            for (int hue : {-180, -37, 0, 91, 180}) {
                QColor expected = original;
                float  h, s, l, a;
                expected.getHslF(&h, &s, &l, &a);
                expected.setHslF(std::fmod(h + hue / 360. + 1., 1.), qBound(0., s * 1.3, 1.),
                                 qBound(0., l - .15, 1.), a);
                QImage reference(1, 1, QImage::Format_RGBA64);
                reference.setPixelColor(0, 0, expected);
                reference.setColorSpace(QColorSpace::SRgb);
                QCOMPARE(PhotoCrap::Filters::applyHueSaturation(
                             wide, {{"hue", hue}, {"saturation", 30}, {"lightness", -15}}),
                         reference.convertToFormat(QImage::Format_RGBA64_Premultiplied));
            }
        }
    }
    void legacyExposureAndVibranceSemantics()
    {
        QImage source(1, 1, QImage::Format_ARGB32);
        source.setPixelColor(0, 0, QColor(64, 128, 192));
        QCOMPARE(PhotoCrap::Filters::applyExposure(source, {{"exposure", 1.}}).pixelColor(0, 0),
                 QColor(128, 255, 255));
        source.setPixelColor(0, 0, QColor(80, 100, 120));
        QCOMPARE(
            PhotoCrap::Filters::applyVibrance(source, {{"vibrance", -100}, {"saturation", -100}})
                .pixelColor(0, 0),
            QColor(112, 95, 78));
        QImage wide(1, 1, QImage::Format_RGBA64);
        reinterpret_cast<QRgba64 *>(wide.scanLine(0))[0] =
            QRgba64::fromRgba64(16465, 33019, 49544, 65535);
        const auto exposed = PhotoCrap::Filters::applyExposure(wide, {{"exposure", 1.}});
        QCOMPARE(exposed.format(), QImage::Format_RGBA64_Premultiplied);
        QCOMPARE(reinterpret_cast<const QRgba64 *>(exposed.constScanLine(0))[0],
                 QRgba64::fromRgba64(32930, 65535, 65535, 65535));
    }
    void cachedBaseAndFinalRender()
    {
        Document doc;
        QVERIFY(doc.newDocument(512, 384, false));
        doc.setColor(QColor(50, 100, 150));
        doc.fillWithColor(true);
        doc.setPreviewSize(256, 192);
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("levels");
        const int history = doc.historyIndex();
        doc.previewLayerParameter("gamma", 2.);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        const auto key = doc.gpuTonePreviewBase().cacheKey();
        QCOMPARE(doc.gpuTonePreviewBase().size(), QSize(256, 192));
        QCOMPARE(doc.gpuTonePreviewBase().pixelColor(100, 100), QColor(50, 100, 150));
        QSignalSpy changed(&doc, &Document::gpuTonePreviewChanged);
        for (const double gamma : {.8, 1.2, 2.4}) {
            doc.previewLayerParameter("gamma", gamma);
            QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), key);
            QCOMPARE(doc.gpuTonePreviewTable(),
                     PhotoCrap::Filters::toneMappingTable("levels", {{"gamma", gamma}}));
        }
        QCOMPARE(changed.count(), 3);
        const auto expected =
            PhotoCrap::Filters::applyLevels(doc.gpuTonePreviewBase(), {{"gamma", 2.4}});
        QTRY_COMPARE_WITH_TIMEOUT(doc.displayComposite().pixelColor(100, 100),
                                  expected.pixelColor(100, 100), 5000);
        doc.finishLayerPreview();
        QCOMPARE(doc.historyIndex(), history + 1);
        QTRY_VERIFY_WITH_TIMEOUT(!doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.composite().pixelColor(100, 100), expected.pixelColor(100, 100));
        // Reuse the same lower stack for a second gesture; cancelling restores the first.
        doc.previewLayerParameter("gamma", .7);
        QVERIFY(doc.gpuTonePreviewActive());
        QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), key);
        doc.cancelLayerPreview();
        QVERIFY(!doc.gpuTonePreviewActive());
        QCOMPARE(doc.historyIndex(), history + 1);
        QTRY_COMPARE_WITH_TIMEOUT(doc.displayComposite().pixelColor(100, 100),
                                  expected.pixelColor(100, 100), 5000);
        doc.undo();
        QCOMPARE(doc.composite().pixelColor(100, 100), QColor(50, 100, 150));
    }
    void cancelledBaseCannotReturn()
    {
        Document doc;
        QVERIFY(doc.newDocument(2048, 1024, false));
        doc.setGpuTonePreviewEnabled(true);
        doc.setPreviewSize(512, 256);
        doc.addAdjustmentLayer("curves");
        const auto before = doc.composite();
        doc.previewLayerParameter("points", QVariantList{QVariantMap{{"x", 0}, {"y", 30}},
                                                         QVariantMap{{"x", 255}, {"y", 200}}});
        doc.cancelLayerPreview();
        QTest::qWait(200);
        QVERIFY(!doc.gpuTonePreviewActive());
        QCOMPARE(doc.composite(), before);
    }
    void uniformMaskAndOpacity_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<int>("coverage");
        QTest::addColumn<double>("opacity");
        for (int bits : {8, 16})
            for (int coverage : {0, 128, 255})
                for (double opacity : {0., .37, 1.})
                    QTest::newRow(
                        qPrintable(QString("%1-%2-%3").arg(bits).arg(coverage).arg(opacity)))
                        << bits << coverage << opacity;
    }
    void uniformMaskAndOpacity()
    {
        QFETCH(int, bits);
        QFETCH(int, coverage);
        QFETCH(double, opacity);
        Document doc, cpu;
        for (Document *target : {&doc, &cpu}) {
            QVERIFY(target->newDocument(256, 128, false, bits));
            target->setColor(QColor(40, 110, 210));
            target->fillWithColor(true);
            target->setPreviewSize(128, 64);
            target->addAdjustmentLayer("levels");
            target->setLayerOpacity(target->activeLayer(), opacity);
            target->setEditingMask(true);
            target->fillMaskWithColor(QColor(coverage, coverage, coverage));
            target->setEditingMask(false);
        }
        doc.setGpuTonePreviewEnabled(true);
        const auto before  = doc.composite();
        const int  history = doc.historyIndex();
        for (Document *target : {&doc, &cpu}) {
            target->previewLayerParameter("gamma", 2.);
            target->previewLayerParameter("_maskDensity", 37.);
            target->previewLayerParameter("_maskFeather", 12.);
        }
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewSettings().effectOpacity, float(opacity));
        QCOMPARE(doc.gpuTonePreviewSettings().maskCoverage,
                 float(qRound(255 - (255 - coverage) * .37)));
        const auto key   = doc.gpuTonePreviewBase().cacheKey();
        const auto table = doc.gpuTonePreviewTable().cacheKey();
        for (Document *target : {&doc, &cpu})
            target->previewLayerParameter("_maskDensity", 85.);
        QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), key);
        QCOMPARE(doc.gpuTonePreviewTable().cacheKey(), table);
        QCOMPARE(doc.gpuTonePreviewSettings().maskCoverage,
                 float(qRound(255 - (255 - coverage) * .85)));
        for (Document *target : {&doc, &cpu})
            target->finishLayerPreview();
        QTRY_VERIFY_WITH_TIMEOUT(!doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.historyIndex(), history + 1);
        QCOMPARE(doc.composite(), cpu.composite());
        QCOMPARE(doc.channelBits(), bits);
        doc.previewLayerParameter("_maskDensity", 15.);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        doc.cancelLayerPreview();
        QCOMPARE(doc.composite(), cpu.composite());
        doc.undo();
        QCOMPARE(doc.composite(), before);
        // An ignored mask must contribute full coverage, even if it is black.
        doc.setMaskEnabled(false);
        doc.previewLayerParameter("gamma", .7);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewSettings().maskCoverage, 255.f);
        doc.cancelLayerPreview();
    }
    void paintedMaskPreview_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8") << 8;
        QTest::newRow("16") << 16;
    }
    void paintedMaskPreview()
    {
        QFETCH(int, bits);
        Document doc, cpu;
        for (Document *target : {&doc, &cpu}) {
            QVERIFY(target->newDocument(512, 256, false, bits));
            target->setColor(QColor(40, 110, 210));
            target->fillWithColor(true);
            target->setPreviewSize(256, 128);
            target->addAdjustmentLayer("levels");
            target->setLayerOpacity(target->activeLayer(), .63);
            target->setEditingMask(true);
            target->setTool("brush");
            target->setBrushSize(120);
            target->setBrushHardness(.4);
            target->setColor(Qt::black);
            target->beginStroke(50, 80);
            target->continueStroke(450, 160);
            target->endStroke();
            target->setEditingMask(false);
        }
        doc.setGpuTonePreviewEnabled(true);
        const auto before  = doc.composite();
        const int  history = doc.historyIndex();
        for (Document *target : {&doc, &cpu}) {
            target->previewLayerParameter("gamma", 2.);
            target->previewLayerParameter("_maskDensity", 37.);
        }
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewMask().size(), QSize(256, 128));
        QCOMPARE(doc.gpuTonePreviewMask().format(), QImage::Format_Grayscale8);
        const auto baseKey  = doc.gpuTonePreviewBase().cacheKey();
        const auto maskKey  = doc.gpuTonePreviewMask().cacheKey();
        const auto tableKey = doc.gpuTonePreviewTable().cacheKey();
        doc.previewLayerParameter("_maskDensity", 85.);
        QCOMPARE(doc.gpuTonePreviewMask().cacheKey(), maskKey);
        QCOMPARE(doc.gpuTonePreviewTable().cacheKey(), tableKey);
        // Queue several feather requests before the worker returns: only the last wins.
        for (double feather : {3., 12., 5.})
            doc.previewLayerParameter("_maskFeather", feather);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewMask().cacheKey() != maskKey, 5000);
        QTest::qWait(150);
        QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), baseKey);
        QCOMPARE(doc.gpuTonePreviewTable().cacheKey(), tableKey);
        const auto stableMask = doc.gpuTonePreviewMask();
        for (double feather : {3., 12., 5.})
            doc.previewLayerParameter("_maskFeather", feather);
        QTest::qWait(150);
        QCOMPARE(doc.gpuTonePreviewMask(), stableMask);
        cpu.previewLayerParameter("_maskDensity", 85.);
        cpu.previewLayerParameter("_maskFeather", 5.);
        // Auxiliary previews must match the independently scheduled CPU path.
        QTRY_COMPARE_WITH_TIMEOUT(doc.displayComposite(), cpu.displayComposite(), 5000);
        for (Document *target : {&doc, &cpu})
            target->finishLayerPreview();
        QTRY_VERIFY_WITH_TIMEOUT(!doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.historyIndex(), history + 1);
        QCOMPARE(doc.composite(), cpu.composite());
        QCOMPARE(doc.channelBits(), bits);
        doc.previewLayerParameter("_maskFeather", 20.);
        doc.cancelLayerPreview();
        QTest::qWait(150);
        QVERIFY(!doc.gpuTonePreviewActive());
        QVERIFY(doc.gpuTonePreviewMask().isNull());
        QCOMPARE(doc.composite(), cpu.composite());
        doc.undo();
        QCOMPARE(doc.composite(), before);
        doc.previewLayerParameter("gamma", 2.);
        doc.previewLayerParameter("_maskFeather", 15.);
        doc.setPreviewSize(128, 64);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewBase().size(), QSize(128, 64));
        QCOMPARE(doc.gpuTonePreviewMask().size(), QSize(128, 64));
        doc.cancelLayerPreview();
    }
    void supportedBlendPreview_data()
    {
        QTest::addColumn<int>("blend");
        QTest::addColumn<int>("bits");
        QTest::addColumn<bool>("painted");
        for (int blend : PhotoCrap::Filters::gpuToneBlendModes())
            for (int bits : {8, 16})
                for (bool painted : {false, true})
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(blend).arg(bits).arg(painted)))
                        << blend << bits << painted;
    }
    void supportedBlendPreview()
    {
        QFETCH(int, blend);
        QFETCH(int, bits);
        QFETCH(bool, painted);
        Document doc, cpu;
        for (Document *target : {&doc, &cpu}) {
            QVERIFY(target->newDocument(512, 256, false, bits));
            target->setColor(QColor(40, 110, 210));
            target->fillWithColor(true);
            target->setPreviewSize(256, 128);
            target->addAdjustmentLayer("levels");
            if (painted) {
                target->setLayerOpacity(target->activeLayer(), .63);
                target->setEditingMask(true);
                target->setTool("brush");
                target->setBrushSize(120);
                target->setBrushHardness(.4);
                target->setColor(Qt::black);
                target->beginStroke(50, 80);
                target->continueStroke(450, 160);
                target->endStroke();
                target->setEditingMask(false);
            }
        }
        doc.setGpuTonePreviewEnabled(true);
        const auto before  = doc.composite();
        const int  history = doc.historyIndex();
        for (Document *target : {&doc, &cpu}) {
            target->previewLayerParameter("gamma", 1.7);
            target->previewLayerBlend(target->activeLayer(), blend);
        }
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewSettings().blendMode, blend);
        const auto base  = doc.gpuTonePreviewBase().cacheKey();
        const auto table = doc.gpuTonePreviewTable().cacheKey();
        const auto mask  = doc.gpuTonePreviewMask().cacheKey();
        const int  next  = blend == 3 ? 8 : 3;
        for (Document *target : {&doc, &cpu})
            target->previewLayerBlend(target->activeLayer(), next);
        QVERIFY(doc.gpuTonePreviewActive());
        QCOMPARE(doc.gpuTonePreviewSettings().blendMode, next);
        QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), base);
        QCOMPARE(doc.gpuTonePreviewTable().cacheKey(), table);
        QCOMPARE(doc.gpuTonePreviewMask().cacheKey(), mask);
        for (Document *target : {&doc, &cpu})
            target->finishLayerPreview();
        QTRY_VERIFY_WITH_TIMEOUT(!doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.composite(), cpu.composite());
        QCOMPARE(doc.channelBits(), bits);
        QCOMPARE(doc.historyIndex(), history + 1);
        doc.previewLayerBlend(doc.activeLayer(), blend);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        doc.cancelLayerPreview();
        QCOMPARE(doc.composite(), cpu.composite());
        doc.undo();
        QCOMPARE(doc.composite(), before);
    }
    void blendFallbackAndReturn()
    {
        Document doc;
        QVERIFY(doc.newDocument(256, 128, false));
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("levels");
        doc.previewLayerParameter("gamma", 2.);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        const auto base     = doc.gpuTonePreviewBase().cacheKey();
        const int  revision = doc.revision();
        doc.previewLayerBlend(doc.activeLayer(), 1);
        QVERIFY(!doc.gpuTonePreviewActive());
        QTRY_VERIFY_WITH_TIMEOUT(doc.revision() > revision, 5000);
        doc.previewLayerBlend(doc.activeLayer(), 3);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), base);
        QCOMPARE(doc.gpuTonePreviewSettings().blendMode, 3);
        QTest::qWait(150);
        QVERIFY(doc.gpuTonePreviewActive());
        doc.cancelLayerPreview();
    }
    void unsupportedCasesStayOnCpu()
    {
        for (const bool transparent : {false, true}) {
            Document doc;
            QVERIFY(doc.newDocument(256, 128, transparent));
            doc.setPreviewSize(256, 128);
            doc.setGpuTonePreviewEnabled(true);
            doc.addAdjustmentLayer("levels");
            if (!transparent)
                doc.setLayerBlend(doc.activeLayer(), 1);
            const int revision = doc.revision();
            doc.previewLayerParameter("gamma", 2.);
            QTRY_VERIFY_WITH_TIMEOUT(doc.revision() > revision, 5000);
            QVERIFY(!doc.gpuTonePreviewActive());
            doc.finishLayerPreview();
        }
        for (int blend = 0; blend <= 26; ++blend) {
            if (PhotoCrap::Filters::gpuToneBlendSupported("levels", blend, true))
                continue;
            Document doc;
            QVERIFY(doc.newDocument(256, 128, false));
            doc.setGpuTonePreviewEnabled(true);
            doc.addAdjustmentLayer("levels");
            doc.setLayerBlend(doc.activeLayer(), blend);
            const int revision = doc.revision();
            doc.previewLayerParameter("gamma", 2.);
            QTRY_VERIFY_WITH_TIMEOUT(doc.revision() > revision, 5000);
            QVERIFY(!doc.gpuTonePreviewActive());
            doc.cancelLayerPreview();
        }
        Document masked;
        QVERIFY(masked.newDocument(256, 128, false));
        masked.setGpuTonePreviewEnabled(true);
        masked.addAdjustmentLayer("curves");
        masked.addLayerMask(true);
        masked.setEditingMask(true);
        masked.setColor(Qt::black);
        masked.beginStroke(40, 40);
        masked.endStroke();
        masked.setEditingMask(false);
        const int revision = masked.revision();
        masked.previewLayerParameter("blackSample", QVariantList{20, 30, 40});
        QTRY_VERIFY_WITH_TIMEOUT(masked.revision() > revision, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(masked.gpuTonePreviewActive(), 5000);
        QVERIFY(!masked.gpuTonePreviewMask().isNull());
        masked.cancelLayerPreview();
    }
    void sixteenBitDocumentsKeepNativeFinal()
    {
        Document doc;
        QVERIFY(doc.newDocument(1024, 768, false, 16));
        doc.setColor(QColor(50, 100, 150));
        doc.fillWithColor(true);
        doc.setPreviewSize(256, 192);
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("levels");
        doc.previewLayerParameter("gamma", 2.);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewBase().format(), QImage::Format_ARGB32_Premultiplied);
        doc.finishLayerPreview();
        QTRY_VERIFY_WITH_TIMEOUT(!doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.channelBits(), 16);
        QCOMPARE(doc.composite().format(), QImage::Format_RGBA64_Premultiplied);
        doc.setGpuTonePreviewEnabled(false);
        const int revision = doc.revision();
        doc.previewLayerParameter("gamma", .7);
        QTRY_VERIFY_WITH_TIMEOUT(doc.revision() > revision, 5000);
        QVERIFY(!doc.gpuTonePreviewActive());
        doc.cancelLayerPreview();
    }
    void previewResolutionChangesDiscardOldBase()
    {
        Document doc;
        QVERIFY(doc.newDocument(1024, 768, false));
        doc.setPreviewSize(256, 192);
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("levels");
        doc.previewLayerParameter("gamma", 2.);
        // Resize before the first asynchronous base has returned.
        doc.setPreviewSize(512, 384);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewBase().size(), QSize(512, 384));
        const auto key = doc.gpuTonePreviewBase().cacheKey();
        doc.setPreviewSize(128, 96);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewBase().size(), QSize(128, 96));
        QVERIFY(doc.gpuTonePreviewBase().cacheKey() != key);
        doc.cancelLayerPreview();
    }
    void nativeSixteenBitPreviewStaysOnCpu()
    {
        Document doc;
        QVERIFY(doc.newDocument(128, 96, false, 16));
        doc.setPreviewSize(128, 96);
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("levels");
        const int revision = doc.revision();
        doc.previewLayerParameter("gamma", 2.);
        QTRY_VERIFY_WITH_TIMEOUT(doc.revision() > revision, 5000);
        QVERIFY(!doc.gpuTonePreviewActive());
        QCOMPARE(doc.displayComposite().format(), QImage::Format_RGBA64_Premultiplied);
        doc.cancelLayerPreview();
    }
};
QTEST_GUILESS_MAIN(GpuTonePreviewTest)
#include "GpuTonePreviewTest.moc"
