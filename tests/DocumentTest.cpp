#include "Document.h"
#include "DocumentStorage.h"
#include "ImageUtils.h"
#include "RawTestImage.h"
#include "RecentFilesManager.h"
#include "ShortcutManager.h"
#include "WorkspaceSettings.h"
#include "filters/BlackWhiteFilter.h"
#include "filters/BrightnessContrastFilter.h"
#include "filters/ChannelMixerFilter.h"
#include "filters/ColorFilter.h"
#include "filters/ColorLookupFilter.h"
#include "filters/CurvesFilter.h"
#include "filters/GradientMapFilter.h"
#include "filters/LevelsFilter.h"
#include "filters/LightFilter.h"
#include "filters/PhotoFilterFilter.h"
#include "filters/PosterizeFilter.h"
#include "filters/SelectiveColorFilter.h"
#include "filters/ThresholdFilter.h"
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSettings>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>
#include <limits>

class DocumentTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // Match the smallest GitHub runners and catch nested global-pool waits.
        QThreadPool::globalInstance()->setMaxThreadCount(2);
    }

    void layerStorageDistinguishesColorPixelsSourcesAndMasks()
    {
        constexpr qint64 pixels = 6000LL * 4000;
        QCOMPARE(DocumentStorage::estimateBytes(6000, 4000, 3, 1, 16), pixels * 25);
        QCOMPARE(DocumentStorage::estimateBytes(6000, 4000, 3, 1, 8), pixels * 13);
        // Fifty adjustments hold masks, not fifty full RGBA64 images.
        QVERIFY(DocumentStorage::estimateBytes(6000, 4000, 1, 50, 16) <
                DocumentStorage::MaxLayerBytes);
        QCOMPARE(DocumentStorage::estimateBytes(3, 2, 0, 1, 16), qint64(8));
        QCOMPARE(DocumentStorage::estimateBytes(8192, 4096, 8, 0, 16),
                 DocumentStorage::MaxLayerBytes);
        QVERIFY(DocumentStorage::estimateBytes(8192, 4096, 8, 1, 16) >
                DocumentStorage::MaxLayerBytes);
    }
    void largeSixteenBitFrequencyLayersCanBeDuplicatedAndMasked()
    {
        Document   d;
        QSignalSpy errors(&d, &Document::errorOccurred);
        QVERIFY(d.newDocument(6000, 4000, true, 16));
        d.addFrequencySeparation(4);
        QCOMPARE(d.layers().size(), 3);
        d.duplicateLayer();
        QCOMPARE(d.layers().size(), 4);
        d.addLayerMask();
        QVERIFY(d.layers().first().toMap().value("hasMask").toBool());
        QVERIFY(errors.isEmpty());
        d.undo();
        QVERIFY(!d.layers().first().toMap().value("hasMask").toBool());
        d.undo();
        QCOMPARE(d.layers().size(), 3);
        QCOMPARE(d.channelBits(), 16);
    }
    void layerStorageLimitRejectsBeforeChangingDocument()
    {
        Document d;
        QVERIFY(d.newDocument(8192, 4096, true, 16));
        for (int i = 1; i < 8; ++i)
            d.duplicateLayer();
        QCOMPARE(d.layers().size(), 8);
        const auto history = d.history();
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.duplicateLayer();
        QCOMPARE(d.layers().size(), 8);
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.last().first().toString().contains("2304"));
        QVERIFY(errors.last().first().toString().contains("2048"));
        d.addLayerMask();
        QVERIFY(d.layers().first().toMap().value("hasMask").toBool());
        QCOMPARE(errors.size(), 1); // A uniform mask needs no full-sized plane.
        const auto compactHistory = d.history();
        d.setTool("brush");
        d.setColor(Qt::black);
        d.beginStroke(10, 10);
        d.endStroke();
        QCOMPARE(errors.size(), 2); // Materializing it must respect the budget.
        QCOMPARE(d.history(), compactHistory);
        QCOMPARE(d.memoryUsage()["compactPlanes"].toInt(), 1);
        d.undo();
        QCOMPARE(d.history().mid(0, history.size()), history);
        // Replacing the large document must not inherit its storage budget.
        QVERIFY(d.newDocument(8, 8));
        d.addLayer();
        QCOMPARE(d.layers().size(), 2);
    }
    void projectStoragePreflightCountsPlanesAndPreservesOpenDocument()
    {
        QTemporaryDir dir;
        Document      d;
        QVERIFY(d.newDocument(8, 8));
        const auto original = d.composite();
        QSignalSpy errors(&d, &Document::errorOccurred);
        auto       writeProject = [&](const QJsonArray &_layers) {
            QFile file(dir.filePath("budget.pcr"));
            if (!file.open(QIODevice::WriteOnly))
                return QUrl{};
            const QJsonObject root{{"format", "PhotoCrap"}, {"version", 6},
                                   {"width", 8192},         {"height", 4096},
                                   {"channelBits", 16},     {"layers", _layers}};
            file.write(QJsonDocument(root).toJson());
            return QUrl::fromLocalFile(file.fileName());
        };
        QJsonArray pixels;
        for (int i = 0; i < 9; ++i)
            pixels.append(QJsonObject{{"kind", "pixel"}, {"png", "not-a-png"}});
        QVERIFY(!d.openFile(writeProject(pixels)));
        QCOMPARE(errors.size(), 1);
        // Storage rejection precedes decoding the deliberately invalid images.
        QVERIFY(errors.last().first().toString().contains("2304"));
        QCOMPARE(d.composite(), original);
        QCOMPARE(d.channelBits(), 8);
        QJsonArray groups;
        for (int i = 0; i < 512; ++i)
            groups.append(QJsonObject{
                {"kind", "group"}, {"opacity", 1}, {"blend", 0}, {"groupId", QString::number(i)}});
        QVERIFY(d.openFile(writeProject(groups)));
        QCOMPARE(d.layers().size(), 512);
        QVERIFY(d.setChannelBits(8));
        QVERIFY(d.setChannelBits(16));
        d.addLayer();
        QCOMPARE(d.layers().size(), 512);
        QVERIFY(errors.last().first().toString().contains("513"));
    }

    void hundredsOfLargeLayersStayCompactAfterReload()
    {
        QTemporaryDir dir;
        Document      d;
        QSignalSpy    errors(&d, &Document::errorOccurred);
        QVERIFY(d.newDocument(6000, 4000, true, 16));
        const qint64  baseBytes = d.memoryUsage()["layerBytes"].toLongLong();
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < 150; ++i) {
            d.addLayer();
            d.addLayerMask();
        }
        for (int i = 0; i < 150; ++i) {
            d.addAdjustmentLayer("light");
            d.setLayerVisible(d.activeLayer(), false);
        }
        QCOMPARE(d.layers().size(), 301);
        QCOMPARE(d.memoryUsage()["compactPlanes"].toInt(), 450);
        QVERIFY(d.memoryUsage()["layerBytes"].toLongLong() < baseBytes + 16 * 1024);
        QCOMPARE(d.memoryUsage()["reservedLayerBytes"].toLongLong(), baseBytes);
        QCOMPARE(d.memoryUsage()["historyBytes"].toLongLong(), qint64(0));
        QVERIFY(errors.isEmpty());
        qInfo() << "301 layers / 24 MP / 16 bit: creation" << timer.elapsed() << "ms, stored bytes"
                << d.memoryUsage()["layerBytes"];
        const auto project = QUrl::fromLocalFile(dir.filePath("many.pcr"));
        QVERIFY(d.saveProject(project));
        QVERIFY(QFileInfo(project.toLocalFile()).size() < 2 * 1024 * 1024);
        Document loaded;
        QVERIFY(loaded.openFile(project));
        QCOMPARE(loaded.layers().size(), 301);
        QCOMPARE(loaded.channelBits(), 16);
        QCOMPARE(loaded.memoryUsage()["compactPlanes"].toInt(), 450);
        QVERIFY(loaded.memoryUsage()["layerBytes"].toLongLong() < baseBytes + 16 * 1024);
        QCOMPARE(loaded.maskImage(1, QSize(12, 12)).pixelColor(5, 5), QColor(Qt::white));
        QCOMPARE(loaded.layerImage(1, QSize(12, 12)).pixelColor(5, 5).alpha(), 0);
    }
    void paintingMaterializesOnlyTheEditedPlane()
    {
        Document d;
        QVERIFY(d.newDocument(128, 96, true, 16));
        for (int i = 0; i < 80; ++i) {
            d.addLayer();
            d.addLayerMask();
        }
        const auto   initial = d.memoryUsage();
        const QImage before  = d.composite();
        d.setTool("brush");
        d.setColor(Qt::black);
        d.setBrushSize(9);
        d.setBrushHardness(1);
        d.beginStroke(50, 40);
        d.endStroke();
        QCOMPARE(d.memoryUsage()["compactPlanes"].toInt(), 159);
        QVERIFY(d.memoryUsage()["layerBytes"].toLongLong() <
                initial["layerBytes"].toLongLong() + 128 * 96 + 16);
        QVERIFY(d.maskImage(d.activeLayer()).pixelColor(50, 40).red() < 255);
        d.undo();
        QCOMPARE(d.memoryUsage()["compactPlanes"].toInt(), 160);
        QCOMPARE(d.composite(), before);
        d.setEditingMask(false);
        d.setColor(Qt::red);
        d.beginStroke(50, 40);
        d.endStroke();
        QCOMPARE(d.memoryUsage()["compactPlanes"].toInt(), 159);
        QCOMPARE(d.layerImage(d.activeLayer()).pixelColor(50, 40), QColor(Qt::red));
        d.undo();
        QCOMPARE(d.memoryUsage()["compactPlanes"].toInt(), 160);
        d.selectAll();
        d.cropToSelection();
        d.resizeImage(96, 64);
        QCOMPARE(d.memoryUsage()["compactPlanes"].toInt(), 160);
        QCOMPARE(d.maskImage(d.activeLayer()).size(), QSize(96, 64));
    }
    void layerCountLimitAllowsFiveHundredAndTwelveCompactLayers()
    {
        Document d;
        QVERIFY(d.newDocument(8, 8));
        for (int i = 1; i < 512; ++i)
            d.addLayer();
        QCOMPARE(d.layers().size(), 512);
        QSignalSpy errors(&d, &Document::errorOccurred);
        const int  history = d.historyIndex();
        d.addLayer();
        QCOMPARE(d.layers().size(), 512);
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.last().first().toString().contains("513"));
    }
    void frequencyCacheIsByteBoundedAndThumbnailsAvoidNativeRendering()
    {
        Document d;
        QVERIFY(d.newDocument(4096, 2048, false));
        for (int i = 0; i < 3; ++i) {
            d.setActiveLayer(0);
            d.addFrequencySeparation(i + 1);
        }
        QCOMPARE(d.memoryUsage()["frequencyCacheBytes"].toLongLong(), qint64(0));
        const auto thumb = d.layerImage(1, QSize(96, 96));
        QCOMPARE(thumb.size(), QSize(96, 48));
        QCOMPARE(d.memoryUsage()["frequencyCacheBytes"].toLongLong(), qint64(0));
        for (int i = 1; i <= 6; ++i) {
            QVERIFY(!d.layerImage(i).isNull());
            QVERIFY(d.memoryUsage()["frequencyCacheBytes"].toLongLong() <= 128LL * 1024 * 1024);
        }
    }
    void compactProjectFlagsCannotBypassDimensionValidation()
    {
        QTemporaryDir dir;
        Document      d;
        QVERIFY(d.newDocument(32, 24));
        d.addLayer();
        d.addLayerMask();
        const auto path = QUrl::fromLocalFile(dir.filePath("compact.pcr"));
        QVERIFY(d.saveProject(path));
        QFile input(path.toLocalFile());
        QVERIFY(input.open(QIODevice::ReadOnly));
        auto root = QJsonDocument::fromJson(input.readAll()).object();
        input.close();
        QCOMPARE(root["version"].toInt(), 7);
        auto layers    = root["layers"].toArray();
        auto compact   = layers[1].toObject();
        compact["png"] = layers[0].toObject()["png"]; // Full image with a compact flag.
        layers[1]      = compact;
        root["layers"] = layers;
        QFile output(path.toLocalFile());
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write(QJsonDocument(root).toJson());
        output.close();
        const auto before = d.layers();
        QVERIFY(!d.openFile(path));
        QCOMPARE(d.layers(), before);
    }
    void uniformMasksKeepTheirCoverageAndSurviveGeometryChanges()
    {
        Document d;
        QVERIFY(d.newDocument(40, 30, false));
        d.addAdjustmentLayer("invert");
        QCOMPARE(d.composite().pixelColor(20, 15), QColor(Qt::black));
        d.addLayerMask();
        d.fillMaskWithColor(QColor(128, 128, 128));
        QCOMPARE(d.memoryUsage()["compactPlanes"].toInt(), 1);
        QVERIFY(qAbs(d.composite().pixelColor(20, 15).red() - 127) <= 1);
        d.invertLayerMask();
        QVERIFY(qAbs(d.composite().pixelColor(20, 15).red() - 128) <= 1);
        d.beginLayerPreview();
        d.previewLayerParameter("_maskFeather", 3);
        d.finishLayerPreview();
        d.resizeImage(25, 20);
        QCOMPARE(d.memoryUsage()["compactPlanes"].toInt(), 1);
        QVERIFY(qAbs(d.composite().pixelColor(10, 10).red() - 128) <= 1);
        d.toggleMaskOnlyView(d.activeLayer());
        QCOMPARE(d.displayComposite().size(), QSize(25, 20));
        QCOMPARE(d.displayComposite().pixelColor(10, 10).red(), 127);
    }
    void largeLiveDocumentDoesNotEvictMetadataUndo()
    {
        Document d;
        QVERIFY(d.newDocument(6000, 4000, true, 16));
        d.setColor(Qt::red);
        d.fillWithColor(true);
        for (int i = 0; i < 12; ++i)
            d.renameLayer(0, QString("Rename %1").arg(i));
        QCOMPARE(d.historyIndex(), 13);
        for (int i = 0; i < 12; ++i)
            d.undo();
        QCOMPARE(d.layerImage(0).pixelColor(0, 0), QColor(Qt::red));
        d.undo();
        QCOMPARE(d.layerImage(0).pixelColor(0, 0).alpha(), 0);
    }

    void rawDecoderDevelopsSixteenBitSensorData()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QString::fromUtf8("échantillon.DNG"));
        QFile         file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(RawTestImage::dng());
        file.close();
        QVERIFY(RawImageReader::isRawFile(path));
        const auto result = RawImageReader::read(path);
        QVERIFY2(!result.image.isNull(), qPrintable(result.error));
        QCOMPARE(result.image.format(), QImage::Format_RGBA64_Premultiplied);
        QCOMPARE(result.image.size(), QSize(128, 96));
        bool finePrecision = false;
        for (int y = 8; y < 88; ++y)
            for (int x = 8; x < 120; ++x)
                finePrecision |= result.image.pixelColor(x, y).rgba64().green() % 257 != 0 ||
                                 result.image.pixelColor(x, y).rgba64().blue() % 257 != 0;
        QVERIFY(finePrecision);
        QFile rotated(dir.filePath("rotated.dng"));
        QVERIFY(rotated.open(QIODevice::WriteOnly));
        rotated.write(RawTestImage::dng(6));
        rotated.close();
        QCOMPARE(RawImageReader::read(rotated.fileName()).image.size(), QSize(96, 128));
        auto cancel = std::make_shared<std::atomic_bool>(true);
        QVERIFY(RawImageReader::read(path, cancel).cancelled);
        QVERIFY(RawImageReader::read(dir.filePath("missing.dng")).image.isNull());
    }
    void rawOpeningRunsAsynchronouslyAndPreservesDocumentOnFailure()
    {
        QTemporaryDir dir;
        QFile         file(dir.filePath("sample.dng"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(RawTestImage::dng());
        file.close();
        Document   document;
        QSignalSpy opened(&document, &Document::rawFileOpened);
        QVERIFY(document.openRawFile(QUrl::fromLocalFile(file.fileName())));
        QVERIFY(document.rawLoading());
        QTRY_VERIFY_WITH_TIMEOUT(!document.rawLoading(), 10000);
        QCOMPARE(opened.size(), 1);
        QCOMPARE(document.channelBits(), 16);
        const auto before = document.composite();
        QVERIFY(document.openRawFile(QUrl::fromLocalFile(dir.filePath("missing.dng"))));
        QTRY_VERIFY_WITH_TIMEOUT(!document.rawLoading(), 10000);
        QCOMPARE(document.composite(), before);
        QVERIFY(document.openRawFile(QUrl::fromLocalFile(file.fileName())));
        document.cancelRawLoad();
        QTRY_VERIFY_WITH_TIMEOUT(!document.rawLoading(), 10000);
        QCOMPARE(opened.size(), 1);
        QCOMPARE(document.composite(), before);
        QVERIFY(document.openRawFile(QUrl::fromLocalFile(file.fileName()), true));
        QTRY_VERIFY_WITH_TIMEOUT(!document.rawLoading(), 10000);
        QCOMPARE(document.layers().size(), 2);
        document.undo();
        QCOMPARE(document.layers().size(), 1);
    }
    void sixteenBitPixelsSurviveEditingHistoryProjectAndExport()
    {
        QTemporaryDir dir;
        QImage        image(32, 32, QImage::Format_RGBA64_Premultiplied);
        for (int y = 0; y < 32; ++y) {
            auto *row = reinterpret_cast<QRgba64 *>(image.scanLine(y));
            for (int x = 0; x < 32; ++x)
                row[x] = QRgba64::fromRgba64(15001 + x, 22003 + y, 32007 + x + y, 65535);
        }
        image.setColorSpace(QColorSpace::SRgb);
        const auto input = QUrl::fromLocalFile(dir.filePath("input.png"));
        QVERIFY(image.save(input.toLocalFile()));
        Document document;
        QVERIFY(document.openFile(input));
        QCOMPARE(document.channelBits(), 16);
        QCOMPARE(document.layerImage(0), image);
        document.setEyedropperSource(1);
        QCOMPARE(document.sampleToolColor(0, 0).rgba64(), image.pixelColor(0, 0).rgba64());
        QCOMPARE(document.composite().format(), QImage::Format_RGBA64_Premultiplied);
        QCOMPARE(document.composite().pixelColor(0, 0).rgba64(), image.pixelColor(0, 0).rgba64());
        QVERIFY(document.saveProject(QUrl::fromLocalFile(dir.filePath("wide.pcr"))));
        Document loaded;
        QVERIFY(loaded.openFile(document.projectUrl()));
        QCOMPARE(loaded.channelBits(), 16);
        QCOMPARE(loaded.layerImage(0), image);
        const auto output = QUrl::fromLocalFile(dir.filePath("output.png"));
        QVERIFY(loaded.exportImage(output));
        QCOMPARE(QImage(output.toLocalFile()).pixelColor(0, 0).rgba64(),
                 image.pixelColor(0, 0).rgba64());
        QVERIFY(loaded.setChannelBits(8));
        QCOMPARE(loaded.channelBits(), 8);
        loaded.undo();
        QCOMPARE(loaded.channelBits(), 16);
        QCOMPARE(loaded.layerImage(0), image);
        loaded.addLayer();
        QCOMPARE(loaded.layerImage(1).format(), QImage::Format_RGBA64_Premultiplied);
        loaded.setColor(QColor::fromRgba64(15001, 22003, 32007, 65535));
        loaded.setBrushHardness(1.);
        loaded.setBrushOpacity(1.);
        loaded.setBrushFlow(1.);
        loaded.setTool("brush");
        loaded.beginStroke(20, 20);
        loaded.endStroke();
        QCOMPARE(loaded.layerImage(1).pixelColor(20, 20).rgba64(),
                 QColor::fromRgba64(15001, 22003, 32007, 65535).rgba64());
        QCOMPARE(loaded.layerImage(1).format(), QImage::Format_RGBA64_Premultiplied);
        loaded.undo();
        loaded.undo();
        QCOMPARE(loaded.layerImage(0), image);
    }
    void sixteenBitAdjustmentsPreserveFineStepsAndAlpha()
    {
        using Filter = QImage (*)(const QImage &, const QVariantMap &);
        const QVector<QPair<QString, Filter>> filters{
            {"brightnessContrast", PhotoCrap::Filters::applyBrightnessContrast},
            {"channelMixer", PhotoCrap::Filters::applyChannelMixer},
            {"color", PhotoCrap::Filters::applyColor},
            {"curves", PhotoCrap::Filters::applyCurves},
            {"levels", PhotoCrap::Filters::applyLevels},
            {"light", PhotoCrap::Filters::applyLight},
            {"selectiveColor", PhotoCrap::Filters::applySelectiveColor},
            {"blackWhite", PhotoCrap::Filters::applyBlackWhite}};
        QImage image(32, 1, QImage::Format_RGBA64_Premultiplied);
        image.setColorSpace(QColorSpace::SRgb);
        auto *row = reinterpret_cast<QRgba64 *>(image.scanLine(0));
        for (int x = 0; x < 32; ++x)
            row[x] = QRgba64::fromRgba64(15001 + x, 15001 + x, 15001 + x, 65535);
        for (const auto &filter : filters) {
            const auto result = filter.second(image, {});
            QVERIFY2(result.format() == QImage::Format_RGBA64_Premultiplied,
                     qPrintable(filter.first));
            for (int x = 0; x < 32; ++x) {
                const auto actual = result.pixelColor(x, 0).rgba64();
                QCOMPARE(actual.alpha(), quint16(65535));
                QVERIFY2(qAbs(int(actual.red()) - (15001 + x)) <= 1, qPrintable(filter.first));
            }
            QVERIFY2(result.pixelColor(0, 0).rgba64().red() !=
                         result.pixelColor(31, 0).rgba64().red(),
                     qPrintable(filter.first));
        }
        const auto exposed = PhotoCrap::Filters::applyLight(image, {{"exposure", 1.}});
        QCOMPARE(exposed.pixelColor(0, 0).rgba64().red(), quint16(30002));
        QVERIFY(exposed.pixelColor(0, 0).rgba64().red() !=
                exposed.pixelColor(31, 0).rgba64().red());
        const auto gradient = PhotoCrap::Filters::applyGradientMap(
            image, {{"shadowColor", "#000000"}, {"highlightColor", "#ffffff"}});
        QCOMPARE(gradient.pixelColor(0, 0).rgba64().red(), quint16(15001));
    }
    void sixteenBitMasksBlendsAndBackgroundRendersRetainPrecision()
    {
        QTemporaryDir dir;
        QImage        image(32, 32, QImage::Format_RGBA64_Premultiplied);
        image.fill(QColor::fromRgba64(15001, 22003, 32007, 65535));
        image.setColorSpace(QColorSpace::SRgb);
        const auto url = QUrl::fromLocalFile(dir.filePath("input.png"));
        QVERIFY(image.save(url.toLocalFile()));
        Document document;
        QVERIFY(document.openFile(url));
        document.addAdjustmentLayer("light");
        document.beginLayerPreview();
        document.previewLayerParameter("exposure", 1.);
        QSignalSpy updated(&document, &Document::imageChanged);
        document.finishLayerPreview("Exposure");
        const int synchronousUpdates = updated.size();
        QTRY_VERIFY_WITH_TIMEOUT(updated.size() > synchronousUpdates, 10000);
        const auto full = document.composite();
        QCOMPARE(full.format(), QImage::Format_RGBA64_Premultiplied);
        QVERIFY(full.pixelColor(0, 0).rgba64().red() % 257 != 0);
        document.setLayerBlend(1, 3);
        QCOMPARE(document.composite().format(), QImage::Format_RGBA64_Premultiplied);
        document.setLayerBlend(1, 0);
        document.setActiveLayer(0);
        document.addLayerMask(true);
        QCOMPARE(document.maskImage(0).format(), QImage::Format_Grayscale8);
        QCOMPARE(document.composite().format(), QImage::Format_RGBA64_Premultiplied);
        document.setTool("brush");
        document.setBrushSize(5);
        document.beginStroke(20, 20);
        document.endStroke();
        QCOMPARE(document.layerImage(0).format(), QImage::Format_RGBA64_Premultiplied);
        QCOMPARE(document.layerImage(0).pixelColor(0, 0).rgba64(), image.pixelColor(0, 0).rgba64());
        document.setActiveLayer(1);
        QVERIFY(document.toolSupportsDepth("brush"));
        QVERIFY(!document.toolSupportsDepth("healing"));
    }
    void recentFilesAreOrderedDeduplicatedLimitedAndPruned()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RecentFilesManager recent(nullptr, false);
        QList<QUrl>        urls;
        for (int i = 0; i < 12; ++i) {
            const QString path = dir.filePath(QString("image-%1.png").arg(i));
            QFile         file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("x");
            file.close();
            urls.append(QUrl::fromLocalFile(path));
            recent.add(urls.last());
        }
        QCOMPARE(recent.entries().size(), 10);
        QCOMPARE(recent.entries().first().toMap().value("url").toUrl().toLocalFile(),
                 QFileInfo(urls.last().toLocalFile()).canonicalFilePath());
        recent.add(urls[5]);
        QCOMPARE(recent.entries().size(), 10);
        QCOMPARE(recent.entries().first().toMap().value("url").toUrl().toLocalFile(),
                 QFileInfo(urls[5].toLocalFile()).canonicalFilePath());
        QFile::remove(urls[5].toLocalFile());
        recent.refresh();
        QCOMPARE(recent.entries().size(), 9);
        recent.remove(urls[10]);
        QCOMPARE(recent.entries().size(), 8);
        recent.clear();
        QVERIFY(recent.entries().isEmpty());
        recent.add(QUrl::fromLocalFile(dir.filePath("missing.png")));
        QVERIFY(recent.entries().isEmpty());
    }
    void recentFilesPersistAcrossSessions()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
        QCoreApplication::setOrganizationName("PhotoCrapTests");
        QCoreApplication::setApplicationName("RecentFilesTest");
        const QString path = dir.filePath("saved.pcr");
        QFile         file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("project");
        file.close();
        {
            RecentFilesManager recent(nullptr, true);
            recent.clear();
            recent.add(QUrl::fromLocalFile(path));
            QCOMPARE(recent.entries().size(), 1);
        }
        {
            RecentFilesManager restored(nullptr, true);
            QCOMPARE(restored.entries().size(), 1);
            QCOMPARE(restored.entries().first().toMap().value("name").toString(),
                     QString("saved.pcr"));
            restored.clear();
        }
    }
    void shortcutsAreValidatedAndResettable()
    {
        ShortcutManager shortcuts(nullptr, false);
        QVERIFY(shortcuts.entries().size() > 250);
        QCOMPARE(shortcuts.sequence("toolBrush"), QString("B"));
        QCOMPARE(shortcuts.sequence("defaultColors"), QString("D"));
        QCOMPARE(shortcuts.sequence("swapColors"), QString("X"));
        QCOMPARE(shortcuts.sequence("brushSofter"), QString("Shift+["));
        QCOMPARE(shortcuts.sequence("brushHarder"), QString("Shift+]"));
        QCOMPARE(shortcuts.sequence("toolFill"), QString("G"));
        QCOMPARE(shortcuts.sequence("toolRotate"), QString("R"));
        QCOMPARE(shortcuts.sequence("toolZoom"), QString("Z"));
        QCOMPARE(shortcuts.sequence("toolDodge"), QString("O"));
        QCOMPARE(shortcuts.sequence("toolLasso"), QString("L"));
        QCOMPARE(shortcuts.sequence("toolSmartSelection"), QString("W"));
        QCOMPARE(shortcuts.sequence("toolStamp"), QString("S"));
        QCOMPARE(shortcuts.sequence("toolHealing"), QString("J"));
        QVERIFY(shortcuts.setSequence("toolBrush", "P"));
        QCOMPARE(shortcuts.sequence("toolBrush"), QString("P"));
        QVERIFY(!shortcuts.setSequence("toolMove", "P"));
        QCOMPARE(shortcuts.sequence("toolMove"), QString("V"));
        QVERIFY(shortcuts.setSequence("toolBrush", ""));
        QVERIFY(shortcuts.sequence("toolBrush").isEmpty());
        shortcuts.resetDefaults();
        QCOMPARE(shortcuts.sequence("toolBrush"), QString("B"));
        QVERIFY(!shortcuts.setSequence("missing", "Q"));
    }
    void shortcutCatalogDistinguishesCommandsGesturesAndMissingFeatures()
    {
        ShortcutManager shortcuts(nullptr, false);
        QSet<QString>   ids;
        QSet<QString>   executableKeys;
        for (const auto &value : shortcuts.entries()) {
            const auto    entry = value.toMap();
            const QString id    = entry.value("id").toString();
            QVERIFY(!ids.contains(id));
            ids.insert(id);
            QVERIFY(!entry.value("context").toString().isEmpty());
            if (!entry.value("editable").toBool()) {
                QVERIFY(shortcuts.sequence(id).isEmpty());
                QVERIFY(!shortcuts.setSequence(id, "Ctrl+Alt+9"));
            } else {
                const auto key = shortcuts.sequence(id);
                if (!key.isEmpty()) {
                    QVERIFY2(!executableKeys.contains(key), qPrintable(id + " : " + key));
                    executableKeys.insert(key);
                }
            }
        }
        QVERIFY(shortcuts.filteredEntries({}, 2).size() > 80);
        QVERIFY(shortcuts.filteredEntries({}, 3).size() > 30);
        QVERIFY(!shortcuts.filteredEntries("Fluidité", 2).isEmpty());
        QVERIFY(shortcuts.filteredEntries("Fluidité", 1).isEmpty());
        QCOMPARE(shortcuts.sequence("invertSelection"), QString("Ctrl+Shift+I"));
        QCOMPARE(shortcuts.sequence("channel0"), QString("Ctrl+2"));
        QCOMPARE(shortcuts.sequence("backgroundFill"), QString("Ctrl+Backspace"));
        QVERIFY(!shortcuts.setSequence("zoomIn", "Shift+B"));
        QVERIFY(!shortcuts.setSequence("toolMove", "Ctrl+I"));
        QVERIFY(shortcuts.setSequence("invertSelection", "Ctrl+Alt+9"));
        QVERIFY(!shortcuts.setSequence("reselect", "Ctrl+Alt+9"));
        shortcuts.resetDefaults();
        QCOMPARE(shortcuts.sequence("invertSelection"), QString("Ctrl+Shift+I"));
    }
    void newShortcutDefaultsPreserveSavedCustomizations()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
        QCoreApplication::setOrganizationName("PhotoCrapTests");
        QCoreApplication::setApplicationName("ShortcutMigrationTest");
        QSettings().setValue("shortcuts/toolBrush", "Ctrl+K");
        ShortcutManager migrated;
        QCOMPARE(migrated.sequence("toolBrush"), QString("Ctrl+K"));
        QVERIFY(migrated.sequence("preferencesAdobe").isEmpty());
        QVERIFY(!QSettings().contains("shortcuts/preferencesAdobe"));
        QVERIFY(migrated.setSequence("toolBrush", "P"));
        QVERIFY(migrated.setSequence("preferencesAdobe", "Ctrl+K"));
        ShortcutManager reloaded;
        QCOMPARE(reloaded.sequence("toolBrush"), QString("P"));
        QCOMPARE(reloaded.sequence("preferencesAdobe"), QString("Ctrl+K"));
    }
    void selectionsCanBeInvertedAndReselectedWithoutChangingPixels()
    {
        Document document;
        QVERIFY(document.newDocument(40, 30, true, 16));
        document.setTool("ellipticalSelect");
        document.beginStroke(4, 5);
        document.continueStroke(24, 20);
        document.endStroke();
        const auto original = document.selectionRegion();
        QVERIFY(!original.isEmpty());
        const auto pixels = document.layerImage(0);
        document.deselect();
        QVERIFY(!document.hasSelection());
        document.reselect();
        QCOMPARE(document.selectionRegion(), original);
        document.invertSelection();
        QCOMPARE(document.selectionRegion(), QRegion(QRect(0, 0, 40, 30)).subtracted(original));
        document.invertSelection();
        QCOMPARE(document.selectionRegion(), original);
        QCOMPARE(document.layerImage(0), pixels);
        QVERIFY(document.newDocument(20, 20));
        document.reselect();
        QVERIFY(!document.hasSelection());
    }
    void chosenFillPreservesColorsDepthAndSelection()
    {
        Document document;
        QVERIFY(document.newDocument(20, 20, true, 16));
        document.setColor(Qt::red);
        document.setSecondaryColor(Qt::blue);
        document.setTool("select");
        document.beginStroke(2, 2);
        document.continueStroke(10, 10);
        document.endStroke();
        const QColor chosen = QColor::fromRgba64(12345, 23456, 34567, 65535);
        document.fillSelectedWithColor(chosen);
        QCOMPARE(document.color(), QColor(Qt::red));
        QCOMPARE(document.secondaryColor(), QColor(Qt::blue));
        QCOMPARE(document.layerImage(0).pixelColor(4, 4).rgba64(), chosen.rgba64());
        QCOMPARE(document.layerImage(0).pixelColor(0, 0).alpha(), 0);
        document.addLayerMask(true);
        document.invertSelectedPixels();
        QCOMPARE(document.maskImage(0).pixelColor(4, 4).red(), 0);
        QCOMPARE(document.maskImage(0).pixelColor(0, 0).red(), 0);
        document.undo();
        QCOMPARE(document.maskImage(0).pixelColor(4, 4).red(), 255);
        document.undo(); // Remove the mask.
        document.undo();
        QCOMPARE(document.layerImage(0).pixelColor(4, 4).alpha(), 0);
        document.addLayer();
        document.selectAllLayers();
        QCOMPARE(document.selectedLayerCount(), 2);
    }
    void toolLayoutIsGroupedConfigurableAndPersistent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
        QCoreApplication::setOrganizationName("PhotoCrapTests");
        QCoreApplication::setApplicationName("ToolLayoutTest");
        QSettings().clear();

        QVariantList customized;
        {
            WorkspaceSettings  settings;
            const QVariantList defaults = settings.toolLayout();
            QCOMPARE(defaults.size(), 22);
            const QVariantMap paint = defaults[12].toMap();
            QCOMPARE(paint.value("shortcut").toString(), QString("toolFill"));
            QCOMPARE(paint.value("tools").toStringList(), QStringList({"gradient", "fill"}));
            const QVariantMap shape = defaults[19].toMap();
            QCOMPARE(shape.value("tools").toStringList(),
                     QStringList({"rectangle", "ellipse", "triangle", "polygon", "star", "line",
                                  "customShape"}));

            customized = defaults;
            customized.append(customized.takeFirst());
            settings.saveToolLayout(customized);
        }
        {
            WorkspaceSettings restored;
            QCOMPARE(restored.toolLayout(), customized);
            restored.resetToolLayout();
            QCOMPARE(restored.toolLayout().first().toMap().value("id").toString(), QString("move"));
        }
    }
    void foregroundAndBackgroundColorsCanResetAndSwap()
    {
        Document d;
        QCOMPARE(d.color(), QColor(Qt::black));
        QCOMPARE(d.secondaryColor(), QColor(Qt::white));
        d.setColor(Qt::red);
        d.setSecondaryColor(Qt::blue);
        d.resetColors();
        QCOMPARE(d.color(), QColor(Qt::black));
        QCOMPARE(d.secondaryColor(), QColor(Qt::white));
        d.swapColors();
        QCOMPARE(d.color(), QColor(Qt::white));
        QCOMPARE(d.secondaryColor(), QColor(Qt::black));
    }
    void pixelLayersFillWithForegroundAndBackgroundColors()
    {
        Document d;
        QVERIFY(d.newDocument(16, 16, true));
        d.setColor(QColor("#c02030"));
        d.setSecondaryColor(QColor("#3040c0"));
        d.fillWithColor(true);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor("#c02030"));
        d.fillWithColor(false);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor("#3040c0"));
        d.clearSelected();
        d.fillWithColor(true, true);
        QCOMPARE(d.composite().pixelColor(8, 8).alpha(), 0);
    }
    void brushUndoRedoAndCleanState()
    {
        Document d;
        QVERIFY(d.newDocument(64, 64, true));
        QVERIFY(!d.modified());
        d.setColor(Qt::red);
        d.setBrushSize(8);
        d.beginStroke(16, 16);
        d.continueStroke(40, 16);
        d.endStroke();
        QCOMPARE(d.composite().pixelColor(25, 16), QColor(Qt::red));
        QVERIFY(d.modified());
        d.undo();
        QCOMPARE(d.composite().pixelColor(25, 16).alpha(), 0);
        QVERIFY(!d.modified());
        d.redo();
        QCOMPARE(d.composite().pixelColor(25, 16), QColor(Qt::red));
        d.undo();
        d.setColor(Qt::blue);
        d.beginStroke(10, 10);
        d.endStroke();
        QVERIFY(!d.canRedo());
    }
    void pencilIsCrispAndSupportsPhotoshopAutoErase()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32, true));
        d.setTool("pencil");
        d.setBrushSize(5);
        d.setBrushOpacity(1);
        d.setBrushFlow(1);
        d.setColor(Qt::red);
        d.setSecondaryColor(Qt::blue);
        d.beginStroke(12, 12);
        d.endStroke();
        QCOMPARE(d.composite().pixelColor(12, 12), QColor(Qt::red));
        QCOMPARE(d.composite().pixelColor(15, 12).alpha(), 0);

        d.setPencilAutoErase(true);
        d.beginStroke(12, 12);
        d.endStroke();
        QCOMPARE(d.composite().pixelColor(12, 12), QColor(Qt::blue));
        d.undo();
        QCOMPARE(d.composite().pixelColor(12, 12), QColor(Qt::red));
    }
    void geometricShapeToolsShareFillAndStrokeOptions()
    {
        Document d;
        QVERIFY(d.newDocument(48, 48, true));
        d.setFilledShapes(true);
        d.setShapeFillColor(Qt::red);
        d.setShapeStrokeEnabled(false);
        for (const QString &tool :
             {QStringLiteral("triangle"), QStringLiteral("polygon"), QStringLiteral("star")}) {
            d.setTool(tool);
            d.beginStroke(6, 6);
            d.continueStroke(42, 42);
            d.endStroke();
            QVERIFY2(d.composite().pixelColor(24, 24).alpha() > 0,
                     qPrintable(tool + " fills its dragged bounds"));
            d.undo();
            QCOMPARE(d.composite().pixelColor(24, 24).alpha(), 0);
        }
        d.setShapeSides(8);
        QCOMPARE(d.shapeSides(), 8);
        d.setShapeStarRatio(.35);
        QCOMPARE(d.shapeStarRatio(), .35);
    }
    void geometricSelectionsClipPixelsToTheirActualShape()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32, true));
        d.setColor(Qt::red);
        d.setTool("ellipticalSelect");
        d.beginStroke(4, 4);
        d.continueStroke(28, 28);
        d.endStroke();
        QCOMPARE(d.selectionShape(), 1);
        d.fillWithColor(true);
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::red));
        QCOMPARE(d.composite().pixelColor(5, 5).alpha(), 0);

        d.deselect();
        d.clearSelected();
        d.setColor(Qt::blue);
        d.setTool("singleRowSelect");
        d.beginStroke(8, 12);
        d.endStroke();
        QCOMPARE(d.selection(), QRect(0, 12, 32, 1));
        d.fillWithColor(true);
        QCOMPARE(d.composite().pixelColor(8, 12), QColor(Qt::blue));
        QCOMPARE(d.composite().pixelColor(8, 11).alpha(), 0);

        d.deselect();
        d.setColor(Qt::green);
        d.setTool("singleColumnSelect");
        d.beginStroke(20, 8);
        d.endStroke();
        QCOMPARE(d.selection(), QRect(20, 0, 1, 32));
        d.fillWithColor(true);
        QCOMPARE(d.composite().pixelColor(20, 4), QColor(Qt::green));
        QCOMPARE(d.composite().pixelColor(19, 4).alpha(), 0);
    }
    void smartSelectionsFollowColorAndCombineLikePhotoshop()
    {
        Document d;
        QVERIFY(d.newDocument(32, 16, true));
        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.setTool("select");
        d.beginStroke(16, 0);
        d.continueStroke(31, 15);
        d.endStroke();
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        d.deselect();

        d.setTool("magicWand");
        d.setSelectionTolerance(0);
        d.setSelectionContiguous(true);
        d.setSelectionMode(0);
        d.beginStroke(4, 8);
        d.setColor(Qt::green);
        d.fillWithColor(true);
        QCOMPARE(d.composite().pixelColor(4, 8), QColor(Qt::green));
        QCOMPARE(d.composite().pixelColor(24, 8), QColor(Qt::blue));

        d.setSelectionMode(1);
        d.beginStroke(24, 8);
        d.setColor(Qt::yellow);
        d.fillWithColor(true);
        QCOMPARE(d.composite().pixelColor(4, 8), QColor(Qt::yellow));
        QCOMPARE(d.composite().pixelColor(24, 8), QColor(Qt::yellow));

        d.deselect();
        d.setSelectionMode(0);
        d.setTool("quickSelection");
        d.setBrushSize(4);
        d.beginStroke(4, 8);
        d.continueStroke(8, 8);
        d.endStroke();
        d.setColor(Qt::cyan);
        d.fillWithColor(true);
        QCOMPARE(d.composite().pixelColor(4, 8), QColor(Qt::cyan));
        QCOMPARE(d.composite().pixelColor(24, 8), QColor(Qt::yellow));
    }
    void eyedropperPointAveragesSourcesAndBackgroundDoNotEditPixels()
    {
        QTemporaryDir dir;
        QImage        image(7, 7, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        image.setPixelColor(3, 3, Qt::red);
        const QString path = dir.filePath("picker.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setEyedropperSource(1);
        d.setEyedropperSize(1);
        QCOMPARE(d.sampleToolColor(3, 3), QColor(Qt::red));
        d.setEyedropperSize(3);
        QCOMPARE(d.eyedropperSize(), 3);
        QCOMPARE(d.sampleToolColor(3, 3), QColor(28, 0, 0));
        d.setEyedropperSize(5);
        QCOMPARE(d.sampleToolColor(3, 3), QColor(10, 0, 0));
        d.setEyedropperSize(101);
        QCOMPARE(d.sampleToolColor(0, 0), QColor(5, 0, 0));
        d.setEyedropperSize(-100);
        QCOMPARE(d.eyedropperSize(), 1);
        d.setEyedropperSize(10000);
        QCOMPARE(d.eyedropperSize(), 101);
        d.setEyedropperSize(1);
        QVERIFY(!d.sampleToolColor(-1, 3).isValid());
        QVERIFY(!d.sampleToolColor(7, 3).isValid());
        d.addLayer();
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        d.setActiveLayer(0);
        d.setEyedropperSource(0);
        QCOMPARE(d.sampleToolColor(3, 3), QColor(Qt::blue));
        d.setDisplayChannel(1);
        QCOMPARE(d.sampleToolColor(3, 3),
                 QColor(Qt::blue)); // Colour sampling ignores channel display.
        d.setEyedropperSource(1);
        QCOMPARE(d.sampleToolColor(3, 3), QColor(Qt::red));
        const int    history = d.historyIndex();
        const QImage pixels  = d.layerImage(0);
        d.setTool("picker");
        d.setColor(Qt::green);
        d.setSecondaryColor(Qt::white);
        d.beginStroke(3, 3, true);
        d.endStroke();
        QCOMPARE(d.secondaryColor(), QColor(Qt::red));
        QCOMPARE(d.color(), QColor(Qt::green));
        d.beginStroke(3, 3);
        d.endStroke();
        QCOMPARE(d.color(), QColor(Qt::red));
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.layerImage(0), pixels);
        d.setEyedropperRing(false);
        QVERIFY(!d.eyedropperRing());
    }

    void eyedropperAveragesPremultipliedAlphaAndSamplesMasks()
    {
        QTemporaryDir dir;
        QImage        image(3, 3, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        image.setPixelColor(1, 1, QColor(255, 0, 0, 128));
        const QString path = dir.filePath("alpha-picker.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setEyedropperSource(1);
        d.setEyedropperSize(3);
        const QColor sample = d.sampleToolColor(1, 1);
        QCOMPARE(sample.red(), 255);
        QCOMPARE(sample.alpha(), 14);
        d.addLayerMask();
        d.setEditingMask(true);
        d.setColor(QColor(70, 70, 70));
        d.fillWithColor(true);
        QCOMPARE(d.sampleToolColor(1, 1), QColor(70, 70, 70));
    }

    void eyedropperUsesExactFullResolutionOncePreviewFinishes()
    {
        QTemporaryDir dir;
        QImage        image(1800, 1200, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        image.setPixelColor(801, 601, Qt::red);
        const QString path = dir.filePath("precise-picker.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setPreviewSize(600, 400);
        QSignalSpy rendered(&d, &Document::imageChanged);
        d.addAdjustmentLayer("brightnessContrast");
        QTRY_VERIFY_WITH_TIMEOUT(rendered.count() >= 2, 5000);
        d.setEyedropperSource(0);
        d.setEyedropperSize(1);
        const QColor exact = d.composite().pixelColor(801, 601);
        QCOMPARE(d.sampleToolColor(801, 601), exact);
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < 500; ++i)
            QVERIFY(d.sampleToolColor(801, 601).isValid());
        QVERIFY2(timer.elapsed() < 500,
                 "Moving the eyedropper must reuse the completed composition");
    }

    void artHistoryStylesInterpretSourceLocallyAndUndo()
    {
        QTemporaryDir dir;
        QImage        texture(200, 160, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < texture.height(); ++y)
            for (int x = 0; x < texture.width(); ++x)
                texture.setPixel(
                    x, y, qRgb((x * 17 + y * 3) % 256, (y * 13) % 256, (x * 5 + y * 11) % 256));
        const QString path = dir.filePath("art-source.png");
        QVERIFY(texture.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const QImage original = d.layerImage(0);
        d.setTool("artHistoryBrush");
        QCOMPARE(d.tool(), QString("artHistoryBrush"));
        d.setHistoryBrushSource(d.historyIndex());
        d.setBrushSize(20);
        d.setBrushHardness(1);
        d.setBrushOpacity(1);
        d.setBrushFlow(1);
        d.setBrushBlend(0);
        d.setBrushSmoothing(0);
        d.setArtHistoryArea(40);
        QList<QImage> results;
        const int     history = d.historyIndex();
        for (int style = 0; style < 10; ++style) {
            d.setArtHistoryStyle(style);
            d.beginStroke(80, 80);
            d.continueStroke(120, 80);
            d.endStroke();
            const QImage painted = d.layerImage(0);
            QVERIFY(painted != original);
            QCOMPARE(painted.pixel(5, 5), original.pixel(5, 5));
            QCOMPARE(painted.pixel(190, 150), original.pixel(190, 150));
            QCOMPARE(d.historyIndex(), history + 1);
            for (const QImage &other : results)
                QVERIFY(painted != other);
            results.append(painted);
            d.undo();
            QCOMPARE(d.layerImage(0), original);
            d.redo();
            QCOMPARE(d.layerImage(0), painted);
            d.undo();
        }
        d.beginStroke(80, 80);
        d.continueStroke(120, 80);
        d.cancelStroke();
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        d.setArtHistoryStyle(99);
        QCOMPARE(d.artHistoryStyle(), 9);
        d.setArtHistoryArea(999);
        QCOMPARE(d.artHistoryArea(), 500);
        d.setArtHistoryArea(-1);
        QCOMPARE(d.artHistoryArea(), 1);
        d.setArtHistoryTolerance(120);
        QCOMPARE(d.artHistoryTolerance(), 100);
        d.setArtHistoryTolerance(-5);
        QCOMPARE(d.artHistoryTolerance(), 0);
    }

    void artHistoryToleranceOpacitySelectionAndProtection()
    {
        Document d;
        d.newDocument(160, 120);
        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.setHistoryBrushSource(d.historyIndex());
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        const QImage blue = d.layerImage(0);
        d.setTool("artHistoryBrush");
        d.setArtHistoryStyle(5);
        d.setArtHistoryArea(1);
        d.setBrushSize(40);
        d.setBrushHardness(1);
        d.setBrushOpacity(.5);
        d.setBrushFlow(1);
        d.setBrushBlend(0);
        d.setBrushSmoothing(0);
        d.beginStroke(80, 60);
        d.endStroke();
        const QColor half = d.layerImage(0).pixelColor(80, 60);
        QVERIFY(qAbs(half.red() - 128) <= 2);
        QVERIFY(qAbs(half.blue() - 127) <= 2);
        d.undo();
        d.setBrushOpacity(1);
        d.setArtHistoryTolerance(100);
        const int noop = d.historyIndex();
        d.beginStroke(80, 60);
        d.endStroke();
        QCOMPARE(d.layerImage(0), blue);
        QCOMPARE(d.historyIndex(), noop);
        d.setArtHistoryTolerance(90);
        d.beginStroke(80, 60);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(80, 60), QColor(Qt::red));
        d.undo();
        d.setTool("ellipticalSelect");
        d.beginStroke(60, 40);
        d.continueStroke(100, 80);
        d.endStroke();
        const QRegion selection = d.selectionRegion();
        d.setTool("artHistoryBrush");
        d.setArtHistoryTolerance(0);
        d.setArtHistoryArea(100);
        d.beginStroke(80, 60);
        d.endStroke();
        const QImage selected = d.layerImage(0);
        QVERIFY(selected != blue);
        for (int y = 0; y < selected.height(); ++y)
            for (int x = 0; x < selected.width(); ++x)
                if (!selection.contains(QPoint(x, y)))
                    QCOMPARE(selected.pixel(x, y), blue.pixel(x, y));
        d.undo();
        d.deselect();
        d.setLayerLocked(0, true);
        const int locked = d.historyIndex();
        d.beginStroke(80, 60);
        d.endStroke();
        QCOMPARE(d.historyIndex(), locked);
        QCOMPARE(d.layerImage(0), blue);
        d.setLayerLocked(0, false);
        d.addLayerMask();
        d.setEditingMask(true);
        const QImage mask      = d.maskImage(0);
        const int    maskState = d.historyIndex();
        d.beginStroke(80, 60);
        d.endStroke();
        QCOMPARE(d.maskImage(0), mask);
        QCOMPARE(d.historyIndex(), maskState);
        d.setEditingMask(false);
        d.duplicateLayer();
        const int missing = d.historyIndex();
        d.beginStroke(80, 60);
        d.endStroke();
        QCOMPARE(d.historyIndex(), missing);
    }

    void artHistoryAreaControlsFootprintAndUhdStrokeRemainsLocal()
    {
        Document d;
        d.newDocument(3840, 2160);
        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.setHistoryBrushSource(d.historyIndex());
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        d.setTool("artHistoryBrush");
        d.setArtHistoryStyle(5);
        d.setBrushSize(16);
        d.setBrushHardness(1);
        d.setBrushFlow(1);
        d.setBrushOpacity(1);
        d.setBrushBlend(0);
        d.setBrushSmoothing(0);
        auto paintedCount = [&]() {
            int          count = 0;
            const QImage image = d.layerImage(0);
            for (int y = 900; y < 1100; ++y)
                for (int x = 900; x < 1100; ++x)
                    if (image.pixelColor(x, y) != QColor(Qt::blue))
                        ++count;
            return count;
        };
        d.setArtHistoryArea(1);
        d.beginStroke(1000, 1000);
        d.endStroke();
        const int small = paintedCount();
        QVERIFY(small > 0);
        d.undo();
        d.setArtHistoryArea(100);
        const int     history = d.historyIndex();
        QElapsedTimer timer;
        timer.start();
        d.beginStroke(1000, 1000);
        d.continueStroke(1010, 1000);
        d.endStroke();
        QVERIFY2(timer.elapsed() < 1000, "A small art gesture must not process the full UHD image");
        QVERIFY(paintedCount() > small * 2);
        QCOMPARE(d.layerImage(0).pixelColor(2000, 1000), QColor(Qt::blue));
        QCOMPARE(d.historyIndex(), history + 1);
    }

    void historyBrushRestoresOnlyPaintedPixelsAndUndoesOnce()
    {
        Document d;
        d.newDocument(160, 100);
        const QImage initial = d.layerImage(0);
        QCOMPARE(d.historyBrushSourceIndex(), 0);
        QVERIFY(d.historyBrushSourceAvailable());
        d.setColor(Qt::red);
        d.fillWithColor(true);
        const QImage red     = d.layerImage(0);
        const int    history = d.historyIndex();
        d.setTool("historyBrush");
        QCOMPARE(d.tool(), QString("historyBrush"));
        d.setBrushSize(20);
        d.setBrushHardness(1);
        d.setBrushOpacity(1);
        d.setBrushFlow(1);
        d.setBrushSmoothing(0);
        d.setBrushSpacing(.1);
        d.setBrushBlend(0);
        d.setColor(Qt::green); // Foreground colour is irrelevant to restoration.
        d.beginStroke(40, 50);
        d.continueStroke(110, 50);
        d.endStroke();
        const QImage restored = d.layerImage(0);
        QCOMPARE(restored.pixel(70, 50), initial.pixel(70, 50));
        QCOMPARE(restored.pixel(20, 20), red.pixel(20, 20));
        QCOMPARE(d.historyIndex(), history + 1);
        QCOMPARE(d.historyBrushSourceIndex(), 0);
        d.undo();
        QCOMPARE(d.layerImage(0), red);
        d.redo();
        QCOMPARE(d.layerImage(0), restored);
        d.beginStroke(30, 30);
        d.continueStroke(80, 30);
        d.cancelStroke();
        QCOMPARE(d.layerImage(0), restored);
        QCOMPARE(d.historyIndex(), history + 1);
        d.setHistoryBrushSource(d.historyIndex());
        const int noop = d.historyIndex();
        d.beginStroke(70, 50);
        d.endStroke();
        QCOMPARE(d.historyIndex(), noop);
    }

    void historyBrushSourceSelectionFutureStatesAndDiscardedBranches()
    {
        Document d;
        d.newDocument(100, 80);
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.setHistoryBrushSource(
            1); // State after blue fill, not the red fill labelled in snapshot 1.
        QCOMPARE(d.historyBrushSourceIndex(), 1);
        QCOMPARE(d.layerImage(0).pixelColor(50, 40), QColor(Qt::red));
        const int history = d.historyIndex();
        d.setTool("historyBrush");
        d.setBrushSize(20);
        d.setBrushHardness(1);
        d.setBrushOpacity(1);
        d.setBrushFlow(1);
        d.setBrushBlend(0);
        d.setBrushSmoothing(0);
        d.beginStroke(50, 40);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(50, 40), QColor(Qt::blue));
        QCOMPARE(d.layerImage(0).pixelColor(5, 5), QColor(Qt::red));
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        d.undo(); // Blue is current, red is the next redo state.
        QCOMPARE(d.historyIndex(), 1);
        d.setHistoryBrushSource(2);
        QCOMPARE(d.historyBrushSourceIndex(), 2);
        QVERIFY(d.canRedo());
        d.beginStroke(50, 40);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(50, 40), QColor(Qt::red));
        QCOMPARE(d.layerImage(0).pixelColor(5, 5), QColor(Qt::blue));
        QVERIFY(!d.canRedo());
        QCOMPARE(d.historyBrushSourceIndex(), -1);
        d.beginStroke(25, 40);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(25, 40), QColor(Qt::red));
        QVERIFY(d.historyBrushSourceAvailable());
        const int retainedIndex = d.historyBrushSourceIndex();
        d.setHistoryBrushSource(-1);
        d.setHistoryBrushSource(999);
        QCOMPARE(d.historyBrushSourceIndex(), retainedIndex);
    }

    void historyBrushRestoresTransparentPixelsAndHonoursOpacityFlowAndSelection()
    {
        Document d;
        d.newDocument(100, 80);
        d.addLayer();
        const int layer = d.activeLayer();
        d.setHistoryBrushSource(d.historyIndex()); // Empty layer exists at this state.
        const QImage transparent = d.layerImage(layer);
        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.setTool("select");
        d.beginStroke(40, 20);
        d.continueStroke(70, 60);
        d.endStroke();
        const QRegion selection = d.selectionRegion();
        d.setTool("historyBrush");
        d.setBrushSize(50);
        d.setBrushHardness(1);
        d.setBrushFlow(1);
        d.setBrushOpacity(.5);
        d.setBrushBlend(0);
        d.setBrushSmoothing(0);
        d.beginStroke(50, 40);
        d.endStroke();
        const QColor half = d.layerImage(layer).pixelColor(50, 40);
        QVERIFY(half.alpha() >= 126 && half.alpha() <= 128);
        QCOMPARE(half.red(), 255);
        QCOMPARE(d.layerImage(layer).pixelColor(35, 40), QColor(Qt::red));
        QCOMPARE(d.selectionRegion(), selection);
        d.undo();
        d.setBrushOpacity(1);
        d.setBrushFlow(.5);
        d.beginStroke(50, 40);
        d.endStroke();
        QVERIFY(d.layerImage(layer).pixelColor(50, 40).alpha() >= 126);
        QVERIFY(d.layerImage(layer).pixelColor(50, 40).alpha() <= 128);
        d.undo();
        d.setBrushFlow(1);
        d.beginStroke(50, 40);
        d.endStroke();
        QCOMPARE(d.layerImage(layer).pixel(50, 40), transparent.pixel(50, 40));
        QCOMPARE(d.layerImage(layer).pixelColor(35, 40), QColor(Qt::red));
        d.deselect();
        d.setBrushSize(20);
        d.setBrushHardness(0);
        d.undo();
        d.beginStroke(50, 40);
        d.endStroke();
        const int alphaCentre = d.layerImage(layer).pixelColor(50, 40).alpha();
        const int alphaEdge   = d.layerImage(layer).pixelColor(58, 40).alpha();
        QVERIFY(alphaCentre < alphaEdge);
        QVERIFY(alphaEdge < 255);
    }

    void historyBrushFollowsLayerIdentityAndRejectsMissingLayersMasksAndSizeChanges()
    {
        Document d;
        d.newDocument(120, 80);
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        d.addLayer();
        d.setColor(Qt::green);
        d.fillWithColor(true);
        d.setHistoryBrushSource(d.historyIndex());
        const QImage green = d.layerImage(1);
        d.moveLayer(-1);
        QCOMPARE(d.activeLayer(), 0);
        d.setColor(Qt::red);
        d.fillWithColor(true);
        const QImage other = d.layerImage(1);
        d.setTool("historyBrush");
        d.setBrushSize(20);
        d.setBrushHardness(1);
        d.setBrushOpacity(1);
        d.setBrushFlow(1);
        d.setBrushBlend(0);
        d.setBrushSmoothing(0);
        QVERIFY(d.historyBrushSourceAvailable());
        d.beginStroke(50, 40);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixel(50, 40), green.pixel(50, 40));
        QCOMPARE(d.layerImage(1), other);
        d.duplicateLayer();
        QVERIFY(!d.historyBrushSourceAvailable());
        const int    missing   = d.historyIndex();
        const QImage duplicate = d.layerImage(d.activeLayer());
        d.beginStroke(50, 40);
        d.endStroke();
        QCOMPARE(d.layerImage(d.activeLayer()), duplicate);
        QCOMPARE(d.historyIndex(), missing);
        d.setHistoryBrushSource(d.historyIndex());
        d.setLayerLocked(d.activeLayer(), true);
        const int locked = d.historyIndex();
        d.beginStroke(50, 40);
        d.endStroke();
        QCOMPARE(d.historyIndex(), locked);
        d.setLayerLocked(d.activeLayer(), false);
        d.addLayerMask();
        d.setEditingMask(true);
        const QImage mask        = d.maskImage(d.activeLayer());
        const int    maskHistory = d.historyIndex();
        d.beginStroke(50, 40);
        d.endStroke();
        QCOMPARE(d.maskImage(d.activeLayer()), mask);
        QCOMPARE(d.historyIndex(), maskHistory);
        QVERIFY(!d.historyBrushSourceAvailable());
        d.setEditingMask(false);
        d.resizeImage(60, 40);
        QVERIFY(!d.historyBrushSourceAvailable());
        const int resized = d.historyIndex();
        d.beginStroke(25, 20);
        d.endStroke();
        QCOMPARE(d.historyIndex(), resized);
        d.setHistoryBrushSource(d.historyIndex());
        QVERIFY(d.historyBrushSourceAvailable());
        d.newDocument(20, 20);
        QCOMPARE(d.historyBrushSourceIndex(), 0);
        QVERIFY(d.historyBrushSourceAvailable());
    }

    void historyBrushBlendModesAndRetainedSourceSurviveHistoryTrimming()
    {
        Document d;
        d.newDocument(64, 64);
        d.setColor(QColor(128, 128, 128));
        d.fillWithColor(true);
        d.setHistoryBrushSource(1);
        d.setColor(QColor(200, 200, 200));
        d.fillWithColor(true);
        d.setTool("historyBrush");
        d.setBrushSize(20);
        d.setBrushHardness(1);
        d.setBrushOpacity(1);
        d.setBrushFlow(1);
        d.setBrushSmoothing(0);
        d.setBrushBlend(3);
        d.beginStroke(32, 32);
        d.endStroke();
        QVERIFY(qAbs(d.layerImage(0).pixelColor(32, 32).red() - 100) <= 2);
        QCOMPARE(d.layerImage(0).pixelColor(5, 5), QColor(200, 200, 200));
        for (int i = 0; i < 45; ++i) {
            d.setColor(QColor(i + 10, 20, 30));
            d.fillWithColor(true);
        }
        QVERIFY(d.historyIndex() <= 40);
        QCOMPARE(d.historyBrushSourceIndex(), -1);
        QVERIFY(d.historyBrushSourceAvailable());
        d.setBrushBlend(0);
        d.beginStroke(32, 32);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(32, 32), QColor(128, 128, 128));
    }

    void historyBrushUhdGestureKeepsOutsidePixelsAndOneUndo()
    {
        Document d;
        d.newDocument(3840, 2160);
        const QColor original = d.layerImage(0).pixelColor(10, 10);
        d.setColor(Qt::red);
        d.fillWithColor(true);
        const int history = d.historyIndex();
        d.setTool("historyBrush");
        d.setBrushSize(48);
        d.setBrushHardness(1);
        d.setBrushOpacity(1);
        d.setBrushFlow(1);
        d.setBrushBlend(0);
        d.setBrushSmoothing(0);
        QElapsedTimer timer;
        timer.start();
        d.beginStroke(100, 100);
        d.continueStroke(220, 100);
        d.endStroke();
        QVERIFY2(timer.elapsed() < 1000,
                 "Small history brush gestures must stay local on UHD images");
        QCOMPARE(d.layerImage(0).pixelColor(160, 100), original);
        QCOMPARE(d.layerImage(0).pixelColor(500, 500), QColor(Qt::red));
        QCOMPARE(d.historyIndex(), history + 1);
    }

    void removalBrushAndLoopRepairLocallyWithOneUndo()
    {
        QTemporaryDir dir;
        QImage        image(200, 140, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const int grey = y % 6 < 3 ? 160 : 170;
                image.setPixel(x, y, qRgb(grey, grey, grey));
            }
        QPainter painter(&image);
        painter.fillRect(64, 60, 12, 12, Qt::red);
        painter.end();
        const QString path = dir.filePath("remove.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const QImage original = d.layerImage(0);
        const int    history  = d.historyIndex();
        d.setTool("remove");
        QCOMPARE(d.tool(), QString("remove"));
        d.setBrushSize(28);
        d.beginStroke(70, 66);
        const QRegion marked = d.removeRegion();
        QVERIFY(marked.contains(QPoint(70, 66)));
        QCOMPARE(d.layerImage(0), original);
        d.endStroke();
        QVERIFY(d.removeBusy());
        QCOMPARE(d.historyIndex(), history);
        QTRY_VERIFY_WITH_TIMEOUT(!d.removeBusy(), 5000);
        QCOMPARE(d.historyIndex(), history + 1);
        QVERIFY(!d.removeHasPending());
        const QImage repaired = d.layerImage(0);
        QVERIFY(repaired.pixelColor(70, 66).red() >= 155);
        QVERIFY(repaired.pixelColor(70, 66).green() >= 155);
        QVERIFY(repaired.pixel(70, 65) != repaired.pixel(70, 68));
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (!marked.contains(QPoint(x, y)))
                    QCOMPARE(repaired.pixel(x, y), original.pixel(x, y));
        d.undo();
        QCOMPARE(d.layerImage(0), original);
        d.redo();
        QCOMPARE(d.layerImage(0), repaired);
        d.undo();
        d.setRemoveAfterEachStroke(false);
        d.setBrushSize(4);
        d.beginStroke(58, 54);
        d.continueStroke(82, 54);
        d.continueStroke(82, 78);
        d.continueStroke(58, 78);
        d.continueStroke(58, 55);
        d.endStroke();
        QVERIFY(d.removeRegion().contains(QPoint(70, 66)));
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.layerImage(0), original);
        d.applyRemoval();
        QTRY_VERIFY_WITH_TIMEOUT(!d.removeBusy(), 5000);
        QVERIFY(d.layerImage(0).pixelColor(70, 66).green() >= 155);
        QCOMPARE(d.historyIndex(), history + 1);
    }

    void removalBatchSubtractSelectionAndCancellation()
    {
        Document d;
        d.newDocument(160, 100);
        d.setTool("select");
        d.beginStroke(35, 20);
        d.continueStroke(110, 80);
        d.endStroke();
        const QRegion selection = d.selectionRegion();
        const QImage  original  = d.layerImage(0);
        const int     history   = d.historyIndex();
        d.setTool("remove");
        d.setRemoveAfterEachStroke(false);
        d.setBrushSize(24);
        d.beginStroke(35, 35);
        d.continueStroke(60, 35);
        d.endStroke();
        d.beginStroke(90, 65);
        d.endStroke();
        QVERIFY((d.removeRegion() - selection).isEmpty());
        QVERIFY(d.removeRegion().contains(QPoint(60, 35)));
        QVERIFY(d.removeRegion().contains(QPoint(90, 65)));
        const QRegion both = d.removeRegion();
        d.setBrushSize(10);
        d.beginStroke(60, 35, true);
        d.endStroke();
        QVERIFY(!d.removeRegion().contains(QPoint(60, 35)));
        QVERIFY(d.removeRegion().contains(QPoint(90, 65)));
        QVERIFY((d.removeRegion() - both).isEmpty());
        d.setRemoveSubtract(true);
        d.beginStroke(90, 65);
        d.endStroke();
        QVERIFY(!d.removeRegion().contains(QPoint(90, 65)));
        d.cancelStroke();
        QVERIFY(!d.removeHasPending());
        QCOMPARE(d.selectionRegion(), selection);
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        d.setRemoveSubtract(false);
        d.beginStroke(55, 45);
        d.endStroke();
        d.applyRemoval();
        QVERIFY(d.removeBusy());
        d.cancelRemoval();
        QTest::qWait(100);
        QVERIFY(!d.removeBusy());
        QVERIFY(!d.removeHasPending());
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        d.beginStroke(55, 45);
        d.endStroke();
        d.setTool("brush");
        QVERIFY(!d.removeHasPending());
    }

    void removalSamplesVisibleLayersIntoEmptyLayerAndRejectsMasksLocks()
    {
        QTemporaryDir dir;
        QImage        image(180, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(150, 150, 150));
        QPainter painter(&image);
        painter.fillRect(54, 54, 12, 12, Qt::red);
        painter.end();
        const QString path = dir.filePath("remove-layers.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const QImage base = d.layerImage(0);
        d.addLayer();
        const int active = d.activeLayer(), history = d.historyIndex();
        d.setTool("remove");
        d.setRemoveSampleAllLayers(true);
        d.setBrushSize(32);
        d.beginStroke(60, 60);
        d.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!d.removeBusy(), 5000);
        const QImage result = d.layerImage(active);
        QVERIFY(result.pixelColor(60, 60).alpha() > 240);
        QVERIFY(qAbs(result.pixelColor(60, 60).red() - 150) <= 3);
        QCOMPARE(result.pixelColor(10, 10).alpha(), 0);
        QCOMPARE(d.layerImage(0), base);
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        d.setLayerVisible(0, false);
        const int hiddenHistory = d.historyIndex();
        d.beginStroke(60, 60);
        d.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!d.removeBusy(), 5000);
        QCOMPARE(d.layerImage(active).pixelColor(60, 60).alpha(), 0);
        QCOMPARE(d.historyIndex(), hiddenHistory);
        d.setLayerLocked(active, true);
        d.beginStroke(60, 60);
        d.endStroke();
        QVERIFY(!d.removeHasPending());
        d.setLayerLocked(active, false);
        d.addLayerMask();
        d.setEditingMask(true);
        const QImage mask = d.maskImage(active);
        d.beginStroke(60, 60);
        d.endStroke();
        QVERIFY(!d.removeBusy());
        QVERIFY(!d.removeHasPending());
        QCOMPARE(d.maskImage(active), mask);
    }

    void removalEdgeToEdgeStrokeIncludesTheCornerAndCancelsWithoutHistory()
    {
        QTemporaryDir dir;
        QImage        image(160, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(180, 180, 180));
        QPainter painter(&image);
        painter.fillRect(0, 0, 12, 12, Qt::red);
        painter.end();
        const QString path = dir.filePath("remove-corner.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const QImage original = d.layerImage(0);
        d.setTool("remove");
        d.setBrushSize(6);
        d.setRemoveAfterEachStroke(false);
        d.beginStroke(0, 40);
        d.continueStroke(20, 20);
        d.continueStroke(40, 0);
        d.endStroke();
        QVERIFY(d.removeRegion().contains(QPoint(4, 4)));
        QVERIFY(!d.removeRegion().contains(QPoint(90, 90)));
        const QRegion region  = d.removeRegion();
        const int     history = d.historyIndex();
        d.applyRemoval();
        QTRY_VERIFY_WITH_TIMEOUT(!d.removeBusy(), 5000);
        QVERIFY(qAbs(d.layerImage(0).pixelColor(4, 4).green() - 180) <= 3);
        QCOMPARE(d.historyIndex(), history + 1);
        const QImage repaired = d.layerImage(0);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (!region.contains(QPoint(x, y)))
                    QCOMPARE(repaired.pixel(x, y), image.pixel(x, y));
        d.undo();
        d.beginStroke(0, 40);
        d.continueStroke(20, 20);
        d.continueStroke(40, 0);
        d.cancelStroke();
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        QVERIFY(!d.removeHasPending());
    }

    void removalNewMarksReplacePendingWorkAndNewDocumentDiscardsIt()
    {
        QTemporaryDir dir;
        QImage        image(800, 600, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(170, 170, 170));
        QPainter painter(&image);
        painter.fillRect(190, 190, 20, 20, Qt::red);
        painter.fillRect(490, 390, 20, 20, Qt::red);
        painter.end();
        const QString path = dir.filePath("remove-latest.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("remove");
        d.setBrushSize(80);
        const int history = d.historyIndex();
        d.beginStroke(200, 200);
        d.endStroke();
        QVERIFY(d.removeBusy());
        d.beginStroke(500, 400);
        d.endStroke();
        QVERIFY(d.removeBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!d.removeBusy(), 5000);
        QVERIFY(d.layerImage(0).pixelColor(200, 200).green() > 160);
        QVERIFY(d.layerImage(0).pixelColor(500, 400).green() > 160);
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        d.beginStroke(200, 200);
        d.endStroke();
        d.newDocument(32, 32);
        const QImage newImage   = d.layerImage(0);
        const int    newHistory = d.historyIndex();
        QTest::qWait(100);
        QCOMPARE(d.layerImage(0), newImage);
        QCOMPARE(d.historyIndex(), newHistory);
        QVERIFY(!d.removeBusy());
        QVERIFY(!d.removeHasPending());
    }

    void removalUhdLoopReleaseDoesNotRunReconstructionOnUiThread()
    {
        QTemporaryDir dir;
        QImage        image(3840, 2160, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(160, 160, 160));
        QPainter painter(&image);
        painter.fillRect(1200, 700, 400, 400, Qt::red);
        painter.end();
        const QString path = dir.filePath("remove-uhd.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("remove");
        d.setBrushSize(12);
        d.beginStroke(1180, 680);
        d.continueStroke(1620, 680);
        d.continueStroke(1620, 1120);
        d.continueStroke(1180, 1120);
        d.continueStroke(1180, 680);
        QElapsedTimer timer;
        timer.start();
        d.endStroke();
        QVERIFY2(timer.elapsed() < 500, "Release must schedule reconstruction, not execute it");
        QVERIFY(d.removeBusy());
        QCOMPARE(d.layerImage(0).pixelColor(1400, 900), QColor(Qt::red));
        QTRY_VERIFY_WITH_TIMEOUT(!d.removeBusy(), 10000);
        QVERIFY(qAbs(d.layerImage(0).pixelColor(1400, 900).green() - 160) <= 3);
        QCOMPARE(d.layerImage(0).pixelColor(20, 20), QColor(160, 160, 160));
    }

    void contentMoveRepairsTheHolePreservesTextureAndUndoesOnce()
    {
        QTemporaryDir dir;
        QImage        image(180, 120, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const int grey = y % 6 < 3 ? 165 : 175;
                image.setPixel(x, y, qRgb(grey, grey, grey));
            }
        QPainter painter(&image);
        painter.fillRect(48, 48, 8, 8, QColor(220, 40, 30));
        painter.end();
        const QString path = dir.filePath("content-move.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const QImage original = d.layerImage(0);
        const int    history  = d.historyIndex();
        d.setTool("contentAwareMove");
        QCOMPARE(d.tool(), QString("contentAwareMove"));
        d.beginStroke(40, 40);
        d.continueStroke(64, 40);
        d.continueStroke(64, 64);
        d.continueStroke(40, 64);
        d.continueStroke(40, 40);
        d.endStroke();
        const QRegion selection = d.selectionRegion();
        QVERIFY(d.hasSelection());
        QCOMPARE(d.historyIndex(), history);
        d.beginStroke(52, 52);
        d.continueStroke(132, 52);
        QCOMPARE(d.patchTargetRegion(), selection.translated(80, 0));
        QVERIFY(!d.patchPreviewImage().isNull());
        QCOMPARE(d.layerImage(0), original);
        d.endStroke();
        QVERIFY(d.contentMoveBusy());
        QCOMPARE(d.historyIndex(), history); // worker cannot mutate Document
        QTRY_VERIFY_WITH_TIMEOUT(!d.contentMoveBusy(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!d.patchDragging(), 5000);
        QCOMPARE(d.historyIndex(), history + 1);
        const QImage moved      = d.layerImage(0);
        const QColor background = moved.pixelColor(52, 52);
        QVERIFY(qAbs(background.red() - image.pixelColor(20, 52).red()) <= 3);
        QCOMPARE(background.red(), background.green());
        QVERIFY(moved.pixel(50, 49) != moved.pixel(50, 51));
        QCOMPARE(moved.pixelColor(132, 52), QColor(220, 40, 30));
        QCOMPARE(d.selectionRegion(), selection.translated(80, 0));
        const QRegion affected = selection | selection.translated(80, 0);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (!affected.contains(QPoint(x, y)))
                    QCOMPARE(moved.pixel(x, y), original.pixel(x, y));
        d.undo();
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.selectionRegion(), selection);
        d.redo();
        QCOMPARE(d.layerImage(0), moved);
    }

    void contentMoveExtensionOverlapAndTransparentBackground()
    {
        QTemporaryDir dir;
        QImage        image(160, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.fillRect(48, 48, 8, 8, QColor(220, 40, 30));
        painter.end();
        const QString path = dir.filePath("content-extend.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("select");
        d.beginStroke(40, 40);
        d.continueStroke(64, 64);
        d.endStroke();
        const QRegion selection = d.selectionRegion();
        const QImage  original  = d.layerImage(0);
        d.setTool("contentAwareMove");
        d.setContentMoveMode(1);
        d.beginStroke(52, 52);
        d.continueStroke(102, 52);
        d.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!d.contentMoveBusy(), 5000);
        QCOMPARE(d.layerImage(0).pixelColor(52, 52), QColor(220, 40, 30));
        QCOMPARE(d.layerImage(0).pixelColor(102, 52), QColor(220, 40, 30));
        QCOMPARE(d.layerImage(0).copy(selection.boundingRect()),
                 original.copy(selection.boundingRect()));
        d.undo();
        d.setContentMoveMode(0);
        d.setContentMoveStructure(7);
        d.beginStroke(52, 52);
        d.continueStroke(56, 52); // overlapping source and destination
        d.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!d.contentMoveBusy(), 5000);
        QCOMPARE(d.layerImage(0).pixelColor(56, 52), QColor(220, 40, 30));
        QCOMPARE(d.layerImage(0).pixelColor(48, 52).alpha(), 0);
        QCOMPARE(d.layerImage(0).pixelColor(52, 52), QColor(220, 40, 30));
        d.undo();
        QCOMPARE(d.layerImage(0), original);
    }

    void contentMoveSamplesVisibleLayersWithoutModifyingThem()
    {
        QTemporaryDir dir;
        QImage        image(160, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(165, 165, 165));
        QPainter painter(&image);
        painter.fillRect(48, 48, 8, 8, QColor(220, 40, 30));
        painter.end();
        const QString path = dir.filePath("content-move-layers.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const QImage base = d.layerImage(0);
        d.addLayer();
        const QImage empty = d.layerImage(1);
        d.setTool("select");
        d.beginStroke(40, 40);
        d.continueStroke(64, 64);
        d.endStroke();
        d.setTool("contentAwareMove");
        d.setContentMoveSampleAllLayers(true);
        d.beginStroke(52, 52);
        d.continueStroke(102, 52);
        d.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!d.contentMoveBusy(), 5000);
        QCOMPARE(d.layerImage(0), base);
        QCOMPARE(d.layerImage(1).pixelColor(52, 52), QColor(165, 165, 165));
        QCOMPARE(d.layerImage(1).pixelColor(102, 52), QColor(220, 40, 30));
        QCOMPARE(d.layerImage(1).pixelColor(10, 10).alpha(), 0);
        d.undo();
        QCOMPARE(d.layerImage(1), empty);
        d.setLayerVisible(0, false);
        const int history = d.historyIndex();
        d.beginStroke(52, 52);
        d.continueStroke(102, 52);
        d.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!d.contentMoveBusy(), 5000);
        QCOMPARE(d.layerImage(1), empty);
        QCOMPARE(d.historyIndex(), history);
    }

    void contentMoveCancelAndNewEditsDiscardPendingResults()
    {
        QTemporaryDir dir;
        QImage        image(180, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(165, 165, 165));
        QPainter painter(&image);
        painter.fillRect(48, 48, 8, 8, Qt::red);
        painter.end();
        const QString path = dir.filePath("content-move-cancel.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("select");
        d.beginStroke(40, 40);
        d.continueStroke(64, 64);
        d.endStroke();
        const QRegion selection = d.selectionRegion();
        const QImage  original  = d.layerImage(0);
        const int     history   = d.historyIndex();
        d.setTool("contentAwareMove");
        d.beginStroke(52, 52);
        d.continueStroke(132, 52);
        d.endStroke();
        QVERIFY(d.contentMoveBusy());
        d.cancelStroke();
        QTest::qWait(80);
        QVERIFY(!d.contentMoveBusy());
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.selectionRegion(), selection);
        QCOMPARE(d.historyIndex(), history);
        d.beginStroke(52, 52);
        d.continueStroke(132, 52);
        d.endStroke();
        d.setTool("brush");
        d.setColor(Qt::blue);
        d.setBrushSize(5);
        d.setBrushHardness(1);
        d.beginStroke(132, 52);
        d.endStroke();
        const QImage painted = d.layerImage(0);
        QTest::qWait(80);
        QCOMPARE(d.layerImage(0), painted);
        QCOMPARE(d.layerImage(0).pixelColor(52, 52), QColor(Qt::red));
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        d.deselect();
        d.setTool("select");
        d.beginStroke(40, 40);
        d.continueStroke(64, 64);
        d.endStroke();
        d.setTool("contentAwareMove");
        d.beginStroke(52, 52);
        d.continueStroke(132, 52);
        d.endStroke();
        QVERIFY(d.newDocument(20, 20, true));
        QTest::qWait(80);
        QCOMPARE(d.width(), 20);
        QCOMPARE(d.height(), 20);
        QVERIFY(!d.contentMoveBusy());
        QCOMPARE(d.layerImage(0).pixelColor(10, 10).alpha(), 0);
    }

    void contentMoveNoOpsMasksLocksAndColorAdaptation()
    {
        QTemporaryDir dir;
        QImage        image(160, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(180, 140, 120));
        QPainter painter(&image);
        painter.fillRect(0, 0, 75, 120, QColor(100, 160, 200));
        painter.fillRect(48, 48, 8, 8, QColor(80, 120, 160));
        painter.end();
        const QString path = dir.filePath("content-move-color.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("select");
        d.beginStroke(40, 40);
        d.continueStroke(64, 64);
        d.endStroke();
        d.setTool("contentAwareMove");
        d.setContentMoveMode(1);
        d.setContentMoveColor(10);
        d.setContentMoveStructure(7);
        const int history = d.historyIndex();
        d.beginStroke(52, 52);
        d.endStroke();
        QCOMPARE(d.historyIndex(), history);
        QVERIFY(!d.contentMoveBusy());
        d.beginStroke(52, 52);
        d.continueStroke(112, 52);
        d.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!d.contentMoveBusy(), 5000);
        const QColor adapted = d.layerImage(0).pixelColor(112, 52);
        QVERIFY(qAbs(adapted.red() - 160) <= 2);
        QVERIFY(qAbs(adapted.green() - 100) <= 2);
        QVERIFY(qAbs(adapted.blue() - 80) <= 2);
        d.undo();
        d.setLayerLocked(0, true);
        const int locked = d.historyIndex();
        d.beginStroke(52, 52);
        d.continueStroke(112, 52);
        d.endStroke();
        QVERIFY(!d.contentMoveBusy());
        QCOMPARE(d.historyIndex(), locked);
        d.setLayerLocked(0, false);
        d.addLayerMask();
        d.setEditingMask(true);
        const QImage mask        = d.maskImage(0);
        const int    maskHistory = d.historyIndex();
        d.beginStroke(52, 52);
        d.continueStroke(112, 52);
        d.endStroke();
        QVERIFY(!d.contentMoveBusy());
        QCOMPARE(d.maskImage(0), mask);
        QCOMPARE(d.historyIndex(), maskHistory);
        d.setContentMoveMode(9);
        QCOMPARE(d.contentMoveMode(), 1);
        d.setContentMoveStructure(-9);
        QCOMPARE(d.contentMoveStructure(), 1);
        d.setContentMoveStructure(99);
        QCOMPARE(d.contentMoveStructure(), 7);
        d.setContentMoveColor(-1);
        QCOMPARE(d.contentMoveColor(), 0);
        d.setContentMoveColor(99);
        QCOMPARE(d.contentMoveColor(), 10);
    }

    void contentMoveEdgeFallbackAndUhdReleaseRemainResponsive()
    {
        QTemporaryDir dir;
        QImage        image(70, 70, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(170, 170, 170));
        QPainter painter(&image);
        painter.fillRect(10, 10, 6, 6, Qt::red);
        painter.end();
        const QString path = dir.filePath("content-move-fallback.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const QImage original = d.layerImage(0);
        d.setTool("select");
        d.beginStroke(0, 0);
        d.continueStroke(60, 60);
        d.endStroke();
        d.setTool("contentAwareMove");
        d.setContentMoveStructure(7);
        d.beginStroke(12, 12);
        d.continueStroke(21, 21);
        d.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!d.contentMoveBusy(), 5000);
        QCOMPARE(d.layerImage(0).pixelColor(12, 12), QColor(170, 170, 170));
        QCOMPARE(d.layerImage(0).pixelColor(21, 21), QColor(Qt::red));
        d.undo();
        QCOMPARE(d.layerImage(0), original);
        QImage uhd(3840, 2160, QImage::Format_ARGB32_Premultiplied);
        uhd.fill(QColor(170, 170, 170));
        QPainter largePainter(&uhd);
        largePainter.fillRect(100, 100, 480, 480, Qt::red);
        largePainter.end();
        const QString uhdPath = dir.filePath("content-move-uhd.png");
        QVERIFY(uhd.save(uhdPath));
        QVERIFY(d.openFile(QUrl::fromLocalFile(uhdPath)));
        d.setTool("select");
        d.beginStroke(80, 80);
        d.continueStroke(600, 600);
        d.endStroke();
        d.setTool("contentAwareMove");
        const int     history     = d.historyIndex();
        const qint64  originalKey = d.layerImage(0).cacheKey();
        QElapsedTimer drag;
        drag.start();
        d.beginStroke(300, 300);
        d.continueStroke(1800, 300);
        QCOMPARE(d.layerImage(0).cacheKey(), originalKey);
        d.endStroke();
        qInfo() << "UHD Content-Aware Move drag/release (ms):" << drag.elapsed();
        QVERIFY(drag.elapsed() < 150);
        QVERIFY(d.contentMoveBusy());
        QCOMPARE(d.historyIndex(), history);
        QTRY_VERIFY_WITH_TIMEOUT(!d.contentMoveBusy(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!d.patchDragging(), 10000);
        QCOMPARE(d.historyIndex(), history + 1);
        QCOMPARE(d.layerImage(0).pixelColor(300, 300), QColor(170, 170, 170));
        QCOMPARE(d.layerImage(0).pixelColor(1800, 300), QColor(Qt::red));
        QCOMPARE(d.layerImage(0).pixelColor(3200, 1800), QColor(170, 170, 170));
    }

    void patchRepairsSourceAndDestinationLocallyWithOneUndo()
    {
        QTemporaryDir dir;
        QImage        image(160, 130, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const int texture = y % 4 < 2 ? -3 : 3;
                image.setPixel(x, y,
                               x < 60 ? qRgb(100 + texture, 160 + texture, 200 + texture)
                                      : qRgb(180 + texture, 140 + texture, 120 + texture));
            }
        QPainter painter(&image);
        painter.fillRect(84, 44, 4, 4, Qt::black);
        painter.end();
        const QString path = dir.filePath("patch.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const QImage original = d.layerImage(0);
        const int    history  = d.historyIndex();
        d.setTool("patch");
        QCOMPARE(d.tool(), QString("patch"));
        d.beginStroke(78, 38);
        d.continueStroke(94, 38);
        d.continueStroke(94, 54);
        d.continueStroke(78, 54);
        d.continueStroke(78, 38);
        d.endStroke();
        QVERIFY(d.hasSelection());
        const QRegion selection = d.selectionRegion();
        QCOMPARE(d.historyIndex(), history);
        d.beginStroke(85, 45);
        d.continueStroke(35, 45);
        QVERIFY(d.patchDragging());
        QVERIFY(!d.patchPreviewImage().isNull());
        QCOMPARE(d.patchTargetRegion(), selection);
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        d.endStroke();
        QVERIFY(!d.patchDragging());
        QVERIFY(d.patchPreviewImage().isNull());
        QCOMPARE(d.selectionRegion(), selection);
        QCOMPARE(d.historyIndex(), history + 1);
        const QImage repaired = d.layerImage(0);
        const QColor pixel    = repaired.pixelColor(85, 45);
        QVERIFY(qAbs(pixel.red() - 177) <= 3);
        QVERIFY(qAbs(pixel.green() - 137) <= 3);
        QVERIFY(qAbs(pixel.blue() - 117) <= 3);
        QVERIFY(repaired.pixel(85, 44) != repaired.pixel(85, 46));
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (!selection.contains(QPoint(x, y)))
                    QCOMPARE(repaired.pixel(x, y), original.pixel(x, y));
        QCOMPARE(d.composite(), repaired);
        d.undo();
        QCOMPARE(d.layerImage(0), original);
        d.redo();
        QCOMPARE(d.layerImage(0), repaired);
        d.undo();
        d.setTool("select");
        d.beginStroke(28, 38);
        d.continueStroke(44, 54);
        d.endStroke();
        const QRegion source = d.selectionRegion();
        d.setTool("patch");
        d.setPatchMode(1);
        d.beginStroke(35, 45);
        d.continueStroke(85, 45);
        QCOMPARE(d.patchTargetRegion(), source.translated(50, 0));
        d.endStroke();
        QCOMPARE(d.selectionRegion(), source.translated(50, 0));
        QCOMPARE(d.historyIndex(), history + 1);
        QVERIFY(d.layerImage(0).pixelColor(85, 45).red() > 170);
        QCOMPARE(d.layerImage(0).copy(source.boundingRect()), original.copy(source.boundingRect()));
        d.undo();
        QCOMPARE(d.selectionRegion(), source);
        QCOMPARE(d.layerImage(0), original);
    }

    void patchIrregularSelectionEdgesCancelMasksLocksAndNoOps()
    {
        QTemporaryDir dir;
        QImage        image(100, 100, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(180, 180, 180));
        QPainter painter(&image);
        painter.fillRect(18, 18, 4, 4, Qt::black);
        painter.end();
        const QString path = dir.filePath("patch-ellipse.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("ellipticalSelect");
        d.beginStroke(10, 10);
        d.continueStroke(30, 30);
        d.endStroke();
        const QRegion selection = d.selectionRegion();
        const QImage  original  = d.layerImage(0);
        const int     history   = d.historyIndex();
        d.setTool("patch");
        d.beginStroke(20, 20);
        d.continueStroke(70, 20);
        d.cancelStroke();
        QCOMPARE(d.selectionRegion(), selection);
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        QVERIFY(d.patchPreviewImage().isNull());
        d.beginStroke(20, 20);
        d.endStroke();
        QCOMPARE(d.historyIndex(), history);
        d.beginStroke(20, 20);
        d.continueStroke(1000, -1000);
        QCOMPARE(d.selection().right(), d.width() - 1);
        QCOMPARE(d.selection().top(), 0);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(20, 20).red() > 170);
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 100; ++x)
                if (!selection.contains(QPoint(x, y)))
                    QCOMPARE(d.layerImage(0).pixel(x, y), original.pixel(x, y));
        d.undo();
        d.setLayerLocked(0, true);
        const int lockedHistory = d.historyIndex();
        d.beginStroke(20, 20);
        d.continueStroke(70, 20);
        d.endStroke();
        QVERIFY(!d.patchDragging());
        QCOMPARE(d.historyIndex(), lockedHistory);
        d.setLayerLocked(0, false);
        d.addLayerMask();
        d.setEditingMask(true);
        const QImage mask        = d.maskImage(0);
        const int    maskHistory = d.historyIndex();
        d.beginStroke(20, 20);
        d.continueStroke(70, 20);
        d.endStroke();
        QCOMPARE(d.historyIndex(), maskHistory);
        QCOMPARE(d.maskImage(0), mask);
        QCOMPARE(d.layerImage(0), original);
        d.setPatchMode(99);
        QCOMPARE(d.patchMode(), 1);
        d.setPatchMode(-1);
        QCOMPARE(d.patchMode(), 0);
        d.setPatchDiffusion(99);
        QCOMPARE(d.patchDiffusion(), 7);
        d.setPatchDiffusion(-1);
        QCOMPARE(d.patchDiffusion(), 1);
    }

    void patchUhdSmallDragStaysLocalAndPreviewIsBounded()
    {
        QTemporaryDir dir;
        QImage        image(3840, 2160, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(170, 170, 170));
        QPainter painter(&image);
        painter.fillRect(98, 98, 4, 4, Qt::black);
        painter.end();
        const QString path = dir.filePath("patch-uhd.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("select");
        d.beginStroke(90, 90);
        d.continueStroke(110, 110);
        d.endStroke();
        d.setTool("patch");
        const int     history   = d.historyIndex();
        const qint64  sourceKey = d.layerImage(0).cacheKey();
        QElapsedTimer timer;
        timer.start();
        d.beginStroke(100, 100);
        for (int x = 130; x < 180; ++x)
            d.continueStroke(x, 100);
        QCOMPARE(d.layerImage(0).cacheKey(), sourceKey);
        d.endStroke();
        qInfo() << "UHD Patch small repair (ms):" << timer.elapsed();
        QVERIFY(timer.elapsed() < 1500);
        QCOMPARE(d.historyIndex(), history + 1);
        QCOMPARE(d.layerImage(0).pixelColor(100, 100), QColor(170, 170, 170));
        QCOMPARE(d.layerImage(0).pixelColor(3000, 1800), QColor(170, 170, 170));
        d.setTool("select");
        d.beginStroke(0, 0);
        d.continueStroke(2500, 2000);
        d.endStroke();
        d.setTool("patch");
        d.beginStroke(1000, 1000);
        d.continueStroke(1100, 1000);
        QVERIFY(d.patchPreviewImage().width() <= 1000);
        QVERIFY(d.patchPreviewImage().height() <= 1000);
        d.cancelStroke();
    }

    void spotHealingAutomaticallyRepairsLocallyAndUndoes()
    {
        QTemporaryDir dir;
        QImage        image(160, 160, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < image.height(); ++y) {
            const int grey = y % 8 < 4 ? 170 : 180;
            for (int x = 0; x < image.width(); ++x)
                image.setPixel(x, y, qRgb(grey, grey, grey));
        }
        QPainter painter(&image);
        painter.fillRect(78, 78, 4, 4, Qt::black);
        painter.end();
        const QString path = dir.filePath("spot-texture.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("spotHealing");
        QCOMPARE(d.tool(), QString("spotHealing"));
        d.setBrushSize(24);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        const QImage original = d.layerImage(0);
        const int    history  = d.historyIndex();
        QCOMPARE(d.composite(), original);
        d.beginStroke(80, 80); // no source click
        d.endStroke();
        QVERIFY(!d.healingSourceValid());
        QCOMPARE(d.historyIndex(), history + 1);
        const QImage repaired = d.layerImage(0);
        QVERIFY(qAbs(repaired.pixelColor(80, 80).red() - 170) <= 2);
        QCOMPARE(d.composite().pixel(80, 80), repaired.pixel(80, 80));
        for (int y = 0; y < 160; ++y)
            for (int x = 0; x < 160; ++x)
                if (!QRect(66, 66, 28, 28).contains(x, y))
                    QCOMPARE(repaired.pixel(x, y), original.pixel(x, y));
        d.undo();
        QCOMPARE(d.layerImage(0), original);
        d.redo();
        QCOMPARE(d.layerImage(0), repaired);
        d.undo();
        d.beginStroke(80, 80);
        d.continueStroke(90, 80);
        d.cancelStroke();
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
    }

    void spotHealingRetouchesOnEmptyLayerAndHonoursVisibleLayers()
    {
        Document d;
        d.newDocument(160, 160, false);
        d.setColor(QColor(150, 170, 190));
        d.fillWithColor(true);
        d.setTool("brush");
        d.setBrushSize(4);
        d.setBrushHardness(1);
        d.setColor(Qt::black);
        d.beginStroke(80, 80);
        d.endStroke();
        const QImage background = d.layerImage(0);
        d.addLayer();
        const int    history = d.historyIndex();
        const QImage empty   = d.layerImage(1);
        d.setTool("spotHealing");
        d.setBrushSize(24);
        d.beginStroke(80, 80);
        d.endStroke();
        QCOMPARE(d.layerImage(1), empty);
        QCOMPARE(d.historyIndex(), history);
        d.setSpotHealingSampleAllLayers(true);
        d.beginStroke(80, 80);
        d.endStroke();
        QVERIFY(qAbs(d.layerImage(1).pixelColor(80, 80).red() - 150) <= 2);
        QCOMPARE(d.layerImage(0), background);
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        QCOMPARE(d.layerImage(1), empty);
        d.setActiveLayer(0);
        d.setLayerVisible(0, false);
        d.setActiveLayer(1);
        const int hiddenHistory = d.historyIndex();
        d.beginStroke(80, 80);
        d.endStroke();
        QCOMPARE(d.layerImage(1), empty);
        QCOMPARE(d.historyIndex(), hiddenHistory);
    }

    void spotHealingSelectionEdgesMasksLocksAndNoOps()
    {
        QTemporaryDir dir;
        QImage        image(160, 160, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(160, 160, 160));
        QPainter painter(&image);
        painter.fillRect(78, 78, 4, 4, Qt::black);
        painter.fillRect(0, 38, 3, 4, Qt::black);
        painter.end();
        const QString path = dir.filePath("spot-selection.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("select");
        d.beginStroke(78, 78);
        d.continueStroke(79, 81);
        d.endStroke();
        d.setTool("spotHealing");
        d.setBrushSize(24);
        d.setBrushHardness(1);
        d.beginStroke(80, 80);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(79, 80).red() > 100);
        QCOMPARE(d.layerImage(0).pixelColor(81, 80), QColor(Qt::black));
        d.undo();
        d.deselect();
        d.beginStroke(1, 40);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(1, 40).red() > 100);
        d.undo();
        const int history = d.historyIndex();
        d.beginStroke(120, 120);
        d.endStroke();
        QCOMPARE(d.historyIndex(), history);
        d.setBrushOpacity(0);
        d.beginStroke(80, 80);
        d.endStroke();
        QCOMPARE(d.historyIndex(), history);
        d.setBrushOpacity(1);
        d.setLayerLocked(0, true);
        const int lockedHistory = d.historyIndex();
        d.beginStroke(80, 80);
        d.endStroke();
        QCOMPARE(d.historyIndex(), lockedHistory);
        d.setLayerLocked(0, false);
        d.addLayerMask();
        d.setEditingMask(true);
        const QImage mask        = d.maskImage(0);
        const int    maskHistory = d.historyIndex();
        d.beginStroke(80, 80);
        d.endStroke();
        QCOMPARE(d.maskImage(0), mask);
        QCOMPARE(d.historyIndex(), maskHistory);
        d.setSpotHealingMode(99);
        QCOMPARE(d.spotHealingMode(), 7);
        d.setSpotHealingMode(-1);
        QCOMPARE(d.spotHealingMode(), 0);
    }

    void spotHealingUhdGestureStaysLocalWithOneUndo()
    {
        Document d;
        QVERIFY(d.newDocument(3840, 2160, false));
        d.setColor(QColor(160, 160, 160));
        d.fillWithColor(true);
        d.setTool("brush");
        d.setBrushSize(4);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        d.setColor(Qt::black);
        d.beginStroke(1920, 1080);
        d.continueStroke(1950, 1080);
        d.endStroke();
        const QImage original = d.layerImage(0);
        d.composite(); // the display is current before the gesture
        d.setTool("spotHealing");
        d.setBrushSize(24);
        const int     history = d.historyIndex();
        QElapsedTimer timer;
        timer.start();
        d.beginStroke(1920, 1080);
        d.continueStroke(1950, 1080);
        d.endStroke();
        const qint64 elapsed = timer.elapsed();
        qInfo("UHD Spot Healing gesture: %lld ms", static_cast<long long>(elapsed));
        QVERIFY2(elapsed < 1500, "A small repair must not scan the full UHD image per dab");
        QCOMPARE(d.historyIndex(), history + 1);
        QVERIFY(d.layerImage(0).pixelColor(1935, 1080).red() > 100);
        QCOMPARE(d.layerImage(0).pixel(100, 100), original.pixel(100, 100));
        QCOMPARE(d.layerImage(0).pixel(3000, 1500), original.pixel(3000, 1500));
        d.undo();
        QCOMPARE(d.layerImage(0), original);
    }

    void healingTransfersTextureAdaptsColourLocallyAndUndoes()
    {
        QTemporaryDir dir;
        QImage        image(128, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(40, 80, 110));
        QPainter painter(&image);
        painter.fillRect(64, 0, 64, 64, QColor(130, 150, 180));
        painter.fillRect(18, 30, 4, 4, QColor(65, 100, 130));
        painter.fillRect(78, 30, 4, 4, Qt::black);
        painter.end();
        const QString path = dir.filePath("healing-texture.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("healing");
        d.setBrushSize(24);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        const QImage original = d.layerImage(0);
        const int    history  = d.historyIndex();
        d.beginStroke(80, 32); // a source is required
        d.endStroke();
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        d.beginStroke(20, 32, true);
        d.continueStroke(25, 32);
        d.endStroke();
        QVERIFY(d.healingSourceValid());
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.composite(), original);
        d.beginStroke(80, 32);
        d.endStroke();
        const QColor healed = d.layerImage(0).pixelColor(80, 32);
        QVERIFY(qAbs(healed.red() - 155) <= 2);
        QVERIFY(qAbs(healed.green() - 170) <= 2);
        QVERIFY(qAbs(healed.blue() - 200) <= 2);
        QCOMPARE(d.composite().pixelColor(80, 32), healed);
        QCOMPARE(d.historyIndex(), history + 1);
        const QImage result = d.layerImage(0);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 128; ++x)
                if (!QRect(66, 18, 28, 28).contains(x, y))
                    QCOMPARE(result.pixel(x, y), original.pixel(x, y));
        d.undo();
        QCOMPARE(d.layerImage(0), original);
        d.redo();
        QCOMPARE(d.layerImage(0), result);
        d.undo();
        d.beginStroke(80, 32);
        d.continueStroke(90, 32);
        d.cancelStroke();
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        d.setHealingMode(1);
        d.beginStroke(80, 32);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(80, 32), QColor(65, 100, 130));
    }
    void healingAlignmentAndCloneSourceAreIndependent()
    {
        QTemporaryDir dir;
        QImage        image(128, 40, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 128; ++x)
                image.setPixelColor(x, y, QColor(x, x, x));
        const QString path = dir.filePath("healing-offset.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setCloneSource(8, 20);
        d.setTool("healing");
        d.setHealingMode(1);
        d.setBrushSize(6);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        d.setHealingSource(14, 20);
        d.beginStroke(60, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(60, 20), QColor(14, 14, 14));
        d.beginStroke(80, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(80, 20), QColor(14, 14, 14));
        QCOMPARE(d.cloneSource(), QPointF(8, 20));
        d.setHealingAligned(true);
        d.setHealingSource(14, 20);
        d.beginStroke(60, 20);
        d.endStroke();
        d.beginStroke(80, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(80, 20), QColor(34, 34, 34));
        QCOMPARE(d.healingOffset(), QPointF(-46, 0));
        d.setHealingSource(20, 20);
        d.beginStroke(100, 20);
        d.cancelStroke();
        d.beginStroke(110, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(110, 20), QColor(20, 20, 20));
        QVERIFY(d.newDocument(64, 40, false));
        QVERIFY(!d.healingSourceValid());
    }
    void healingSamplingLayersAndIgnoringAdjustments()
    {
        Document d;
        QVERIFY(d.newDocument(80, 40, true));
        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.addLayer();
        const int active = d.activeLayer();
        d.setTool("healing");
        d.setHealingMode(1);
        d.setBrushSize(8);
        d.setBrushHardness(1);
        d.setHealingSource(12, 20);
        const int history = d.historyIndex();
        d.beginStroke(50, 20);
        d.endStroke();
        QCOMPARE(d.historyIndex(), history); // current layer has no source pixels
        d.setHealingSampleMode(1);
        d.beginStroke(50, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(50, 20), QColor(Qt::red));
        d.undo();
        d.addLayer();
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        d.setActiveLayer(active);
        d.beginStroke(50, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(50, 20), QColor(Qt::red));
        d.undo();
        d.setHealingSampleMode(2);
        d.beginStroke(50, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(50, 20), QColor(Qt::blue));
        d.undo();
        d.setActiveLayer(2);
        d.addAdjustmentLayer("invert");
        d.setActiveLayer(active);
        d.beginStroke(50, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(50, 20), QColor(Qt::yellow));
        d.undo();
        d.setHealingIgnoreAdjustments(true);
        d.beginStroke(50, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(50, 20), QColor(Qt::blue));
        d.undo();
        d.setLayerVisible(2, false);
        d.beginStroke(50, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(50, 20), QColor(Qt::red));
    }
    void healingClipsSelectionsRejectsMasksAndDoesNotRecordNoOps()
    {
        Document d;
        QVERIFY(d.newDocument(80, 48, false));
        d.setTool("healing");
        d.setHealingSource(15, 24);
        const int history = d.historyIndex();
        d.beginStroke(55, 24);
        d.endStroke();
        QCOMPARE(d.historyIndex(), history); // identical texture and tone
        d.setHealingMode(1);
        d.setTool("brush");
        d.setColor(Qt::black);
        d.setBrushSize(8);
        d.setBrushHardness(1);
        d.beginStroke(55, 24);
        d.endStroke();
        const QImage original = d.layerImage(0);
        d.setTool("ellipticalSelect");
        d.beginStroke(51, 20);
        d.continueStroke(59, 28);
        d.endStroke();
        const QRegion selection = d.selectionRegion();
        d.setTool("healing");
        d.setBrushSize(30);
        d.beginStroke(55, 24);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(55, 24), QColor(Qt::white));
        const QImage result = d.layerImage(0);
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 80; ++x)
                if (!selection.contains(QPoint(x, y)))
                    QCOMPARE(result.pixel(x, y), original.pixel(x, y));
        d.undo();
        d.deselect();
        d.setBrushOpacity(0);
        const int noOpHistory = d.historyIndex();
        d.beginStroke(55, 24);
        d.endStroke();
        QCOMPARE(d.historyIndex(), noOpHistory);
        d.setBrushOpacity(1);
        d.setLayerLocked(0, true);
        const int lockedHistory = d.historyIndex();
        d.beginStroke(55, 24);
        d.endStroke();
        QCOMPARE(d.historyIndex(), lockedHistory);
        d.setLayerLocked(0, false);
        d.addLayerMask();
        d.setEditingMask(true);
        const QImage mask        = d.maskImage(0);
        const int    maskHistory = d.historyIndex();
        d.beginStroke(55, 24);
        d.endStroke();
        QCOMPARE(d.historyIndex(), maskHistory);
        QCOMPARE(d.maskImage(0), mask);
        d.setHealingDiffusion(20);
        QCOMPARE(d.healingDiffusion(), 7);
        d.setHealingDiffusion(-1);
        QCOMPARE(d.healingDiffusion(), 1);
    }
    void redEyeClickCorrectsOnePupilPreservesHighlightsAndUndo()
    {
        QTemporaryDir dir;
        QImage        image(160, 100, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(210, 165, 140));
        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(230, 40, 35));
        painter.drawEllipse(QRect(32, 32, 20, 20));
        painter.drawEllipse(QRect(58, 36, 4, 4)); // separate red object in the search area
        painter.drawEllipse(QRect(100, 32, 20, 20));
        painter.setBrush(Qt::white);
        painter.drawEllipse(QRect(40, 38, 4, 4));
        painter.end();
        const QString path = dir.filePath("red-eyes.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("redEye");
        QCOMPARE(d.tool(), QString("redEye"));
        const QImage original = d.layerImage(0);
        const int    history  = d.historyIndex();
        QCOMPARE(d.composite(), original); // prime the full-resolution cache before the local edit
        d.beginStroke(42, 40);             // clicking a catchlight finds the surrounding red pupil
        QCOMPARE(d.layerImage(0), original); // modification happens at release
        d.endStroke();
        QCOMPARE(d.historyIndex(), history + 1);
        const QColor corrected = d.layerImage(0).pixelColor(38, 42);
        QCOMPARE(corrected.red(), corrected.green());
        QCOMPARE(corrected.green(), corrected.blue());
        QVERIFY(corrected.red() < 40);
        QCOMPARE(d.layerImage(0).pixelColor(42, 40), QColor(Qt::white));
        QCOMPARE(d.layerImage(0).pixelColor(60, 38), original.pixelColor(60, 38));
        QCOMPARE(d.layerImage(0).pixelColor(110, 42), original.pixelColor(110, 42));
        const QImage result = d.layerImage(0);
        for (int y = 0; y < original.height(); ++y)
            for (int x = 0; x < original.width(); ++x)
                if (!QRect(32, 32, 20, 20).contains(x, y))
                    QCOMPARE(result.pixel(x, y), original.pixel(x, y));
        QCOMPARE(d.composite().pixelColor(38, 42), corrected);
        d.undo();
        QCOMPARE(d.layerImage(0), original);
        d.redo();
        QCOMPARE(d.layerImage(0), result);
    }
    void redEyeBoxControlsSizeAndDarkeningAndCancelsWithoutEditing()
    {
        QTemporaryDir dir;
        QImage        image(100, 100, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(220, 80, 60));
        painter.drawEllipse(QRect(30, 30, 40, 40));
        painter.end();
        const QString path = dir.filePath("pupil.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("redEye");
        const QImage original = d.layerImage(0);
        const int    history  = d.historyIndex();
        d.beginStroke(25, 25);
        d.continueStroke(75, 75);
        QCOMPARE(d.redEyeRegion(), QRectF(25, 25, 50, 50));
        d.cancelStroke();
        QVERIFY(d.redEyeRegion().isEmpty());
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        d.setRedEyePupilSize(.1);
        d.setRedEyeDarken(0);
        d.beginStroke(25, 25);
        d.continueStroke(75, 75);
        d.endStroke();
        QVERIFY(d.redEyeRegion().isEmpty());
        QCOMPARE(d.layerImage(0).pixelColor(50, 50), QColor(70, 70, 70));
        QCOMPARE(d.layerImage(0).pixelColor(37, 50), original.pixelColor(37, 50));
        d.undo();
        d.setRedEyePupilSize(1);
        d.setRedEyeDarken(1);
        d.beginStroke(75, 75); // either drag direction works
        d.continueStroke(25, 25);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(50, 50), QColor(7, 7, 7));
        QVERIFY(d.layerImage(0).pixelColor(37, 50).red() < 20);
        QCOMPARE(d.historyIndex(), history + 1);
    }
    void redEyeHonoursSelectionAlphaAndRejectsMasksAndLockedLayers()
    {
        QTemporaryDir dir;
        QImage        image(64, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(230, 60, 40, 128));
        painter.drawEllipse(QRect(16, 16, 32, 32));
        painter.end();
        const QString path = dir.filePath("alpha-eye.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const QImage original = d.layerImage(0);
        d.setTool("ellipticalSelect");
        d.beginStroke(24, 24);
        d.continueStroke(40, 40);
        d.endStroke();
        const QRegion selection = d.selectionRegion();
        d.setTool("redEye");
        d.beginStroke(32, 32);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(32, 32).red() < 60);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                QCOMPARE(qAlpha(d.layerImage(0).pixel(x, y)), qAlpha(original.pixel(x, y)));
                if (!selection.contains(QPoint(x, y)))
                    QCOMPARE(d.layerImage(0).pixel(x, y), original.pixel(x, y));
            }
        d.undo();
        d.deselect();
        d.setLayerLocked(0, true);
        const int lockedHistory = d.historyIndex();
        d.beginStroke(32, 32);
        d.endStroke();
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), lockedHistory);
        d.setLayerLocked(0, false);
        d.addLayerMask();
        d.setEditingMask(true);
        const QImage mask        = d.maskImage(0);
        const int    maskHistory = d.historyIndex();
        d.beginStroke(32, 32);
        d.endStroke();
        QCOMPARE(d.maskImage(0), mask);
        QCOMPARE(d.historyIndex(), maskHistory);
        QCOMPARE(d.layerImage(0), original);
    }
    void redEyeNoOpClicksKeepHistoryAndValidateSettings()
    {
        Document d;
        QVERIFY(d.newDocument(64, 64, false));
        d.setTool("redEye");
        const int    history  = d.historyIndex();
        const QImage original = d.layerImage(0);
        for (const QPointF point : {QPointF(32, 32), QPointF(-20, -20), QPointF(500, 500)}) {
            d.beginStroke(point.x(), point.y());
            d.endStroke();
        }
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.layerImage(0), original);
        d.setRedEyePupilSize(-1);
        QCOMPARE(d.redEyePupilSize(), .01);
        d.setRedEyePupilSize(5);
        QCOMPARE(d.redEyePupilSize(), 1.);
        d.setRedEyeDarken(-2);
        QCOMPARE(d.redEyeDarken(), 0.);
        d.setRedEyeDarken(2);
        QCOMPARE(d.redEyeDarken(), 1.);
        d.setRedEyeDarken(qQNaN());
        d.setRedEyePupilSize(qInf());
        QCOMPARE(d.redEyeDarken(), 1.);
        QCOMPARE(d.redEyePupilSize(), 1.);
    }
    void mixerDryPaintFlowAndReservoirDepletion()
    {
        Document d;
        QVERIFY(d.newDocument(256, 40, true));
        d.setTool("mixerBrush");
        QCOMPARE(d.tool(), QString("mixerBrush"));
        d.setColor(Qt::red);
        d.setBrushSize(8);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        d.setMixerWet(0);
        d.setMixerMix(1);
        d.setMixerAutoLoad(false);
        d.setMixerAutoClean(false);
        d.setMixerLoad(.01);
        const int    history  = d.historyIndex();
        const QImage original = d.layerImage(0);
        d.beginStroke(12, 20);
        d.continueStroke(180, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(12, 20), QColor(Qt::red));
        QCOMPARE(d.layerImage(0).pixelColor(140, 20).alpha(), 0);
        QVERIFY(!d.mixerLoaded());
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        QCOMPARE(d.layerImage(0), original);
        d.setMixerLoad(1);
        d.beginStroke(12, 20);
        d.continueStroke(180, 20);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(140, 20).alpha() > 200);
        QCOMPARE(d.layerImage(0).pixelColor(140, 8).alpha(), 0);
        QCOMPARE(d.layerImage(0).pixelColor(210, 20).alpha(), 0);
        d.undo();
        d.setMixerFlow(.5);
        d.loadMixerBrush();
        d.beginStroke(40, 20);
        d.endStroke();
        const QColor partial = d.layerImage(0).pixelColor(40, 20);
        QVERIFY(partial.alpha() >= 126 && partial.alpha() <= 128);
        QCOMPARE(partial.red(), 255);
        d.undo();
        d.setMixerFlow(0);
        d.beginStroke(40, 20);
        d.continueStroke(100, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        d.setMixerFlow(1);
        d.loadMixerBrush();
        d.beginStroke(40, 20);
        d.continueStroke(80, 20);
        d.cancelStroke();
        QCOMPARE(d.layerImage(0), original);
        QVERIFY(d.mixerLoaded());
    }
    void mixerWetPickupAndMixRatio()
    {
        Document d;
        QVERIFY(d.newDocument(96, 40, true));
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        d.setTool("mixerBrush");
        d.setColor(Qt::red);
        d.setBrushSize(10);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        d.setMixerWet(1);
        d.setMixerLoad(1);
        d.setMixerMix(0);
        d.beginStroke(48, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(48, 20), QColor(Qt::red));
        d.undo();
        d.setMixerMix(.5);
        d.beginStroke(48, 20);
        d.endStroke();
        const QColor mixed = d.layerImage(0).pixelColor(48, 20);
        QVERIFY(mixed.red() >= 126 && mixed.red() <= 129);
        QVERIFY(mixed.blue() >= 126 && mixed.blue() <= 129);
        QCOMPARE(mixed.green(), 0);
        d.undo();
        const int history = d.historyIndex();
        d.setMixerMix(1);
        d.beginStroke(48, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(48, 20), QColor(Qt::blue));
        QCOMPARE(d.historyIndex(), history);
        d.setTool("select");
        d.beginStroke(0, 0);
        d.continueStroke(31, 39);
        d.endStroke();
        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.deselect();
        const QImage original = d.layerImage(0);
        d.setTool("mixerBrush");
        d.setColor(Qt::green);
        d.cleanMixerBrush();
        d.beginStroke(24, 20);
        d.continueStroke(48, 20);
        d.endStroke();
        const QColor streak = d.layerImage(0).pixelColor(36, 20);
        QVERIFY(streak.red() > 0 && streak.blue() > 0 && streak.green() == 0);
        QCOMPARE(d.layerImage(0).pixelColor(36, 2), QColor(Qt::blue));
        d.undo();
        QCOMPARE(d.layerImage(0), original);
    }
    void mixerAltLoadsMulticolourOrSolidPaintWithoutDrawing()
    {
        QTemporaryDir dir;
        QImage        image(80, 32, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::blue);
        QPainter painter(&image);
        painter.fillRect(0, 0, 20, 32, Qt::red);
        painter.end();
        const QString path = dir.filePath("mixer-sample.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const QImage original = d.layerImage(0);
        d.setTool("mixerBrush");
        d.setBrushSize(12);
        d.setBrushHardness(1);
        d.setColor(Qt::green);
        d.setMixerWet(0);
        d.setMixerLoad(1);
        const int history = d.historyIndex();
        d.beginStroke(20, 16, true);
        d.continueStroke(40, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.color(), QColor(Qt::green));
        QVERIFY(d.mixerLoadColor().red() > 0 && d.mixerLoadColor().blue() > 0);
        d.addLayer();
        const int active = d.activeLayer();
        d.beginStroke(56, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(53, 16), QColor(Qt::red));
        QCOMPARE(d.layerImage(active).pixelColor(59, 16), QColor(Qt::blue));
        d.undo();
        d.setActiveLayer(0);
        d.setMixerSolidOnly(true);
        d.beginStroke(20, 16, true);
        d.endStroke();
        QCOMPARE(d.mixerLoadColor(), QColor(Qt::blue));
        d.setActiveLayer(active);
        d.beginStroke(56, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(53, 16), QColor(Qt::blue));
        QCOMPARE(d.layerImage(active).pixelColor(59, 16), QColor(Qt::blue));
        d.undo();
        d.setMixerAutoLoad(false);
        d.setMixerAutoClean(true);
        d.loadMixerBrush();
        d.beginStroke(56, 16);
        d.endStroke();
        QVERIFY(!d.mixerLoaded());
        d.loadMixerBrush();
        QVERIFY(d.mixerLoaded());
        QCOMPARE(d.mixerLoadColor(), QColor(Qt::blue));
        d.setMixerAutoLoad(true);
        d.beginStroke(68, 16);
        d.endStroke();
        QVERIFY(d.mixerLoaded());
    }
    void mixerSamplesLayersClipsSelectionsAndPaintsMasks()
    {
        Document d;
        QVERIFY(d.newDocument(80, 40, true));
        d.setColor(Qt::yellow);
        d.fillWithColor(true);
        const QImage background = d.layerImage(0);
        d.addLayer();
        const int active = d.activeLayer();
        d.setTool("mixerBrush");
        d.setBrushSize(12);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        d.setMixerWet(1);
        d.setMixerMix(1);
        d.cleanMixerBrush();
        d.beginStroke(20, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(20, 20).alpha(), 0);
        d.setMixerSampleAllLayers(true);
        d.beginStroke(20, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(20, 20), QColor(Qt::yellow));
        QCOMPARE(d.layerImage(0), background);
        d.setTool("ellipticalSelect");
        d.beginStroke(36, 6);
        d.continueStroke(54, 30);
        d.endStroke();
        d.setTool("mixerBrush");
        d.setMixerWet(0);
        d.setMixerLoad(1);
        d.setColor(Qt::red);
        d.beginStroke(44, 20);
        d.continueStroke(56, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(44, 20), QColor(Qt::red));
        QCOMPARE(d.layerImage(active).pixelColor(36, 6).alpha(), 0);
        QCOMPARE(d.layerImage(active).pixelColor(56, 20).alpha(), 0);
        d.deselect();
        d.addLayerMask();
        d.setEditingMask(true);
        const QImage pixels = d.layerImage(active);
        d.setColor(Qt::black);
        d.beginStroke(20, 20);
        d.continueStroke(32, 20);
        d.endStroke();
        QCOMPARE(d.maskImage(active).pixelColor(20, 20), QColor(Qt::black));
        QCOMPARE(d.maskImage(active).pixelColor(64, 20), QColor(Qt::white));
        QCOMPARE(d.layerImage(active), pixels);
        d.undo();
        QCOMPARE(d.maskImage(active).pixelColor(20, 20), QColor(Qt::white));
    }
    void mixerShiftClickPaintsStraightLineAndPresetsAreEditable()
    {
        Document d;
        QVERIFY(d.newDocument(96, 40, true));
        d.setTool("mixerBrush");
        d.applyMixerPreset(1);
        QCOMPARE(d.mixerPreset(), 1);
        d.setBrushSize(6);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        d.setColor(Qt::red);
        d.beginStroke(12, 20);
        d.endStroke();
        const int history = d.historyIndex();
        d.beginStroke(80, 20, false, true);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(48, 20).red() > 250);
        QCOMPARE(d.layerImage(0).pixelColor(48, 30).alpha(), 0);
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        QCOMPARE(d.layerImage(0).pixelColor(48, 20).alpha(), 0);
        d.applyMixerPreset(4);
        QCOMPARE(d.mixerWet(), 1.);
        QCOMPARE(d.mixerMix(), 1.);
        QCOMPARE(d.mixerPreset(), 4);
        d.setMixerWet(.75);
        QCOMPARE(d.mixerPreset(), 0);
    }
    void colorReplacementSamplingUndoAndLocality()
    {
        QTemporaryDir dir;
        QImage        image(80, 40, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::blue);
        QPainter painter(&image);
        painter.fillRect(0, 0, 30, 40, Qt::red);
        painter.end();
        const QString path = dir.filePath("replacement.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const QImage original = d.layerImage(0);
        d.setTool("colorReplacement");
        QCOMPARE(d.tool(), QString("colorReplacement"));
        d.setBrushSize(8);
        d.setBrushSmoothing(0);
        d.setBrushHardness(1);
        d.setColor(Qt::green);
        d.setReplacementTolerance(0);
        d.setReplacementSampling(1);
        const int history = d.historyIndex();
        d.beginStroke(20, 20);
        d.continueStroke(50, 20);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(20, 20).green() > 100);
        QCOMPARE(d.layerImage(0).pixelColor(45, 20), QColor(Qt::blue));
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 80; ++x)
                if (x < 15 || x > 55 || y < 15 || y > 25)
                    QCOMPARE(d.layerImage(0).pixelColor(x, y), image.pixelColor(x, y));
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        QCOMPARE(d.layerImage(0), original);
        d.redo();
        QVERIFY(d.layerImage(0).pixelColor(20, 20).green() > 100);
        d.undo();
        d.setReplacementSampling(0);
        d.beginStroke(20, 20);
        d.continueStroke(50, 20);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(45, 20).green() > 0);
        d.undo();
        d.beginStroke(20, 20);
        d.continueStroke(50, 20);
        d.cancelStroke();
        QCOMPARE(d.layerImage(0), original);
        d.setReplacementSampling(2);
        d.setReplacementLimits(1);
        d.setSecondaryColor(Qt::blue);
        d.beginStroke(20, 20);
        d.continueStroke(50, 20);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(20, 20), QColor(Qt::red));
        QVERIFY(d.layerImage(0).pixelColor(45, 20).green() > 0);
        const int beforePick = d.historyIndex();
        d.beginStroke(2, 2, true);
        d.continueStroke(60, 20);
        d.endStroke();
        QCOMPARE(d.color(), QColor(Qt::red));
        QCOMPARE(d.historyIndex(), beforePick);
    }
    void colorReplacementLimitsToleranceAndEdges()
    {
        QTemporaryDir dir;
        QImage        image(40, 24, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::red);
        QPainter painter(&image);
        painter.fillRect(19, 0, 2, 24, Qt::blue);
        painter.end();
        const QString path = dir.filePath("islands.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("colorReplacement");
        d.setBrushSize(40);
        d.setBrushHardness(1);
        d.setColor(Qt::green);
        d.setReplacementTolerance(0);
        d.beginStroke(16, 12);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(16, 12).green() > 100);
        QCOMPARE(d.layerImage(0).pixelColor(25, 12), QColor(Qt::red));
        QCOMPARE(d.layerImage(0).pixelColor(20, 12), QColor(Qt::blue));
        d.undo();
        d.setReplacementLimits(1);
        d.beginStroke(16, 12);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(25, 12).green() > 100);
        QCOMPARE(d.layerImage(0).pixelColor(20, 12), QColor(Qt::blue));
        d.undo();
        d.setReplacementLimits(0);
        d.setReplacementTolerance(100);
        d.beginStroke(16, 12);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(20, 12).green() > 0);
        d.undo();
        d.setReplacementLimits(2);
        d.beginStroke(16, 12);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(20, 12), QColor(Qt::blue));
        QCOMPARE(d.layerImage(0).pixelColor(25, 12), QColor(Qt::red));
    }
    void colorReplacementAntialiasAndUnchangedStroke()
    {
        Document d;
        QVERIFY(d.newDocument(32, 24, true));
        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.setTool("colorReplacement");
        d.setBrushSize(8);
        d.setBrushHardness(1);
        d.setColor(Qt::green);
        d.setReplacementAntialias(false);
        d.beginStroke(16.25, 12.25);
        d.endStroke();
        const auto mixedPixels = [](const QImage &_image) {
            int count = 0;
            for (int y = 0; y < _image.height(); ++y)
                for (int x = 0; x < _image.width(); ++x) {
                    const QColor c = _image.pixelColor(x, y);
                    if (c.red() > 0 && c.green() > 0)
                        ++count;
                }
            return count;
        };
        QCOMPARE(mixedPixels(d.layerImage(0)), 0);
        d.undo();
        d.setReplacementAntialias(true);
        d.beginStroke(16.25, 12.25);
        d.endStroke();
        QVERIFY(mixedPixels(d.layerImage(0)) > 0);
        d.undo();
        d.setColor(Qt::red);
        const int history = d.historyIndex();
        d.beginStroke(16, 12);
        d.endStroke();
        QCOMPARE(d.historyIndex(), history);
    }
    void colorReplacementModesPreserveAlphaAndLuminosity()
    {
        QTemporaryDir dir;
        QImage        image(32, 24, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(200, 80, 50, 180));
        image.setPixelColor(16, 11, Qt::transparent);
        const QString path = dir.filePath("modes.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("colorReplacement");
        d.setBrushSize(16);
        d.setBrushHardness(1);
        d.setReplacementLimits(1);
        d.setColor(QColor(40, 170, 100));
        const QColor original = d.layerImage(0).pixelColor(16, 12);
        const auto   lum      = [](const QColor &_c) {
            return .3 * _c.red() + .59 * _c.green() + .11 * _c.blue();
        };
        for (int mode = 0; mode < 4; ++mode) {
            d.setReplacementMode(mode);
            d.beginStroke(16, 12);
            d.endStroke();
            const QColor result = d.layerImage(0).pixelColor(16, 12);
            QCOMPARE(result.alpha(), original.alpha());
            QCOMPARE(d.layerImage(0).pixelColor(16, 11).alpha(), 0);
            QVERIFY(result != original);
            const qreal expected = mode == 3 ? lum(d.color()) : lum(original);
            QVERIFY(qAbs(lum(result) - expected) < 2);
            if (mode == 2) {
                const QColor ink      = d.color();
                const int    range    = qMax(result.red(), qMax(result.green(), result.blue())) -
                                        qMin(result.red(), qMin(result.green(), result.blue()));
                const int    inkRange = qMax(ink.red(), qMax(ink.green(), ink.blue())) -
                                        qMin(ink.red(), qMin(ink.green(), ink.blue()));
                QVERIFY(qAbs(range - inkRange) < 3);
            }
            d.undo();
        }
        d.setTool("ellipticalSelect");
        d.beginStroke(10, 6);
        d.continueStroke(22, 18);
        d.endStroke();
        d.setTool("colorReplacement");
        d.setReplacementMode(0);
        d.setBrushSize(30);
        d.beginStroke(16, 12);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(16, 12) != original);
        QCOMPARE(d.layerImage(0).pixelColor(10, 6), image.pixelColor(10, 6));
    }
    void smudgeTransportsPaintOnlyAlongTheStrokeAndRemainsUndoable()
    {
        Document d;
        QVERIFY(d.newDocument(96, 48, true));
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        d.setTool("select");
        d.beginStroke(0, 0);
        d.continueStroke(31, 47);
        d.endStroke();
        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.deselect();
        d.setTool("smudge");
        d.setBrushSize(12);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        d.setToolStrength(1);
        d.setColor(Qt::green);
        const QImage original = d.layerImage(0);
        const int    history  = d.historyIndex();
        d.beginStroke(24, 24);
        d.continueStroke(48, 24);
        d.continueStroke(60, 24);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(48, 24), QColor(Qt::red));
        QCOMPARE(d.layerImage(0).pixelColor(48, 4), QColor(Qt::blue));
        QCOMPARE(d.layerImage(0).pixelColor(84, 24), QColor(Qt::blue));
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        QCOMPARE(d.layerImage(0), original);
        d.redo();
        QCOMPARE(d.layerImage(0).pixelColor(48, 24), QColor(Qt::red));
        d.undo();
        d.setToolStrength(.5);
        d.beginStroke(28, 24);
        d.continueStroke(36, 24);
        d.endStroke();
        const QColor mixed = d.layerImage(0).pixelColor(34, 24);
        QVERIFY(mixed.red() > 0 && mixed.blue() > 0 && mixed.green() == 0);
        d.undo();
        d.beginStroke(24, 24);
        d.continueStroke(48, 24);
        d.cancelStroke();
        QCOMPARE(d.layerImage(0), original);
    }
    void smudgeFingerPaintingAndAltIntroduceForegroundColour()
    {
        Document d;
        QVERIFY(d.newDocument(64, 32, true));
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        d.setTool("smudge");
        d.setBrushSize(10);
        d.setBrushHardness(1);
        d.setToolStrength(1);
        d.setColor(Qt::red);
        d.beginStroke(16, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(16, 16), QColor(Qt::blue));
        d.beginStroke(16, 16, true);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(16, 16), QColor(Qt::red));
        QVERIFY(!d.smudgeFingerPainting());
        d.undo();
        d.setSmudgeFingerPainting(true);
        d.beginStroke(32, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(32, 16), QColor(Qt::red));
    }
    void smudgeSamplesVisibleLayersAndRespectsSelectionsAndMasks()
    {
        Document d;
        QVERIFY(d.newDocument(64, 32, true));
        d.setColor(Qt::yellow);
        d.fillWithColor(true);
        const QImage background = d.layerImage(0);
        d.addLayer();
        const int active = d.activeLayer();
        d.setTool("smudge");
        d.setBrushSize(10);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        d.setToolStrength(1);
        d.beginStroke(16, 16);
        d.continueStroke(32, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(32, 16).alpha(), 0);
        d.setSmudgeSampleAllLayers(true);
        d.beginStroke(16, 16);
        d.continueStroke(32, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(32, 16), QColor(Qt::yellow));
        QCOMPARE(d.layerImage(0), background);
        d.setTool("ellipticalSelect");
        d.beginStroke(36, 6);
        d.continueStroke(54, 26);
        d.endStroke();
        d.setTool("smudge");
        d.setColor(Qt::red);
        d.beginStroke(44, 16, true);
        d.continueStroke(54, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(active).pixelColor(44, 16), QColor(Qt::red));
        QCOMPARE(d.layerImage(active).pixelColor(56, 16).alpha(), 0);
        QCOMPARE(d.layerImage(active).pixelColor(36, 6).alpha(), 0);
        d.deselect();
        d.addLayerMask();
        d.setEditingMask(true);
        d.setColor(Qt::black);
        d.beginStroke(16, 16, true);
        d.continueStroke(32, 16);
        d.endStroke();
        QCOMPARE(d.maskImage(active).pixelColor(24, 16), QColor(Qt::black));
        QCOMPARE(d.maskImage(active).pixelColor(48, 16), QColor(Qt::white));
        d.undo();
        QCOMPARE(d.maskImage(active).pixelColor(24, 16), QColor(Qt::white));
    }
    void cloneStampKeepsSourceOffsetAndClipsToSelection()
    {
        QTemporaryDir dir;
        QImage        source(64, 32, QImage::Format_ARGB32);
        for (int y = 0; y < source.height(); ++y)
            for (int x = 0; x < source.width(); ++x)
                source.setPixelColor(x, y, QColor(x * 4, y * 8, 80));
        const QString path = dir.filePath("source.png");
        QVERIFY(source.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("cloneStamp");
        d.setBrushSize(8);
        d.setBrushHardness(1);
        d.setBrushOpacity(1);
        d.setBrushFlow(1);
        const int    before   = d.historyIndex();
        const QImage original = d.layerImage(0);
        d.beginStroke(40, 16);
        d.endStroke();
        QCOMPARE(d.historyIndex(), before);
        QCOMPARE(d.layerImage(0), original);
        d.setCloneSource(8, 16);
        QCOMPARE(d.historyIndex(), before);
        d.beginStroke(40, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(40, 16), source.pixelColor(8, 16));
        QCOMPARE(d.layerImage(0).pixelColor(60, 16), source.pixelColor(60, 16));
        d.beginStroke(48, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(48, 16), source.pixelColor(16, 16));
        d.setStampAligned(false);
        d.beginStroke(56, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(56, 16), source.pixelColor(8, 16));
        d.undo();
        QCOMPARE(d.layerImage(0).pixelColor(56, 16), source.pixelColor(56, 16));
        d.setTool("select");
        d.beginStroke(38, 14);
        d.continueStroke(42, 18);
        d.endStroke();
        const QImage beforeClip = d.layerImage(0);
        d.setTool("cloneStamp");
        d.setCloneSource(20, 16);
        d.beginStroke(40, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(40, 16), source.pixelColor(20, 16));
        QCOMPARE(d.layerImage(0).pixelColor(36, 16), beforeClip.pixelColor(36, 16));
        d.beginStroke(41, 16);
        d.cancelStroke();
        QCOMPARE(d.layerImage(0).pixelColor(40, 16), source.pixelColor(20, 16));
    }
    void cloneStampSamplesBelowAndAllLayersAndMasks()
    {
        Document d;
        QVERIFY(d.newDocument(48, 32, true));
        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.addLayer();
        const int destination = d.activeLayer();
        d.addLayer();
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        d.setActiveLayer(destination);
        d.setTool("cloneStamp");
        d.setBrushSize(8);
        d.setBrushHardness(1);
        d.setBrushFlow(1);
        d.setCloneSource(8, 16);
        d.setCloneSampleMode(1);
        d.beginStroke(24, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(destination).pixelColor(24, 16), QColor(Qt::red));
        d.setCloneSampleMode(2);
        d.beginStroke(36, 16);
        d.endStroke();
        QCOMPARE(d.layerImage(destination).pixelColor(36, 16), QColor(Qt::blue));
        d.addLayerMask();
        d.setEditingMask(true);
        d.setTool("brush");
        d.setColor(Qt::black);
        d.beginStroke(8, 16);
        d.endStroke();
        d.setTool("cloneStamp");
        d.setCloneSource(8, 16);
        d.beginStroke(24, 16);
        d.endStroke();
        QCOMPARE(d.maskImage(destination).pixelColor(24, 16), QColor(Qt::black));
        QCOMPARE(d.maskImage(destination).pixelColor(44, 16), QColor(Qt::white));
        d.undo();
        QCOMPARE(d.maskImage(destination).pixelColor(24, 16), QColor(Qt::white));
    }
    void patternStampRepeatsAndDefinesCustomPatterns()
    {
        Document d;
        QVERIFY(d.newDocument(80, 32, true));
        d.setTool("patternStamp");
        d.setBrushSize(8);
        d.setBrushHardness(1);
        d.setBrushFlow(1);
        d.beginStroke(8, 8);
        d.endStroke();
        const QColor first = d.layerImage(0).pixelColor(8, 8);
        d.beginStroke(32, 8);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(32, 8), first);
        d.beginStroke(44, 8);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(44, 8) != first);
        d.setStampAligned(false);
        d.beginStroke(56, 8);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(56, 8), first);
        d.undo();
        QCOMPARE(d.layerImage(0).pixelColor(56, 8).alpha(), 0);
        QVERIFY(!d.defineStampPattern());
        d.setTool("select");
        d.beginStroke(2, 20);
        d.continueStroke(5, 23);
        d.endStroke();
        d.setColor(Qt::cyan);
        d.fillWithColor(true);
        QVERIFY(d.defineStampPattern());
        QVERIFY(d.hasCustomStampPattern());
        QCOMPARE(d.stampPattern(), 3);
        d.deselect();
        d.setTool("patternStamp");
        d.beginStroke(64, 24);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(64, 24), QColor(Qt::cyan));
        QCOMPARE(d.layerImage(0).pixelColor(76, 24).alpha(), 0);
    }
    void photoshopSelectionGesturesAndModifiers()
    {
        Document d;
        QVERIFY(d.newDocument(100, 100, true));
        d.setTool("select");
        d.beginStroke(10, 10);
        d.continueStroke(30, 30);
        d.endStroke();
        const QRegion original = d.selectionRegion();
        const int     revision = d.revision();
        d.setSelectionMode(1);
        d.beginStroke(50, 50);
        d.continueStroke(70, 70);
        d.endStroke();
        QVERIFY(d.selectionRegion().contains(QPoint(20, 20)));
        QVERIFY(d.selectionRegion().contains(QPoint(60, 60)));
        d.setSelectionMode(2);
        d.beginStroke(5, 5);
        d.continueStroke(40, 40);
        d.endStroke();
        QVERIFY(!d.selectionRegion().contains(QPoint(20, 20)));
        QVERIFY(d.selectionRegion().contains(QPoint(60, 60)));
        const QRegion beforeCancel = d.selectionRegion();
        d.setSelectionMode(0);
        d.beginStroke(80, 80);
        d.continueStroke(95, 95);
        d.cancelStroke();
        QCOMPARE(d.selectionRegion(), beforeCancel);
        d.beginStroke(60, 60);
        d.continueStroke(70, 65);
        d.endStroke();
        QVERIFY(d.selectionRegion().contains(QPoint(80, 70)));
        QVERIFY(!d.selectionRegion().contains(QPoint(51, 51)));
        QCOMPARE(d.revision(), revision);
        d.deselect();
        d.beginStroke(50, 50);
        d.setToolModifiers(true, true, false);
        d.continueStroke(60, 70);
        QCOMPARE(d.selection().width(), d.selection().height());
        QCOMPARE(d.selection().center(), QPoint(50, 50));
        const QSize size = d.selection().size();
        d.setToolModifiers(true, true, true);
        d.continueStroke(65, 75);
        QCOMPARE(d.selection().size(), size);
        QCOMPARE(d.selection().center(), QPoint(55, 55));
        d.endStroke();
        d.nudgeTool(0, -10);
        QCOMPARE(d.selection().center(), QPoint(55, 45));
        d.setSelectionMode(3);
        d.beginStroke(40, 40);
        d.continueStroke(60, 60);
        d.endStroke();
        QVERIFY(d.selectionRegion().contains(QPoint(50, 45)));
        QVERIFY(!d.selectionRegion().contains(QPoint(70, 45)));
        d.setSelectionMode(0);
        d.deselect();
        d.setTool("singleRowSelect");
        d.beginStroke(10, 10);
        d.continueStroke(80, 40);
        d.endStroke();
        QCOMPARE(d.selection(), QRect(0, 40, 100, 1));
        QVERIFY(!original.isEmpty());
    }
    void photoshopLassoMixedGesturesPreserveTheOriginalSelection()
    {
        Document d;
        QVERIFY(d.newDocument(100, 100, true));
        d.setTool("select");
        d.beginStroke(5, 5);
        d.continueStroke(20, 20);
        d.endStroke();
        const QRegion original = d.selectionRegion();
        d.setTool("lasso");
        d.setSelectionMode(1);
        d.beginStroke(50, 50);
        d.continueStroke(80, 50);
        d.continueStroke(65, 80);
        d.endStroke();
        QVERIFY(d.selectionRegion().contains(QPoint(10, 10)));
        QVERIFY(d.selectionRegion().contains(QPoint(65, 60)));
        const QRegion combined = d.selectionRegion();
        d.beginStroke(30, 30);
        d.continueStroke(45, 30);
        d.setToolModifiers(false, true, false);
        d.endStroke();
        QVERIFY(d.polygonLassoActive());
        d.addPolygonLassoPoint(45, 45);
        d.cancelPolygonLasso();
        QCOMPARE(d.selectionRegion(), combined);
        d.setTool("polygonalLasso");
        d.addPolygonLassoPoint(30, 30);
        d.setToolModifiers(true, false, false);
        d.addPolygonLassoPoint(50, 33);
        const auto path = d.selectionPath();
        QCOMPARE(path[0].toPointF().y(), path[1].toPointF().y());
        d.setToolModifiers(false, true, false);
        d.continuePolygonLasso(60, 50);
        d.continuePolygonLasso(40, 60);
        d.finishPolygonLasso();
        QVERIFY(d.selectionRegion().contains(QPoint(10, 10)));
        QVERIFY(d.selectionRegion().contains(QPoint(45, 45)));
        QVERIFY(!original.isEmpty());
    }
    void photoshopPaintAndShapeGestures()
    {
        Document d;
        QVERIFY(d.newDocument(100, 100, true));
        d.setBrushSize(3);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        d.setColor(Qt::red);
        d.setTool("brush");
        d.beginStroke(10, 10);
        d.endStroke();
        d.beginStroke(40, 40, false, true);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(25, 25).alpha() > 0);
        QCOMPARE(d.layerImage(0).pixelColor(25, 10).alpha(), 0);
        d.undo();
        QCOMPARE(d.layerImage(0).pixelColor(25, 25).alpha(), 0);
        d.beginStroke(10, 60);
        d.setToolModifiers(true, false, false);
        d.continueStroke(40, 67);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(30, 60).alpha() > 0);
        QCOMPARE(d.layerImage(0).pixelColor(30, 66).alpha(), 0);
        d.setTool("rectangle");
        d.setShapeStrokeEnabled(false);
        d.setShapeFillColor(Qt::blue);
        d.beginStroke(60, 60, true, true);
        d.continueStroke(70, 80);
        d.endStroke();
        QCOMPARE(d.layerImage(0).pixelColor(41, 41), QColor(Qt::blue));
        QCOMPARE(d.layerImage(0).pixelColor(79, 79), QColor(Qt::blue));
        QCOMPARE(d.layerImage(0).pixelColor(39, 39).alpha(), 0);
        d.undo();
        d.setTool("move");
        d.beginStroke(0, 0, true);
        d.continueStroke(10, 0);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(10, 10).alpha() > 0);
        QCOMPARE(d.layers().size(), 2);
        QVERIFY(d.layerImage(1).pixelColor(20, 10).alpha() > 0);
        d.undo();
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.layerImage(0).pixelColor(20, 10).alpha(), 0);
    }
    void photoshopCropSliceAndRetouchModifiers()
    {
        Document d;
        QVERIFY(d.newDocument(100, 100, true));
        d.setTool("crop");
        d.setToolModifiers(true, true, false);
        d.beginCropGesture(50, 50);
        d.updateCropGesture(60, 70);
        d.endCropGesture();
        QCOMPARE(d.cropPoints().at(0).toMap().value("x").toReal(), 30.);
        QCOMPARE(d.cropPoints().at(2).toMap().value("x").toReal(), 70.);
        d.cancelCrop();
        d.setTool("slice");
        d.setToolModifiers(false, false, false);
        d.beginSliceGesture(50, 50);
        d.setToolModifiers(true, true, false);
        d.updateSliceGesture(60, 70);
        d.endSliceGesture();
        QCOMPARE(d.slices().first().toMap().value("width").toInt(), 40);
        QCOMPARE(d.slices().first().toMap().value("height").toInt(), 40);
        d.setTool("dodge");
        d.setColor(QColor(128, 128, 128));
        d.fillWithColor(true);
        d.setBrushSize(20);
        d.setToolStrength(1);
        d.beginStroke(50, 50, true);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(50, 50).red() < 128);
        d.undo();
        d.beginStroke(50, 50);
        d.endStroke();
        QVERIFY(d.layerImage(0).pixelColor(50, 50).red() > 128);
    }
    void lassoSelectionsFollowFreehandAndPolygonalContours()
    {
        Document d;
        QVERIFY(d.newDocument(36, 36, true));
        d.setColor(Qt::red);
        d.setTool("lasso");
        d.beginStroke(4, 4);
        d.continueStroke(31, 5);
        d.continueStroke(18, 31);
        d.endStroke();
        QCOMPARE(d.selectionShape(), 4);
        QVERIFY(d.hasSelection());
        QVERIFY(d.selectionPath().size() >= 3);
        d.fillWithColor(true);
        QCOMPARE(d.composite().pixelColor(18, 12), QColor(Qt::red));
        QCOMPARE(d.composite().pixelColor(3, 31).alpha(), 0);

        d.deselect();
        d.setColor(Qt::blue);
        d.setTool("polygonalLasso");
        d.addPolygonLassoPoint(5, 5);
        d.addPolygonLassoPoint(30, 5);
        d.addPolygonLassoPoint(30, 30);
        QVERIFY(d.polygonLassoActive());
        QCOMPARE(d.selectionPath().size(), 3);
        d.removeLastPolygonLassoPoint();
        QCOMPARE(d.selectionPath().size(), 2);
        d.addPolygonLassoPoint(30, 30);
        d.addPolygonLassoPoint(5, 30);
        d.finishPolygonLasso();
        QVERIFY(!d.polygonLassoActive());
        QCOMPARE(d.selectionShape(), 5);
        QVERIFY(d.hasSelection());
        d.fillWithColor(true);
        QCOMPARE(d.composite().pixelColor(18, 18), QColor(Qt::blue));
        QCOMPARE(d.composite().pixelColor(2, 2).alpha(), 0);
    }
    void localRetouchToolsUseBrushCoverageAndRemainUndoable()
    {
        Document d;
        QVERIFY(d.newDocument(48, 48, true));
        d.setColor(QColor(128, 128, 128));
        d.fillWithColor(true);
        d.setBrushSize(18);
        d.setBrushOpacity(1);
        d.setBrushFlow(1);
        d.setToolStrength(1);

        d.setTool("dodge");
        d.beginStroke(24, 24);
        d.endStroke();
        QVERIFY(d.composite().pixelColor(24, 24).red() > 128);
        d.undo();
        QCOMPARE(d.composite().pixelColor(24, 24).red(), 128);

        d.setTool("burn");
        d.beginStroke(24, 24);
        d.endStroke();
        QVERIFY(d.composite().pixelColor(24, 24).red() < 128);
        d.undo();

        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.setSpongeSaturate(false);
        d.setTool("sponge");
        d.beginStroke(24, 24);
        d.endStroke();
        QVERIFY(d.composite().pixelColor(24, 24).green() > 0);

        d.undo();
        d.setColor(Qt::black);
        d.setTool("pencil");
        d.setBrushSize(8);
        d.beginStroke(24, 24);
        d.endStroke();
        QCOMPARE(d.composite().pixelColor(24, 24), QColor(Qt::black));
        const QColor untouched = d.composite().pixelColor(4, 4);
        d.setBrushSize(20);
        d.setTool("blur");
        d.beginStroke(24, 24);
        d.endStroke();
        QVERIFY(d.composite().pixelColor(27, 24).red() > 0);
        QCOMPARE(d.composite().pixelColor(4, 4), untouched);
        d.undo();
        QCOMPARE(d.composite().pixelColor(27, 24), QColor(Qt::black));
    }
    void brushHardnessFlowOpacityAndSpacingFollowDabs()
    {
        Document soft;
        QVERIFY(soft.newDocument(64, 64, true));
        soft.setColor(Qt::black);
        soft.setBrushSize(20);
        soft.setBrushHardness(0);
        soft.beginStroke(32, 32);
        soft.endStroke();
        QVERIFY(soft.composite().pixelColor(32, 32).alpha() > 225);
        const int softEdge = soft.composite().pixelColor(40, 32).alpha();
        QVERIFY(softEdge > 0 && softEdge < 180);

        Document flow;
        QVERIFY(flow.newDocument(64, 64, true));
        flow.setColor(Qt::black);
        flow.setBrushSize(20);
        flow.setBrushFlow(.25);
        flow.beginStroke(32, 32);
        flow.endStroke();
        const int oneDab = flow.composite().pixelColor(32, 32).alpha();
        QVERIFY(oneDab >= 60 && oneDab <= 70);
        flow.undo();
        flow.setBrushSpacing(.01);
        flow.setBrushSmoothing(0);
        flow.beginStroke(30, 32);
        flow.continueStroke(34, 32);
        flow.endStroke();
        QVERIFY(flow.composite().pixelColor(32, 32).alpha() > oneDab);

        Document capped;
        QVERIFY(capped.newDocument(64, 64, true));
        capped.setColor(Qt::black);
        capped.setBrushSize(20);
        capped.setBrushOpacity(.5);
        capped.setBrushFlow(1);
        capped.setBrushSpacing(.01);
        capped.setBrushSmoothing(0);
        capped.beginStroke(30, 32);
        capped.continueStroke(34, 32);
        capped.endStroke();
        const int cappedAlpha = capped.composite().pixelColor(32, 32).alpha();
        QVERIFY(cappedAlpha >= 125 && cappedAlpha <= 130);

        Document spaced;
        QVERIFY(spaced.newDocument(64, 32, true));
        spaced.setColor(Qt::black);
        spaced.setBrushSize(10);
        spaced.setBrushSpacing(2);
        spaced.setBrushSmoothing(0);
        spaced.beginStroke(10, 16);
        spaced.continueStroke(50, 16);
        spaced.endStroke();
        QCOMPARE(spaced.composite().pixelColor(20, 16).alpha(), 0);
        QCOMPARE(spaced.composite().pixelColor(30, 16).alpha(), 255);

        Document mode;
        QVERIFY(mode.newDocument(32, 32));
        mode.setTool("fill");
        mode.setColor(Qt::green);
        mode.beginStroke(0, 0);
        mode.setTool("brush");
        mode.setColor(Qt::red);
        mode.setBrushSize(12);
        mode.setBrushBlend(3);
        mode.beginStroke(16, 16);
        mode.endStroke();
        const QColor multiplied = mode.composite().pixelColor(16, 16);
        QVERIFY(multiplied.red() < 5 && multiplied.green() < 5 && multiplied.blue() < 5);
    }
    void layerOpacityVisibilityAndOrder()
    {
        Document d;
        d.newDocument(16, 16);
        d.addLayer();
        d.setTool("fill");
        d.setColor(Qt::red);
        d.beginStroke(0, 0);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor(Qt::red));
        d.setLayerOpacity(1, .5);
        const auto c = d.composite().pixelColor(8, 8);
        QCOMPARE(c.red(), 255);
        QVERIFY(qAbs(c.green() - 128) <= 1);
        d.setLayerVisible(1, false);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor(Qt::white));
        d.setLayerVisible(1, true);
        d.moveLayer(-1);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor(Qt::white));
        d.undo();
        QCOMPARE(d.activeLayer(), 1);
    }
    void altClickIsolationRestoresPreviousLayerVisibility()
    {
        Document d;
        QVERIFY(d.newDocument(16, 16));
        d.addLayer();
        d.addLayer();
        d.setLayerVisible(1, false);
        d.toggleLayerIsolation(2);
        for (const QVariant &entry : d.layers()) {
            const QVariantMap layer = entry.toMap();
            QCOMPARE(layer.value("visible").toBool(), layer.value("layerIndex").toInt() == 2);
        }

        d.toggleLayerIsolation(2);
        for (const QVariant &entry : d.layers()) {
            const QVariantMap layer = entry.toMap();
            const int         index = layer.value("layerIndex").toInt();
            QCOMPARE(layer.value("visible").toBool(), index != 1);
        }
    }
    void clippingMasksRestrictRenderingAndPersist()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32, true));
        d.setColor(Qt::black);
        d.setBrushSize(12);
        d.beginStroke(8, 16);
        d.endStroke();
        QVERIFY(d.composite().pixelColor(8, 16).alpha() > 0);
        QCOMPARE(d.composite().pixelColor(28, 16).alpha(), 0);

        d.addLayer();
        d.setTool("fill");
        d.setColor(Qt::red);
        d.beginStroke(0, 0);
        QCOMPARE(d.composite().pixelColor(28, 16), QColor(Qt::red));

        d.setClippingMask(true);
        QVERIFY(d.layers().first().toMap().value("clippingMask").toBool());
        QCOMPARE(d.composite().pixelColor(8, 16), QColor(Qt::red));
        QCOMPARE(d.composite().pixelColor(28, 16).alpha(), 0);
        d.undo();
        QCOMPARE(d.composite().pixelColor(28, 16), QColor(Qt::red));
        d.redo();
        QCOMPARE(d.composite().pixelColor(28, 16).alpha(), 0);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QUrl url = QUrl::fromLocalFile(dir.filePath("clipping-mask.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QVERIFY(loaded.layers().first().toMap().value("clippingMask").toBool());
        QCOMPARE(loaded.composite(), d.composite());
    }
    void multipleLayerSelectionCreatesPersistentPassThroughGroup()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Document d;
        QVERIFY(d.newDocument(24, 24));
        d.addLayer();
        d.addLayer();
        d.selectLayer(0);
        d.selectLayer(2, true);
        QCOMPARE(d.selectedLayerCount(), 2);
        QVERIFY(d.canGroupSelected());
        const QImage before = d.composite();

        d.groupSelectedLayers();
        QCOMPARE(d.layers().size(), 4);
        QCOMPARE(d.layers().first().toMap().value("kind").toString(), QString("group"));
        QVERIFY(d.canUngroupActive());
        QCOMPARE(d.composite(), before);
        const QString groupId = d.layers().first().toMap().value("groupId").toString();
        QVERIFY(!groupId.isEmpty());
        QCOMPARE(d.layers()[1].toMap().value("groupId").toString(), groupId);
        QCOMPARE(d.layers()[2].toMap().value("groupId").toString(), groupId);

        d.setLayerVisible(d.activeLayer(), false);
        QCOMPARE(d.composite().pixelColor(0, 0).alpha(), 0);
        d.undo();
        QCOMPARE(d.composite(), before);

        d.toggleGroupCollapsed(d.activeLayer());
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.layers().first().toMap().value("kind").toString(), QString("group"));
        const auto url = QUrl::fromLocalFile(dir.filePath("grouped.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.layers().first().toMap().value("kind").toString(), QString("group"));
        QCOMPARE(loaded.layers().size(), 2);
        QCOMPARE(loaded.composite(), before);
        loaded.toggleGroupCollapsed(loaded.activeLayer());
        QCOMPARE(loaded.layers().size(), 4);
        loaded.ungroupActiveLayers();
        QCOMPARE(loaded.layers().size(), 3);
        QVERIFY(!loaded.canUngroupActive());
        QCOMPARE(loaded.composite(), before);
    }
    void lockPreventsPixelChanges()
    {
        Document d;
        d.newDocument(32, 32);
        d.setLayerLocked(0, true);
        const QImage before  = d.composite();
        const int    history = d.historyIndex();
        d.beginStroke(10, 10);
        d.endStroke();
        d.applyFilter("invert");
        d.clearSelected();
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.historyIndex(), history);
    }
    void cropFrameRatiosAndUndoPreservePixelsAndMasks()
    {
        Document d;
        d.newDocument(80, 60, true);
        d.setColor(QColor(40, 100, 220));
        d.fillWithColor(true);
        d.addLayerMask(true);
        const QImage original = d.composite();
        const int    history  = d.historyIndex();
        d.setTool("crop");
        d.setCropRatio(1);
        d.beginCropGesture(10, 10, 0);
        d.updateCropGesture(50, 30);
        d.endCropGesture();
        QVERIFY(d.hasCrop());
        QVERIFY(!d.hasSelection());
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.cropPoints()[2].toMap()["x"].toReal(), 50.);
        QCOMPARE(d.cropPoints()[2].toMap()["y"].toReal(), 50.);
        d.beginCropGesture(30, 30, 0);
        d.updateCropGesture(35, 35);
        d.endCropGesture();
        QCOMPARE(d.cropPoints()[0].toMap()["x"].toReal(), 15.);
        d.setCropOutputSize(QSize(20, 20));
        d.applyCrop();
        QVERIFY(d.cropBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!d.cropBusy(), 5000);
        QCOMPARE(d.width(), 20);
        QCOMPARE(d.height(), 20);
        QCOMPARE(d.composite().pixelColor(10, 10), original.pixelColor(35, 35));
        QVERIFY(d.layers().first().toMap()["hasMask"].toBool());
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        QCOMPARE(d.width(), 80);
        QCOMPARE(d.height(), 60);
        QCOMPARE(d.composite(), original);
        d.redo();
        QCOMPARE(d.width(), 20);
    }
    void perspectiveCropMapsAQuadAndRejectsCrossedCorners()
    {
        QTemporaryDir dir;
        QImage        image(80, 60, QImage::Format_ARGB32);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                image.setPixelColor(x, y, QColor(x * 3, y * 4, 0));
        QVERIFY(image.save(dir.filePath("perspective.png")));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(dir.filePath("perspective.png"))));
        d.setTool("perspectiveCrop");
        QCOMPARE(d.tool(), QString("perspectiveCrop"));
        d.beginStroke(10, 10);
        d.continueStroke(70, 50);
        d.endStroke();
        d.setCropCorner(0, 20, 10);
        d.setCropCorner(1, 60, 10);
        const QVariantList valid = d.cropPoints();
        d.setCropCorner(0, 80, 60);
        QCOMPARE(d.cropPoints(), valid);
        d.setCropCorner(0, std::numeric_limits<double>::quiet_NaN(), 5);
        QCOMPARE(d.cropPoints(), valid);
        d.setCropOutputSize(QSize(40, 30));
        const QImage before = d.composite();
        d.applyCrop();
        QTRY_VERIFY_WITH_TIMEOUT(!d.cropBusy(), 5000);
        QCOMPARE(d.width(), 40);
        QCOMPARE(d.height(), 30);
        const QImage cropped = d.composite();
        QVERIFY(qAbs(cropped.pixelColor(0, 0).red() - 60) <= 6);
        QVERIFY(qAbs(cropped.pixelColor(0, 0).green() - 40) <= 6);
        QVERIFY(qAbs(cropped.pixelColor(39, 29).red() - 210) <= 10);
        QVERIFY(qAbs(cropped.pixelColor(0, 29).red() - 30) <= 10);
        QVERIFY(qAbs(cropped.pixelColor(0, 29).green() - 200) <= 10);
        d.undo();
        QCOMPARE(d.composite(), before);
        d.resetCrop();
        d.cancelCrop();
        QVERIFY(!d.hasCrop());
    }
    void cropWorkerCannotReplaceANewerDocument()
    {
        Document d;
        d.newDocument(100, 80);
        d.setTool("perspectiveCrop");
        d.beginStroke(10, 10);
        d.continueStroke(90, 70);
        d.endStroke();
        d.applyCrop();
        d.newDocument(32, 24, true);
        QTRY_VERIFY_WITH_TIMEOUT(!d.cropBusy(), 5000);
        QCOMPARE(d.width(), 32);
        QCOMPARE(d.height(), 24);
        QCOMPARE(d.composite().pixelColor(10, 10).alpha(), 0);
        QCOMPARE(d.historyIndex(), 0);
    }
    void slicesAreMetadataWithCancelableGesturesDivisionAndExport()
    {
        QTemporaryDir dir;
        Document      d;
        d.newDocument(81, 61);
        const QImage original = d.composite();
        const int    revision = d.revision();
        d.setTool("slice");
        d.beginStroke(5, 5);
        d.continueStroke(46, 36);
        d.endStroke();
        QCOMPARE(d.slices().size(), 1);
        QCOMPARE(d.revision(), revision);
        QCOMPARE(d.composite(), original);
        d.setTool("sliceSelect");
        d.beginSliceGesture(20, 20, 0);
        d.updateSliceGesture(25, 25);
        d.endSliceGesture(true);
        QCOMPARE(d.slices().first().toMap()["x"].toInt(), 5);
        d.beginSliceGesture(20, 20, 0);
        d.updateSliceGesture(25, 25);
        d.endSliceGesture();
        QCOMPARE(d.slices().first().toMap()["x"].toInt(), 10);
        d.divideActiveSlice(2, 2);
        QCOMPARE(d.slices().size(), 4);
        int     area = 0;
        QRegion covered;
        for (const auto &value : d.slices()) {
            const auto  slice = value.toMap();
            const QRect rect(slice["x"].toInt(), slice["y"].toInt(), slice["width"].toInt(),
                             slice["height"].toInt());
            QVERIFY((covered & QRegion(rect)).isEmpty());
            covered += rect;
            area += rect.width() * rect.height();
        }
        QCOMPARE(area, 41 * 31);
        d.setSliceShowAuto(true);
        QVERIFY(d.slices().size() > 4);
        d.setSliceShowAuto(false);
        const auto project = QUrl::fromLocalFile(dir.filePath("sliced.pcr"));
        QVERIFY(d.saveProject(project));
        Document restored;
        QVERIFY(restored.openFile(project));
        QCOMPARE(restored.slices(), d.slices());
        d.exportSlices(QUrl::fromLocalFile(dir.path()));
        QTRY_VERIFY_WITH_TIMEOUT(!d.sliceBusy(), 5000);
        QImage exported(dir.filePath("tranche_01.png"));
        QCOMPARE(exported.size(), QSize(20, 15));
        QCOMPARE(exported.pixelColor(0, 0), QColor(Qt::white));
        d.exportSlices(QUrl::fromLocalFile(dir.path()));
        QTRY_VERIFY_WITH_TIMEOUT(!d.sliceBusy(), 5000);
        QVERIFY(QFile::exists(dir.filePath("tranche_01_1.png")));
        d.undo();
        QCOMPARE(d.slices().size(), 1);
        d.deleteActiveSlice();
        QCOMPARE(d.slices().size(), 0);
        d.undo();
        QCOMPARE(d.slices().size(), 1);
    }
    void selectionClipsPaintingAndCropping()
    {
        Document d;
        d.newDocument(32, 32, true);
        d.setTool("select");
        d.beginStroke(8, 8);
        d.continueStroke(15, 15);
        d.endStroke();
        QCOMPARE(d.selection(), QRect(8, 8, 8, 8));
        d.setTool("brush");
        d.setColor(Qt::green);
        d.setBrushSize(50);
        d.beginStroke(12, 12);
        d.endStroke();
        QCOMPARE(d.composite().pixelColor(0, 0).alpha(), 0);
        QCOMPARE(d.composite().pixelColor(12, 12), QColor(Qt::green));
        d.cropToSelection();
        QCOMPARE(d.width(), 8);
        QCOMPARE(d.height(), 8);
        QVERIFY(!d.hasSelection());
        d.undo();
        QCOMPARE(d.width(), 32);
        QCOMPARE(d.selection(), QRect(8, 8, 8, 8));
    }
    void projectRoundTripAndExport()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Document d;
        d.newDocument(24, 18, true);
        d.addLayer();
        d.setTool("fill");
        d.setColor(QColor(60, 140, 220, 180));
        d.beginStroke(0, 0);
        d.renameLayer(1, "Bleu");
        d.setLayerOpacity(1, .7);
        d.setLayerBlend(1, 2);
        d.setLayerLocked(1, true);
        auto url = QUrl::fromLocalFile(dir.filePath("test.pcr"));
        QVERIFY(d.saveProject(url));
        QVERIFY(!d.modified());
        const QImage expected = d.composite();
        Document     loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.composite(), expected);
        QCOMPARE(loaded.layers(), d.layers());
        QCOMPARE(loaded.activeLayer(), 1);
        QVERIFY(!loaded.modified());
        QVERIFY(loaded.exportImage(QUrl::fromLocalFile(dir.filePath("export.png"))));
        QCOMPARE(
            QImage(dir.filePath("export.png")).convertToFormat(QImage::Format_ARGB32_Premultiplied),
            expected);
        QVERIFY(loaded.exportImage(QUrl::fromLocalFile(dir.filePath("export.jpg"))));
        QVERIFY(!QImage(dir.filePath("export.jpg")).hasAlphaChannel());
        d.setLayerLocked(1, false);
        QVERIFY(d.modified());
        d.undo();
        QVERIFY(!d.modified());
    }
    void malformedProjectPreservesDocument()
    {
        QTemporaryDir dir;
        QFile         file(dir.filePath("broken.pcr"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"format\":\"PhotoCrap\",\"version\":1,\"width\":32,\"height\":32,\"layers\":["
                   "{\"png\":\"nonsense\"}]}");
        file.close();
        Document d;
        d.newDocument(8, 8);
        const QImage before = d.composite();
        QVERIFY(!d.openFile(QUrl::fromLocalFile(file.fileName())));
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.width(), 8);
        QVERIFY(!d.newDocument(20000, 100));
        QCOMPARE(d.width(), 8);
        QVERIFY(!d.newDocument(-1, 100));
    }
    void shapePreviewIsSingleUndoOperation()
    {
        Document d;
        d.newDocument(64, 64, true);
        d.setTool("rectangle");
        d.setColor(Qt::blue);
        d.setBrushSize(2);
        d.beginStroke(5, 5);
        d.continueStroke(25, 25);
        d.continueStroke(45, 45);
        d.endStroke();
        QCOMPARE(d.historyIndex(), 1);
        QCOMPARE(d.composite().pixelColor(40, 40), QColor(Qt::blue));
        d.undo();
        QCOMPARE(d.composite().pixelColor(40, 40).alpha(), 0);
        d.redo();
        d.setTool("eraser");
        d.setBrushSize(10);
        d.beginStroke(20, 20);
        d.endStroke();
        QCOMPARE(d.composite().pixelColor(20, 20).alpha(), 0);
    }
    void rectangleUsesIndependentFillStrokeAndCorners()
    {
        Document d;
        d.newDocument(64, 64, true);
        d.setTool("rectangle");
        d.setShapeFillColor(Qt::red);
        d.setShapeStrokeColor(Qt::blue);
        d.setShapeStrokeWidth(4);
        d.setShapeCornerRadius(8);
        d.setShapeStrokeStyle(1);
        d.setShapeAlignPixels(true);
        d.beginStroke(8.2, 8.2);
        d.continueStroke(48.4, 48.4);
        d.endStroke();

        QCOMPARE(d.shapeFillColor(), QColor(Qt::red));
        QCOMPARE(d.shapeStrokeColor(), QColor(Qt::blue));
        QCOMPARE(d.shapeStrokeWidth(), 4.);
        QCOMPARE(d.shapeCornerRadius(), 8.);
        QCOMPARE(d.shapeStrokeStyle(), 1);
        QCOMPARE(d.composite().pixelColor(28, 28), QColor(Qt::red));
        QVERIFY(d.composite().pixelColor(8, 8).alpha() < 32);

        d.setFilledShapes(false);
        d.setShapeStrokeEnabled(false);
        d.beginStroke(12, 12);
        d.continueStroke(36, 36);
        d.endStroke();
        QVERIFY(!d.filledShapes());
        QVERIFY(!d.shapeStrokeEnabled());
    }
    void lineUsesShapeStrokeOptions()
    {
        Document d;
        d.newDocument(64, 64, true);
        d.setTool("line");
        d.setShapeStrokeEnabled(true);
        d.setShapeStrokeColor(Qt::green);
        d.setShapeStrokeWidth(3);
        d.setShapeStrokeStyle(0);
        d.setShapeAlignPixels(true);
        d.beginStroke(5.3, 24.3);
        d.continueStroke(54.4, 24.4);
        d.endStroke();

        const QColor center = d.composite().pixelColor(30, 24);
        QVERIFY(center.green() > 180);
        QVERIFY(center.red() < 80);
        QCOMPARE(d.historyIndex(), 1);
    }
    void fillIsContiguousAndHandlesPartialAlpha()
    {
        Document d;
        d.newDocument(24, 24);
        d.setColor(Qt::black);
        d.setBrushSize(2);
        d.beginStroke(12, 0);
        d.continueStroke(12, 24);
        d.endStroke();
        d.setTool("fill");
        d.setColor(Qt::red);
        d.setBrushOpacity(.5);
        d.beginStroke(0, 0);
        QVERIFY(d.composite().pixelColor(3, 3).green() < 200);
        QCOMPARE(d.composite().pixelColor(20, 3), QColor(Qt::white));
    }
    void transformsFiltersAndMerge()
    {
        Document d;
        d.newDocument(12, 8);
        d.addLayer();
        d.setTool("fill");
        d.setColor(Qt::red);
        d.beginStroke(0, 0);
        d.applyFilter("invert");
        QCOMPARE(d.composite().pixelColor(0, 0), QColor(Qt::cyan));
        d.applyFilter("grayscale");
        auto c = d.composite().pixelColor(0, 0);
        QCOMPARE(c.red(), c.green());
        d.applyFilter("blur", 2);
        d.applyFilter("sharpen");
        d.transformImage("rotateRight");
        QCOMPARE(d.width(), 8);
        QCOMPARE(d.height(), 12);
        d.resizeImage(20, 30);
        QCOMPARE(d.width(), 20);
        QCOMPARE(d.height(), 30);
        QImage before = d.composite();
        d.mergeDown();
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.composite(), before);
        d.undo();
        QCOMPARE(d.layers().size(), 2);
    }
    void layerMasksCanBePaintedAndToggled()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.addLayerMask(false);
        QVERIFY(d.editingMask());
        QCOMPARE(d.composite().pixelColor(16, 16).alpha(), 0);
        d.invertLayerMask();
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
        d.setTool("brush");
        d.setBrushSize(8);
        d.beginStroke(16, 16);
        d.endStroke();
        QCOMPARE(d.maskImage(0).pixelColor(16, 16).red(), 0);
        QCOMPARE(d.composite().pixelColor(16, 16).alpha(), 0);
        d.beginLayerPreview();
        d.previewLayerParameter("_maskDensity", 0);
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
        d.cancelLayerPreview();
        QCOMPARE(d.composite().pixelColor(16, 16).alpha(), 0);
        d.setMaskEnabled(false);
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
        d.undo();
        QCOMPARE(d.composite().pixelColor(16, 16).alpha(), 0);
        d.undo();
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
    }
    void adjustmentLayersRenderAndPreviewAsOneEdit()
    {
        Document d;
        QVERIFY(d.newDocument(12, 12));
        d.addAdjustmentLayer("invert");
        QCOMPARE(d.layers().size(), 2);
        QVERIFY(!d.maskImage(1).isNull());
        QCOMPARE(d.composite().pixelColor(4, 4), QColor(Qt::black));
        d.undo();
        QCOMPARE(d.layers().size(), 1);

        d.addAdjustmentLayer("brightnessContrast");
        const int before = d.historyIndex();
        d.beginLayerPreview();
        d.previewLayerParameter("brightness", -20);
        d.previewLayerParameter("brightness", -40);
        QCOMPARE(d.historyIndex(), before);
        QVERIFY(d.composite().pixelColor(4, 4).red() < 200);
        d.finishLayerPreview("Luminosité");
        QCOMPARE(d.historyIndex(), before + 1);
        d.undo();
        QCOMPARE(d.composite().pixelColor(4, 4), QColor(Qt::white));
    }
    void layerPropertiesSessionCanCancelOrKeepSeveralEdits()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.addAdjustmentLayer("brightnessContrast");
        const int historyBefore = d.historyIndex();

        d.beginLayerPropertiesSession();
        d.beginLayerPreview();
        d.previewLayerParameter("brightness", -60);
        d.finishLayerPreview("Luminosité");
        QVERIFY(d.composite().pixelColor(4, 4).red() < 150);
        d.cancelLayerPropertiesSession();
        QCOMPARE(d.composite().pixelColor(4, 4), QColor(Qt::white));
        QCOMPARE(d.historyIndex(), historyBefore);

        d.beginLayerPropertiesSession();
        d.beginLayerPreview();
        d.previewLayerParameter("brightness", -30);
        d.finishLayerPreview("Luminosité");
        d.acceptLayerPropertiesSession();
        QVERIFY(d.composite().pixelColor(4, 4).red() < 220);
        QCOMPARE(d.historyIndex(), historyBefore + 1);
    }
    void sharedAdjustmentResetRestoresDefaultsAndIsUndoable()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.addAdjustmentLayer("light");
        d.beginLayerPreview();
        d.previewLayerParameter("exposure", -2.);
        d.finishLayerPreview("Exposition");
        QVERIFY(d.composite().pixelColor(4, 4).red() < 100);

        d.resetAdjustmentLayer();
        const QVariantMap parameters = d.layers().first().toMap().value("parameters").toMap();
        QCOMPARE(parameters.value("exposure").toDouble(), 0.);
        QCOMPARE(parameters.value("contrast").toInt(), 0);
        QCOMPARE(parameters.value("_maskDensity").toDouble(), 100.);
        QCOMPARE(d.composite().pixelColor(4, 4), QColor(Qt::white));

        d.undo();
        QVERIFY(d.composite().pixelColor(4, 4).red() < 100);
    }
    void everyAdjustmentTypeProducesAComposite()
    {
        const QStringList types = {
            "light",      "color",       "solidColor",   "brightnessContrast", "levels",
            "curves",     "exposure",    "vibrance",     "hueSaturation",      "colorBalance",
            "blackWhite", "photoFilter", "channelMixer", "colorLookup",        "invert",
            "posterize",  "threshold",   "gradientMap",  "selectiveColor",     "gaussianBlur",
            "motionBlur", "boxBlur",     "unsharpMask",  "highPass",           "mosaic",
            "noise"};
        Document d;
        QVERIFY(d.newDocument(4, 4));
        for (const auto &type : types) {
            const int count = d.layers().size();
            d.addAdjustmentLayer(type);
            QCOMPARE(d.layers().size(), count + 1);
            QVERIFY(!d.composite().isNull());
            d.undo();
        }
    }
    void colorLookupProvidesNamedStylesAndAdjustableStrength()
    {
        Document d;
        QVERIFY(d.newDocument(16, 16, true));
        const QColor sourceColor("#5276a4");
        d.setColor(sourceColor);
        d.setTool("fill");
        d.beginStroke(8, 8);
        d.endStroke();
        const QImage source = d.composite();

        d.addAdjustmentLayer("colorLookup");
        const QVariantMap parameters = d.layers().first().toMap().value("parameters").toMap();
        QCOMPARE(d.layers().first().toMap().value("name").toString(),
                 QString("Style colorimétrique (LUT)"));
        QCOMPARE(parameters.value("strength").toInt(), 100);
        QVERIFY(!parameters.value("preserveLuminosity").toBool());
        QVERIFY(d.composite() != source);

        d.beginLayerPreview();
        d.previewLayerParameter("strength", 0);
        QCOMPARE(d.composite(), source);
        d.previewLayerParameter("preset", 4);
        d.previewLayerParameter("strength", 100);
        const QImage tealAndOrange = d.composite();
        QVERIFY(tealAndOrange != source);
        d.finishLayerPreview("Modifier le style colorimétrique");
        QCOMPARE(d.layers().first().toMap().value("parameters").toMap().value("preset").toInt(), 4);
    }
    void selectiveColorTargetsNineFamiliesIndependently()
    {
        QImage source(2, 1, QImage::Format_ARGB32_Premultiplied);
        source.setPixelColor(0, 0, QColor(190, 52, 38));
        source.setPixelColor(1, 0, QColor(38, 72, 190));
        QVariantMap  parameters = PhotoCrap::Filters::defaultSelectiveColorParameters();
        const QImage unchanged  = PhotoCrap::Filters::applySelectiveColor(source, parameters);
        QCOMPARE(unchanged.pixelColor(0, 0), source.pixelColor(0, 0));
        QCOMPARE(unchanged.pixelColor(1, 0), source.pixelColor(1, 0));

        parameters["relative"]    = false;
        parameters["redsCyanRed"] = -60;
        const QImage adjusted     = PhotoCrap::Filters::applySelectiveColor(source, parameters);
        QVERIFY(adjusted.pixelColor(0, 0).red() < source.pixelColor(0, 0).red());
        QCOMPARE(adjusted.pixelColor(1, 0), source.pixelColor(1, 0));

        Document d;
        QVERIFY(d.newDocument(8, 8));
        d.addAdjustmentLayer("selectiveColor");
        const QVariantMap layerParameters = d.layers().first().toMap().value("parameters").toMap();
        QVERIFY(layerParameters.value("relative").toBool());
        QCOMPARE(layerParameters.value("magentasBlack").toInt(), 0);
        QCOMPARE(layerParameters.value("neutralsYellowBlue").toInt(), 0);
        QCOMPARE(layerParameters.value("whitesCyanRed").toInt(), 0);
    }
    void posterizeUsesTheRequestedNumberOfLevels()
    {
        QImage source(3, 1, QImage::Format_ARGB32_Premultiplied);
        source.setPixelColor(0, 0, QColor(0, 85, 170));
        source.setPixelColor(1, 0, QColor(42, 128, 230));
        source.setPixelColor(2, 0, QColor(255, 255, 255));

        const QImage result =
            PhotoCrap::Filters::applyPosterize(source, QVariantMap{{"levels", 4}});
        QCOMPARE(result.pixelColor(0, 0), QColor(0, 85, 170));
        QCOMPARE(result.pixelColor(1, 0), QColor(0, 170, 255));
        QCOMPARE(result.pixelColor(2, 0), QColor(255, 255, 255));
    }
    void thresholdSeparatesDarkAndLightTones()
    {
        QImage source(3, 1, QImage::Format_ARGB32_Premultiplied);
        source.setPixelColor(0, 0, QColor(30, 30, 30));
        source.setPixelColor(1, 0, QColor(127, 127, 127));
        source.setPixelColor(2, 0, QColor(220, 220, 220));

        const QImage result =
            PhotoCrap::Filters::applyThreshold(source, QVariantMap{{"level", 128}});
        QCOMPARE(result.pixelColor(0, 0), QColor(Qt::black));
        QCOMPARE(result.pixelColor(1, 0), QColor(Qt::black));
        QCOMPARE(result.pixelColor(2, 0), QColor(Qt::white));
    }
    void curvesAndLevelsUseDedicatedParametersAndHistogram()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        const QVariantList histogram = d.histogram();
        QCOMPARE(histogram.size(), 256);
        QCOMPARE(histogram.at(255).toDouble(), 1.);
        QCOMPARE(histogram.at(0).toDouble(), 0.);

        d.addAdjustmentLayer("curves");
        d.beginLayerPreview();
        d.previewLayerParameter("points", QVariantList{QVariantMap{{"x", 0}, {"y", 0}},
                                                       QVariantMap{{"x", 255}, {"y", 0}}});
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::black));
        d.cancelLayerPreview();
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
        d.undo();

        d.addAdjustmentLayer("levels");
        d.beginLayerPreview();
        d.previewLayerParameter("outputWhite", 96);
        const QColor limited = d.composite().pixelColor(16, 16);
        QVERIFY(limited.red() >= 95 && limited.red() <= 97);
        d.cancelLayerPreview();
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
    }
    void colorHistogramSeparatesRgbChannels()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.setColor(Qt::red);
        d.setTool("fill");
        d.beginStroke(16, 16);
        d.endStroke();
        const QVariantMap  histogram = d.colorHistogram();
        const QVariantList red       = histogram.value("red").toList();
        const QVariantList green     = histogram.value("green").toList();
        const QVariantList blue      = histogram.value("blue").toList();
        const QVariantList luminance = histogram.value("luminance").toList();
        QCOMPARE(red.size(), 256);
        QCOMPARE(green.size(), 256);
        QCOMPARE(blue.size(), 256);
        QCOMPARE(luminance.size(), 256);
        QCOMPARE(red.at(255).toDouble(), 1.);
        QCOMPARE(green.at(0).toDouble(), 1.);
        QCOMPARE(blue.at(0).toDouble(), 1.);
        QVERIFY(luminance.at(54).toDouble() > 0.9);
    }
    void displayChannelsAreNonDestructive()
    {
        Document d;
        QVERIFY(d.newDocument(16, 16));
        d.setColor(QColor(20, 100, 220));
        d.setTool("fill");
        d.beginStroke(8, 8);
        d.endStroke();
        const QImage composite = d.composite();

        d.setDisplayChannel(1);
        QCOMPARE(d.displayComposite().pixelColor(8, 8), QColor(20, 20, 20));
        d.setDisplayChannel(2);
        QCOMPARE(d.displayComposite().pixelColor(8, 8), QColor(100, 100, 100));
        d.setDisplayChannel(3);
        QCOMPARE(d.displayComposite().pixelColor(8, 8), QColor(220, 220, 220));
        QCOMPARE(d.composite(), composite);
        QCOMPARE(d.historyIndex(), 1);
        d.setDisplayChannel(0);
        QCOMPARE(d.displayComposite().pixelColor(8, 8), QColor(20, 100, 220));
    }
    void masksCanBeViewedAndFilledWithShortcutsWithoutChangingColors()
    {
        Document d;
        QVERIFY(d.newDocument(16, 16));
        d.addLayerMask(false);
        d.setColor(QColor("#cc6633"));
        d.setSecondaryColor(QColor("#336699"));

        d.fillWithColor(false);
        const int backgroundGray = qGray(QColor("#336699").rgb());
        QCOMPARE(d.maskImage(0).pixelColor(8, 8).red(), backgroundGray);

        d.fillMaskWithColor(QColor("#90c030"));
        const int selectedGray = qGray(QColor("#90c030").rgb());
        QCOMPARE(d.maskImage(0).pixelColor(8, 8).red(), selectedGray);
        QCOMPARE(d.color(), QColor("#cc6633"));
        QCOMPARE(d.secondaryColor(), QColor("#336699"));

        d.fillMaskWithColor(Qt::black);
        d.setColor(Qt::white);
        d.setTool("brush");
        d.setBrushSize(6);
        d.beginStroke(8, 8);
        d.endStroke();
        QCOMPARE(d.maskImage(0).pixelColor(8, 8).red(), 255);

        const QImage composite = d.composite();
        d.toggleMaskOnlyView(0);
        QCOMPARE(d.maskOnlyLayer(), 0);
        QCOMPARE(d.displayComposite().pixelColor(8, 8), QColor(Qt::white));
        QCOMPARE(d.displayComposite().pixelColor(1, 1), QColor(Qt::black));
        QCOMPARE(d.composite(), composite);
        d.toggleMaskOnlyView(0);
        QCOMPARE(d.maskOnlyLayer(), -1);
        QCOMPARE(d.displayComposite(), composite);
    }
    void layerMasksCanBeCopiedOntoAnotherLayerWithUndo()
    {
        Document d;
        QVERIFY(d.newDocument(16, 16, true));
        d.addLayerMask(false);
        d.setColor(Qt::white);
        d.setTool("brush");
        d.setBrushSize(6);
        d.beginStroke(8, 8);
        d.endStroke();
        const QImage sourceMask = d.maskImage(0);

        d.addLayer();
        QCOMPARE(d.activeLayer(), 1);
        d.copyLayerMask(0, 1);
        QCOMPARE(d.maskImage(1), sourceMask);
        QVERIFY(d.editingMask());
        QCOMPARE(d.history().last(), QString("Copier le masque de fusion"));

        d.setColor(Qt::black);
        d.beginStroke(8, 8);
        d.endStroke();
        QVERIFY(d.maskImage(1) != d.maskImage(0));
        d.undo();
        QCOMPARE(d.maskImage(1), sourceMask);
        d.undo();
        QVERIFY(d.maskImage(1).isNull());
    }
    void curveEyedroppersSampleColorsAndNeutralizeGray()
    {
        Document d;
        QVERIFY(d.newDocument(16, 16));
        const QColor cast(72, 128, 184);
        d.setColor(cast);
        d.setTool("fill");
        d.beginStroke(8, 8);
        d.endStroke();
        d.addAdjustmentLayer("curves");

        QCOMPARE(d.sampleCompositeColor(8, 8), cast);
        QCOMPARE(d.sampleCurvePoint("gray", 8, 8), cast);
        const QColor neutral = d.composite().pixelColor(8, 8);
        QVERIFY(qAbs(neutral.red() - neutral.green()) <= 1);
        QVERIFY(qAbs(neutral.green() - neutral.blue()) <= 1);
        d.undo();

        QCOMPARE(d.sampleCurvePoint("black", 8, 8), cast);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor(Qt::black));
        d.undo();

        QCOMPARE(d.sampleCurvePoint("white", 8, 8), cast);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor(Qt::white));
        d.resetCurveAdjustment();
        QCOMPARE(d.composite().pixelColor(8, 8), cast);
        d.undo();
        QCOMPARE(d.composite().pixelColor(8, 8), QColor(Qt::white));
    }
    void pointAdjustmentPreviewsRemainInteractiveAtUhd()
    {
        Document d;
        QVERIFY(d.newDocument(3840, 2160));
        d.addAdjustmentLayer("blackWhite");
        d.beginLayerPreview();
        QElapsedTimer timer;
        timer.start();
        d.previewLayerParameter("reds", 65);
        const QImage first               = d.composite();
        const qint64 elapsedMilliseconds = timer.elapsed();
        QVERIFY(!first.isNull());
        QVERIFY2(
            elapsedMilliseconds < 2000,
            qPrintable(QString("UHD black-and-white preview took %1 ms").arg(elapsedMilliseconds)));
        d.cancelLayerPreview();
        d.undo();

        d.addAdjustmentLayer("brightnessContrast");
        d.beginLayerPreview();
        timer.restart();
        d.previewLayerParameter("brightness", 25);
        const QImage second                        = d.composite();
        const qint64 brightnessElapsedMilliseconds = timer.elapsed();
        QVERIFY(!second.isNull());
        QVERIFY2(brightnessElapsedMilliseconds < 2000,
                 qPrintable(QString("UHD brightness/contrast preview took %1 ms")
                                .arg(brightnessElapsedMilliseconds)));
        d.cancelLayerPreview();
    }
    void blackWhiteUsesSixColorFamiliesAutoAndTint()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.setColor(QColor(220, 80, 35));
        d.setTool("fill");
        d.beginStroke(16, 16);
        d.endStroke();
        d.addAdjustmentLayer("blackWhite");

        const QVariantMap parameters = d.layers().first().toMap().value("parameters").toMap();
        QCOMPARE(parameters.value("reds").toInt(), 40);
        QCOMPARE(parameters.value("yellows").toInt(), 60);
        QCOMPARE(parameters.value("greens").toInt(), 40);
        QCOMPARE(parameters.value("cyans").toInt(), 60);
        QCOMPARE(parameters.value("blues").toInt(), 20);
        QCOMPARE(parameters.value("magentas").toInt(), 80);
        QVERIFY(!parameters.value("tintEnabled").toBool());

        const QVariantList automatic = d.autoBlackWhiteValues();
        QCOMPARE(automatic.size(), 6);
        for (const QVariant &value : automatic)
            QVERIFY(value.toInt() >= -200 && value.toInt() <= 300);

        d.beginLayerPreview();
        d.previewLayerParameter("tintEnabled", true);
        d.previewLayerParameter("tintColor", QColor("#c05030"));
        const QColor tinted = d.composite().pixelColor(16, 16);
        QVERIFY(tinted.red() != tinted.green() || tinted.green() != tinted.blue());
        d.cancelLayerPreview();
        const QColor neutral = d.composite().pixelColor(16, 16);
        QCOMPARE(neutral.red(), neutral.green());
        QCOMPARE(neutral.green(), neutral.blue());
    }
    void asynchronousPreviewKeepsOnlyTheLatestRequestedValue()
    {
        Document d;
        QVERIFY(d.newDocument(1920, 1080));
        d.addAdjustmentLayer("brightnessContrast");
        d.beginLayerPreview();
        QSignalSpy    rendered(&d, &Document::imageChanged);
        QElapsedTimer scheduling;
        scheduling.start();
        d.previewLayerParameter("brightness", -10);
        d.previewLayerParameter("brightness", -40);
        d.previewLayerParameter("brightness", -20);
        QVERIFY2(scheduling.elapsed() < 100, "preview requests must not render on the UI thread");
        QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 5000);
        const QImage latestPreview = d.displayComposite();
        const QColor latest =
            latestPreview.pixelColor(latestPreview.width() / 2, latestPreview.height() / 2);
        QVERIFY(latest.red() >= 203 && latest.red() <= 205);
        QCOMPARE(d.historyIndex(), 1);
        d.cancelLayerPreview();
    }
    void addingAnAdjustmentShowsAScreenProxyBeforeTheFullRender()
    {
        Document d;
        QVERIFY(d.newDocument(3840, 2160));
        d.setPreviewSize(800, 450);
        (void)d.composite();
        QSignalSpy     rendered(&d, &Document::imageChanged);
        QVector<QSize> renderedSizes;
        connect(&d, &Document::imageChanged, &d,
                [&] { renderedSizes.append(d.displayComposite().size()); });
        QElapsedTimer scheduling;
        scheduling.start();
        d.addAdjustmentLayer("gaussianBlur");
        const qint64 schedulingMilliseconds = scheduling.elapsed();
        QVERIFY2(schedulingMilliseconds < 500,
                 qPrintable(QStringLiteral("adding an adjustment took %1 ms; the UI thread must "
                                           "only schedule rendering")
                                .arg(schedulingMilliseconds)));
        QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty() && renderedSizes.contains(QSize(800, 450)),
                                 20000);
        QTRY_VERIFY_WITH_TIMEOUT(rendered.count() >= 3, 10000);
        QCOMPARE(d.composite().size(), QSize(3840, 2160));
        QCOMPARE(d.displayComposite().size(), QSize(800, 450));
    }
    void beginningSelectiveColorPreviewDoesNotComposeOnTheUiThread()
    {
        Document d;
        QVERIFY(d.newDocument(3840, 2160));
        d.setPreviewSize(800, 450);
        (void)d.composite();
        d.addAdjustmentLayer("selectiveColor");

        QElapsedTimer scheduling;
        scheduling.start();
        d.beginLayerPreview();
        QVERIFY2(scheduling.elapsed() < 100,
                 qPrintable(QStringLiteral("starting selective color preview took %1 ms")
                                .arg(scheduling.elapsed())));
        d.previewLayerParameter("redsCyanRed", -35);
        d.cancelLayerPreview();
    }
    void photoFilterPresetsPreserveLuminosity()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.setColor(QColor(110, 130, 150));
        d.setTool("fill");
        d.beginStroke(16, 16);
        d.endStroke();
        const QColor source = d.composite().pixelColor(16, 16);
        d.addAdjustmentLayer("photoFilter");
        const QVariantMap parameters = d.layers().first().toMap().value("parameters").toMap();
        QCOMPARE(parameters.value("preset").toInt(), 0);
        QCOMPARE(parameters.value("color").toString(), QString("#ec8a00"));
        QCOMPARE(parameters.value("density").toInt(), 25);
        QVERIFY(parameters.value("preserveLuminosity").toBool());

        QTRY_VERIFY_WITH_TIMEOUT(d.displayComposite().size() == QSize(32, 32), 5000);
        const QColor filtered  = d.composite().pixelColor(16, 16);
        const auto   luminance = [](const QColor &_color) {
            return .2126 * _color.red() + .7152 * _color.green() + .0722 * _color.blue();
        };
        QVERIFY(qAbs(luminance(filtered) - luminance(source)) < 2.);
        QVERIFY(filtered.red() > source.red());
        QVERIFY(filtered.blue() < source.blue());
    }
    void channelMixerSupportsOutputChannelsConstantsAndMonochrome()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.setColor(QColor(80, 140, 210));
        d.setTool("fill");
        d.beginStroke(16, 16);
        d.endStroke();
        d.addAdjustmentLayer("channelMixer");
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(80, 140, 210));

        d.beginLayerPreview();
        d.previewLayerParameter("redRed", 0);
        d.previewLayerParameter("redGreen", 100);
        QCOMPARE(d.composite().pixelColor(16, 16).red(), 140);
        d.cancelLayerPreview();

        d.beginLayerPreview();
        d.previewLayerParameter("monochrome", true);
        d.previewLayerParameter("monoRed", 0);
        d.previewLayerParameter("monoGreen", 100);
        d.previewLayerParameter("monoBlue", 0);
        const QColor monochrome = d.composite().pixelColor(16, 16);
        QCOMPARE(monochrome, QColor(140, 140, 140));
        d.previewLayerParameter("monoConstant", 10);
        QCOMPARE(d.composite().pixelColor(16, 16).red(), 166);
        d.cancelLayerPreview();
    }
    void blendModeHoverUsesDraftAndCommitsOnlyTheChosenMode()
    {
        Document d;
        QVERIFY(d.newDocument(1920, 1080));
        d.setPreviewSize(800, 450);
        d.addLayer();
        const int historyBefore = d.historyIndex();
        d.beginLayerPreview();
        d.previewLayerBlend(d.activeLayer(), 3);
        d.previewLayerBlend(d.activeLayer(), 12);
        QCOMPARE(d.historyIndex(), historyBefore);
        QTRY_COMPARE_WITH_TIMEOUT(d.displayComposite().size(), QSize(800, 450), 5000);
        d.finishLayerPreview("Mode de fusion");
        QCOMPARE(d.historyIndex(), historyBefore + 1);
        QCOMPARE(d.layers().first().toMap().value("blend").toInt(), 12);
        d.undo();
        QCOMPARE(d.layers().first().toMap().value("blend").toInt(), 0);

        d.beginLayerPreview();
        d.previewLayerBlend(d.activeLayer(), 8);
        d.cancelLayerPreview();
        QCOMPARE(d.layers().first().toMap().value("blend").toInt(), 0);
        QCOMPARE(d.historyIndex(), historyBefore);
    }
    void filterLayersAreNonDestructiveAndEditable()
    {
        Document d;
        QVERIFY(d.newDocument(64, 64));
        d.setColor(Qt::black);
        d.setBrushSize(9);
        d.beginStroke(32, 32);
        d.endStroke();
        const QImage      original = d.composite();
        const QStringList filters  = {"gaussianBlur", "motionBlur", "boxBlur", "unsharpMask",
                                      "highPass",     "mosaic",     "noise"};
        for (const auto &filter : filters) {
            d.addAdjustmentLayer(filter);
            QCOMPARE(d.layers().size(), 2);
            const QImage filtered = d.composite();
            QVERIFY2(filtered != original, qPrintable(filter + " should alter the image"));
            const int historyBefore = d.historyIndex();
            d.beginLayerPreview();
            const QString parameter = filter == "motionBlur" ? QStringLiteral("angle")
                                      : filter == "mosaic"   ? QStringLiteral("cellSize")
                                      : filter == "noise"    ? QStringLiteral("amount")
                                                             : QStringLiteral("radius");
            d.previewLayerParameter(parameter, filter == "motionBlur" ? 45 : 16);
            d.finishLayerPreview(QStringLiteral("Modifier le filtre"));
            QCOMPARE(d.historyIndex(), historyBefore + 1);
            d.undo();
            d.undo();
            QCOMPARE(d.composite(), original);
        }
    }
    void frequencyRetouchMakesBothBandsEditable_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8-bit") << 8;
        QTest::newRow("16-bit") << 16;
    }
    void frequencyRetouchMakesBothBandsEditable()
    {
        QFETCH(int, bits);
        QTemporaryDir dir;
        QImage        image(32, 32,
                            bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                       : QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const int texture = (x + y) % 3 == 0 ? 6000 : -3000;
                image.setPixelColor(x, y,
                                    QColor::fromRgba64(18001 + x * 400 + texture,
                                                       24003 + y * 300 + texture,
                                                       28007 + x * 100 + texture, 65535));
            }
        const auto input = QUrl::fromLocalFile(dir.filePath("texture.png"));
        QVERIFY(image.save(input.toLocalFile()));
        Document d;
        QVERIFY(d.openFile(input));
        const QImage original = d.layerImage(0);
        d.addFrequencySeparation(3);
        const QImage low     = d.layerImage(1).convertToFormat(ImageUtils::workingFormat(original));
        const QImage high    = d.layerImage(2);
        const QImage before  = d.composite();
        const int    history = d.historyIndex();
        QSignalSpy   errors(&d, &Document::errorOccurred);
        QVERIFY(d.prepareFrequencyRetouch());
        QVERIFY(d.frequencyRetouchBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!d.frequencyRetouchBusy(), 10000);
        QVERIFY(errors.isEmpty());
        QCOMPARE(d.historyIndex(), history + 1);
        QCOMPARE(d.layers().size(), 3);
        QCOMPARE(d.layers()[0].toMap()["kind"].toString(), QString("pixel"));
        QCOMPARE(d.layers()[1].toMap()["kind"].toString(), QString("pixel"));
        QCOMPARE(d.layers()[0].toMap()["blend"].toInt(), 16);
        QCOMPARE(d.layers()[0].toMap()["parameters"].toMap()["_frequencyBand"].toString(),
                 QString("high"));
        QCOMPARE(d.layers()[1].toMap()["parameters"].toMap()["_frequencyBand"].toString(),
                 QString("low"));
        QCOMPARE(d.layers()[0].toMap()["parameters"].toMap()["_frequencyRadius"].toDouble(), 3.0);
        QCOMPARE(d.layerImage(1), low);
        QCOMPARE(d.layerImage(2), high);
        QCOMPARE(d.channelBits(), bits);
        const auto rebuilt = d.composite();
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const auto a         = before.pixelColor(x, y).rgba64(),
                           b         = rebuilt.pixelColor(x, y).rgba64();
                const int  tolerance = bits == 16 ? 3 : 257;
                QVERIFY(qAbs(int(a.red()) - int(b.red())) <= tolerance);
                QVERIFY(qAbs(int(a.green()) - int(b.green())) <= tolerance);
                QVERIFY(qAbs(int(a.blue()) - int(b.blue())) <= tolerance);
            }
        d.setActiveLayer(1);
        d.setTool("brush");
        d.setColor(Qt::red);
        d.setBrushSize(5);
        d.setBrushHardness(1);
        d.setBrushOpacity(1);
        d.setBrushFlow(1);
        d.beginStroke(8, 8);
        d.endStroke();
        QCOMPARE(d.layerImage(1).pixelColor(8, 8), QColor(Qt::red));
        QCOMPARE(d.layerImage(2), high);
        QCOMPARE(d.layerImage(0), original);
        const QImage editedLow = d.layerImage(1);
        d.setActiveLayer(2);
        d.setTool("cloneStamp");
        d.setCloneSampleMode(0);
        QVERIFY(high.pixelColor(8, 8) != high.pixelColor(21, 21));
        d.setCloneSource(8, 8);
        d.beginStroke(21, 21);
        d.endStroke();
        QCOMPARE(d.layerImage(2).pixelColor(21, 21).rgba64(), high.pixelColor(8, 8).rgba64());
        QVERIFY(d.layerImage(2) != high);
        QCOMPARE(d.layerImage(1), editedLow);
        QCOMPARE(d.layerImage(0), original);
        const auto project = QUrl::fromLocalFile(dir.filePath("retouch.pcr"));
        QVERIFY(d.saveProject(project));
        Document loaded;
        QVERIFY(loaded.openFile(project));
        QCOMPARE(loaded.channelBits(), bits);
        QCOMPARE(loaded.layers()[0].toMap()["parameters"], d.layers()[0].toMap()["parameters"]);
        QCOMPARE(loaded.layers()[1].toMap()["parameters"], d.layers()[1].toMap()["parameters"]);
        QCOMPARE(loaded.layerImage(1), d.layerImage(1));
        QCOMPARE(loaded.layerImage(2), d.layerImage(2));
        QCOMPARE(loaded.composite(), d.composite());
        // Returning to the radius works after reload, without any saved undo history.
        loaded.setActiveLayer(1);
        loaded.setLayerLocked(2, true);
        QVERIFY(!loaded.restoreDynamicFrequency());
        QCOMPARE(loaded.layers()[0].toMap()["kind"].toString(), QString("pixel"));
        loaded.setLayerLocked(2, false);
        const auto   preparedLayers    = loaded.layers();
        const QImage preparedComposite = loaded.composite();
        QVERIFY(loaded.restoreDynamicFrequency());
        QCOMPARE(loaded.layers()[0].toMap()["kind"].toString(), QString("frequencyHigh"));
        QCOMPARE(loaded.layers()[1].toMap()["kind"].toString(), QString("frequencyLow"));
        QCOMPARE(loaded.layers()[1].toMap()["parameters"].toMap()["radius"].toDouble(), 3.0);
        QCOMPARE(loaded.layerImage(1).convertToFormat(ImageUtils::workingFormat(original)), low);
        QCOMPARE(loaded.layerImage(2), high);
        QCOMPARE(loaded.layerImage(0), original);
        loaded.undo();
        QCOMPARE(loaded.layers(), preparedLayers);
        QCOMPARE(loaded.layerImage(1), d.layerImage(1));
        QCOMPARE(loaded.layerImage(2), d.layerImage(2));
        QCOMPARE(loaded.composite(), preparedComposite);
        d.undo(); // Clone stroke.
        QCOMPARE(d.layerImage(2), high);
        d.undo(); // Brush stroke.
        QCOMPARE(d.layerImage(1), low);
        d.undo(); // Retouch preparation.
        QCOMPARE(d.layers()[0].toMap()["kind"].toString(), QString("frequencyHigh"));
        QCOMPARE(d.layers()[1].toMap()["kind"].toString(), QString("frequencyLow"));
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(d.composite(), before);
        d.redo();
        QCOMPARE(d.layerImage(1), low);
        QCOMPARE(d.layerImage(2), high);
    }
    void frequencyRetouchPreservesMasksAndLayerMetadata()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.addFrequencySeparation(4);
        d.addLayerMask(false);
        d.setLayerOpacity(2, .6);
        d.setLayerBlend(2, 8);
        d.setActiveLayer(1);
        d.addLayerMask(true);
        d.setLayerOpacity(1, .8);
        d.setEditingMask(false);
        const auto   layers  = d.layers();
        const QImage lowMask = d.maskImage(1), highMask = d.maskImage(2);
        QVERIFY(d.prepareFrequencyRetouch());
        QTRY_VERIFY_WITH_TIMEOUT(!d.frequencyRetouchBusy(), 10000);
        QCOMPARE(d.maskImage(1), lowMask);
        QCOMPARE(d.maskImage(2), highMask);
        QCOMPARE(d.layers()[0].toMap()["opacity"], layers[0].toMap()["opacity"]);
        QCOMPARE(d.layers()[1].toMap()["opacity"], layers[1].toMap()["opacity"]);
        QCOMPARE(d.layers()[0].toMap()["blend"], layers[0].toMap()["blend"]);
        QCOMPARE(d.layers()[1].toMap()["name"], layers[1].toMap()["name"]);
        QVERIFY(!d.layers()[1].toMap()["parameters"].toMap().contains("radius"));
        QVERIFY(d.layers()[1].toMap()["parameters"].toMap().contains("_maskDensity"));
        QVERIFY(d.restoreDynamicFrequency());
        QCOMPARE(d.maskImage(1), lowMask);
        QCOMPARE(d.maskImage(2), highMask);
        QCOMPARE(d.layers()[0].toMap()["opacity"], layers[0].toMap()["opacity"]);
        QCOMPARE(d.layers()[1].toMap()["opacity"], layers[1].toMap()["opacity"]);
        QCOMPARE(d.layers()[0].toMap()["blend"], layers[0].toMap()["blend"]);
        QCOMPARE(d.layers()[1].toMap()["name"], layers[1].toMap()["name"]);
        QCOMPARE(d.layers()[1].toMap()["parameters"].toMap()["radius"].toDouble(), 4.0);
    }
    void frequencyRadiusRestoreRejectsAmbiguousOrIncompletePairs()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.addFrequencySeparation(4);
        QVERIFY(d.prepareFrequencyRetouch());
        QTRY_VERIFY_WITH_TIMEOUT(!d.frequencyRetouchBusy(), 10000);
        d.setActiveLayer(2);
        d.duplicateLayer();
        const auto ambiguousLayers = d.layers();
        const int  history         = d.historyIndex();
        QVERIFY(!d.restoreDynamicFrequency());
        QCOMPARE(d.layers(), ambiguousLayers);
        QCOMPARE(d.historyIndex(), history);
        d.undo();
        d.setActiveLayer(1);
        d.removeLayer();
        const auto incompleteLayers = d.layers();
        QVERIFY(!d.restoreDynamicFrequency());
        QCOMPARE(d.layers(), incompleteLayers);
    }
    void frequencyRetouchCancellationAndStaleResultsAreSafe()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.addFrequencySeparation(4);
        const auto layers  = d.layers();
        const auto history = d.history();
        QVERIFY(d.prepareFrequencyRetouch());
        d.cancelFrequencyRetouch();
        QTRY_VERIFY_WITH_TIMEOUT(!d.frequencyRetouchBusy(), 10000);
        QCOMPARE(d.layers(), layers);
        QCOMPARE(d.history(), history);
        d.beginLayerPropertiesSession();
        QVERIFY(d.prepareFrequencyRetouch());
        d.cancelLayerPropertiesSession();
        QTRY_VERIFY_WITH_TIMEOUT(!d.frequencyRetouchBusy(), 10000);
        QCOMPARE(d.layers(), layers);
        QVERIFY(d.prepareFrequencyRetouch());
        QVERIFY(d.newDocument(16, 16, false, 16));
        QTRY_VERIFY_WITH_TIMEOUT(!d.frequencyRetouchBusy(), 10000);
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.channelBits(), 16);
        QCOMPARE(d.layerImage(0).pixelColor(0, 0), QColor(Qt::white));
    }
    void frequencyRetouchRejectsLockedOrIncompletePairs()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.addFrequencySeparation(4);
        d.setLayerLocked(1, true);
        QVERIFY(!d.prepareFrequencyRetouch());
        QVERIFY(!d.frequencyRetouchBusy());
        d.setLayerLocked(1, false);
        d.removeLayer();
        QCOMPARE(d.layers().size(), 2);
        QVERIFY(!d.prepareFrequencyRetouch());
        QCOMPARE(d.layers()[0].toMap()["kind"].toString(), QString("frequencyLow"));
    }

    void frequencySeparationHasRealtimePreview()
    {
        Document d;
        QVERIFY(d.newDocument(24, 24));
        d.setColor(Qt::black);
        d.setBrushSize(3);
        d.beginStroke(12, 12);
        d.endStroke();
        const QImage original = d.composite();
        d.addFrequencySeparation(4);
        QCOMPARE(d.layers().size(), 3);
        QVERIFY(qAbs(d.composite().pixelColor(12, 12).red() - original.pixelColor(12, 12).red()) <=
                1);
        const QImage initialLow      = d.layerImage(1);
        const int    before          = d.historyIndex();
        const int    thumbnailBefore = d.thumbnailRevision();
        d.beginLayerPreview();
        d.previewLayerParameter("radius", 9);
        QCOMPARE(d.thumbnailRevision(), thumbnailBefore);
        QVERIFY(d.layerImage(1) != initialLow);
        QCOMPARE(d.historyIndex(), before);
        d.finishLayerPreview("Rayon des fréquences");
        QVERIFY(d.thumbnailRevision() > thumbnailBefore);
        QCOMPARE(d.historyIndex(), before + 1);
        d.undo();
        QVERIFY(qAbs(d.composite().pixelColor(12, 12).red() - original.pixelColor(12, 12).red()) <=
                1);
    }
    void highFrequencyMaskUsesRegionalCompositeWithoutChangingResult()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Document d;
        QVERIFY(d.newDocument(128, 128));
        d.setColor(Qt::black);
        d.setBrushSize(12);
        d.beginStroke(64, 64);
        d.endStroke();
        d.addFrequencySeparation(6);
        d.addLayerMask(true);
        d.setTool("brush");
        d.setBrushSize(18);
        (void)d.composite();
        d.beginStroke(48, 64);
        d.continueStroke(80, 64);
        (void)d.composite();
        d.endStroke();
        const QImage regional = d.composite();
        const auto   url      = QUrl::fromLocalFile(dir.filePath("regional.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        const QImage rebuilt   = loaded.composite();
        int          different = 0, maxDelta = 0;
        QRect        differenceBounds;
        for (int y = 0; y < regional.height(); ++y)
            for (int x = 0; x < regional.width(); ++x) {
                const QColor a = regional.pixelColor(x, y), b = rebuilt.pixelColor(x, y);
                const int    delta = qMax(qAbs(a.red() - b.red()), qMax(qAbs(a.green() - b.green()),
                                                                        qAbs(a.blue() - b.blue())));
                if (delta) {
                    ++different;
                    maxDelta         = qMax(maxDelta, delta);
                    differenceBounds = differenceBounds.united(QRect(x, y, 1, 1));
                }
            }
        QVERIFY2(maxDelta <= 1, qPrintable(QString("%1 pixels differ, max %2, bounds %3,%4 %5x%6")
                                               .arg(different)
                                               .arg(maxDelta)
                                               .arg(differenceBounds.x())
                                               .arg(differenceBounds.y())
                                               .arg(differenceBounds.width())
                                               .arg(differenceBounds.height())));
    }
    void blurAdjustmentUsesRegionalCompositeWithoutChangingResult()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Document d;
        QVERIFY(d.newDocument(256, 192, true));
        d.setColor(Qt::black);
        d.setBrushSize(24);
        d.beginStroke(64, 96);
        d.endStroke();
        d.addAdjustmentLayer("gaussianBlur");
        (void)d.composite();
        d.setActiveLayer(0);
        d.setColor(Qt::red);
        d.setBrushSize(18);
        d.beginStroke(176, 96);
        d.endStroke();
        const QImage regional = d.composite();

        const auto url = QUrl::fromLocalFile(dir.filePath("regional-blur.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.composite(), regional);
    }
    void selectionDefinesAdjustmentMaskLikePhotoshop()
    {
        Document d;
        QVERIFY(d.newDocument(20, 20));
        d.setTool("select");
        d.beginStroke(5, 5);
        d.continueStroke(14, 14);
        d.endStroke();
        d.addAdjustmentLayer("invert");
        QCOMPARE(d.maskImage(1).pixelColor(8, 8).red(), 255);
        QCOMPARE(d.maskImage(1).pixelColor(1, 1).red(), 0);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor(Qt::black));
        QCOMPARE(d.composite().pixelColor(1, 1), QColor(Qt::white));
    }
    void allPhotoshopBlendModesAreSelectable()
    {
        Document d;
        QVERIFY(d.newDocument(4, 4));
        d.addLayer();
        for (int mode = 0; mode <= 26; ++mode) {
            d.setLayerBlend(1, mode);
            const auto layers = d.layers();
            QCOMPARE(layers.first().toMap().value("blend").toInt(), mode);
            QVERIFY(!d.composite().isNull());
        }
    }
    void advancedLayersSurviveProjectRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Document d;
        QVERIFY(d.newDocument(20, 16));
        d.addLayerMask(true);
        d.setEditingMask(false);
        d.addAdjustmentLayer("posterize");
        d.beginLayerPreview();
        d.previewLayerParameter("levels", 6);
        d.finishLayerPreview();
        d.addFrequencySeparation(5);
        d.setLayerBlend(d.activeLayer(), 8);
        const QImage expected = d.composite();
        const auto   url      = QUrl::fromLocalFile(dir.filePath("advanced.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.layers(), d.layers());
        QCOMPARE(loaded.composite(), expected);
        QVERIFY(!loaded.maskImage(0).isNull());
    }
    void moveSelectionAndCancel()
    {
        Document d;
        d.newDocument(32, 32, true);
        d.setColor(Qt::red);
        d.setBrushSize(4);
        d.beginStroke(6, 6);
        d.endStroke();
        d.setTool("select");
        d.beginStroke(2, 2);
        d.continueStroke(10, 10);
        d.endStroke();
        d.setTool("move");
        d.beginStroke(6, 6);
        d.continueStroke(16, 16);
        d.endStroke();
        QCOMPARE(d.composite().pixelColor(6, 6).alpha(), 0);
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::red));
        QCOMPARE(d.selection().topLeft(), QPoint(12, 12));
        d.deselect();
        const QImage before = d.composite();
        d.setTool("brush");
        d.beginStroke(25, 25);
        d.cancelStroke();
        QCOMPARE(d.composite(), before);
    }
    void failedSaveDoesNotMarkClean()
    {
        Document d;
        d.newDocument(8, 8);
        d.addLayer();
        QVERIFY(d.modified());
        QVERIFY(!d.saveProject(QUrl::fromLocalFile("/missing-directory-photocrap/project.pcr")));
        QVERIFY(d.modified());
    }
};
QTEST_MAIN(DocumentTest)
#include "DocumentTest.moc"
