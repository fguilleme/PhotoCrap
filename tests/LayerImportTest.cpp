#include "Document.h"
#include "filters/ColorLookupFilter.h"
#include "filters/LightFilter.h"
#include "filters/ExposureFilter.h"
#include "filters/HueSaturationFilter.h"
#include "filters/PhotoFilterFilter.h"
#include "filters/GradientMapFilter.h"
#include "filters/ColorBalanceFilter.h"
#include "filters/ColorFilter.h"
#include "filters/ToneMappingTable.h"
#include "filters/PosterizeFilter.h"
#include "Model3DTestImage.h"
#include "RawTestImage.h"
#include "model3d/Model3D.h"
#include "model3d/Model3DPrivate.h"
#include <QElapsedTimer>
#include <QFile>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

class LayerImportTest : public QObject
{
    Q_OBJECT
private slots:
    void largePsdSurvivesProjectRoundTrip()
    {
        // A sparse, flat 8192 x 4097 grayscale PSD just above the former pixel limit.
        QTemporaryDir temp;
        QFile psd(temp.filePath("large.psd"));
        QVERIFY(psd.open(QIODevice::WriteOnly));
        const auto header = QByteArray::fromHex(
            "38425053 0001 000000000000 0001 00001001 00002000 0008 0001 "
            "00000000 00000000 00000000 0000");
        QCOMPARE(psd.write(header), header.size());
        QVERIFY(psd.seek(header.size() + 8192LL * 4097 - 1));
        QVERIFY(psd.putChar(char(230)));
        psd.close();

        Document doc;
        QVERIFY(doc.openFile(QUrl::fromLocalFile(psd.fileName())));
        QCOMPARE(doc.width(), 8192);
        QCOMPARE(doc.height(), 4097);
        const auto project = QUrl::fromLocalFile(temp.filePath("large.pcr"));
        QVERIFY(doc.saveProject(project));
        Document reopened;
        QVERIFY(reopened.openFile(project));
        QCOMPARE(reopened.width(), 8192);
        QCOMPARE(reopened.height(), 4097);
        QCOMPARE(reopened.layerImage(0).pixelColor(8191, 4096).red(), 230);
    }

    void nestedPsdGroupKeepsEditableMask()
    {
        Document doc;
        QSignalSpy report(&doc, &Document::psdImportReport);
        QVERIFY(doc.openFile(QUrl::fromLocalFile(QFINDTESTDATA("fixtures/nested-masked-group.psd"))));
        QCOMPARE(report.size(), 0);
        QCOMPARE(doc.layerCount(), 5);
        const auto layers = doc.layers();
        QCOMPARE(layers[2].toMap()["groupDepth"].toInt(), 2);
        QVERIFY(layers[1].toMap()["hasMask"].toBool());
        QCOMPARE(doc.composite().pixelColor(0, 0), QColor(Qt::blue));
        QCOMPARE(doc.composite().pixelColor(1, 0), QColor(0, 128, 127));
        QCOMPARE(doc.composite().pixelColor(2, 0), QColor(Qt::green));
        doc.selectLayer(3);
        doc.setEditingMask(true);
        QVERIFY(doc.layers()[1].toMap()["editingMask"].toBool());
        doc.setTool("brush");
        doc.setColor(Qt::white);
        doc.beginStroke(0, 0);
        doc.endStroke();
        QCOMPARE(doc.composite().pixelColor(0, 0), QColor(Qt::green));
        doc.undo();
        QCOMPARE(doc.composite().pixelColor(0, 0), QColor(Qt::blue));
        doc.invertLayerMask();
        QCOMPARE(doc.composite().pixelColor(0, 0), QColor(Qt::green));
        doc.undo();
        QCOMPARE(doc.composite().pixelColor(0, 0), QColor(Qt::blue));
        doc.toggleGroupCollapsed(4);
        QCOMPARE(doc.layers().size(), 2);
        doc.setLayerVisible(4, false);
        QCOMPARE(doc.composite().pixelColor(2, 0), QColor(Qt::blue));
        doc.setLayerVisible(4, true);
        QTemporaryDir temp;
        const auto url = QUrl::fromLocalFile(temp.filePath("nested.pcr"));
        QVERIFY(doc.saveProject(url));
        Document reopened;
        QVERIFY(reopened.openFile(url));
        QCOMPARE(reopened.layers().size(), 2);
        QCOMPARE(reopened.composite(), doc.composite());
        reopened.toggleGroupCollapsed(4);
        QCOMPARE(reopened.layers()[1].toMap()["groupDepth"].toInt(), 1);
        reopened.selectLayer(3);
        reopened.setMaskEnabled(false);
        QCOMPARE(reopened.composite().pixelColor(0, 0), QColor(Qt::green));
        reopened.selectLayer(4);
        reopened.removeLayer();
        QCOMPARE(reopened.layerCount(), 1);
    }

    void psdAdjustmentsKeepSettingsAndLocalMask()
    {
        Document doc;
        QVERIFY(doc.openFile(QUrl::fromLocalFile(QFINDTESTDATA("fixtures/adjustment-layers.psd"))));
        QCOMPARE(doc.layerCount(), 6);
        const auto layers = doc.layers();
        const QStringList types{"vibrance", "colorBalance", "levels", "curves", "hueSaturation"};
        for (int i = 0; i < types.size(); ++i) {
            const auto layer = layers[i].toMap();
            QCOMPARE(layer["kind"].toString(), QString("adjustment"));
            QCOMPARE(layer["adjustmentType"].toString(), types[i]);
        }
        const auto params = [&](int row) { return layers[row].toMap().value("parameters").toMap(); };
        QCOMPARE(params(0)["vibrance"].toInt(), -22);
        QCOMPARE(params(1)["shadowCyanRed"].toInt(), -15);
        QCOMPARE(params(1)["highlightYellowBlue"].toInt(), 15);
        QCOMPARE(params(2)["gamma"].toDouble(), 1.25);
        QCOMPARE(params(3)["points"].toList().last().toMap().value("y").toInt(), 240);
        QVERIFY(params(4)["colorize"].toBool());
        for (int index = 2; index < doc.layerCount(); ++index)
            doc.setLayerVisible(index, false);
        auto image = doc.composite();
        QCOMPARE(image.pixelColor(0, 0), QColor(128, 128, 128));
        QVERIFY(image.pixelColor(1, 0).green() > image.pixelColor(1, 0).red() + 100);
        QTemporaryDir temp;
        const auto url = QUrl::fromLocalFile(temp.filePath("adjustments.pcr"));
        QVERIFY(doc.saveProject(url));
        Document reopened;
        QVERIFY(reopened.openFile(url));
        QCOMPARE(reopened.composite(), image);
        QCOMPARE(reopened.layers().first().toMap()["parameters"], layers.first().toMap()["parameters"]);
    }

    void psdSelectiveColorKeepsMethodsAndInkDirections()
    {
        Document doc;
        QSignalSpy report(&doc, &Document::psdImportReport);
        QVERIFY(doc.openFile(QUrl::fromLocalFile(QFINDTESTDATA("fixtures/selective-color.psd"))));
        QVERIFY(report.isEmpty());
        QCOMPARE(doc.layerCount(), 3);
        const auto layers = doc.layers();
        for (int row = 0; row < 2; ++row) {
            const auto layer = layers[row].toMap();
            QCOMPARE(layer["adjustmentType"].toString(), QString("selectiveColor"));
            const auto params = layer["parameters"].toMap();
            QVERIFY(params["photoshopAdjustment"].toBool());
            QCOMPARE(params.size(), 38);
            QCOMPARE(params["relative"].toBool(), row == 1);
            QCOMPARE(params["redsCyanRed"].toInt(), 12);
            QCOMPARE(params["redsMagentaGreen"].toInt(), -18);
            QCOMPARE(params["redsYellowBlue"].toInt(), 24);
            QCOMPARE(params["redsBlack"].toInt(), 30);
            QCOMPARE(params["neutralsBlack"].toInt(), -28);
            QCOMPARE(params["blacksCyanRed"].toInt(), -34);
        }
        const auto both = doc.composite();
        doc.setLayerVisible(layers.first().toMap()["layerIndex"].toInt(), false);
        const auto relativeOnly = doc.composite();
        QCOMPARE(both.pixelColor(0, 0), relativeOnly.pixelColor(0, 0));
        QVERIFY(both.pixelColor(1, 0) != relativeOnly.pixelColor(1, 0));
    }

    void modernPsdImportsEditableAdjustments()
    {
        Document   doc;
        QSignalSpy report(&doc, &Document::psdImportReport);
        QVERIFY(
            doc.openFile(QUrl::fromLocalFile(QFINDTESTDATA("fixtures/modern-adjustments.psd"))));
        QVERIFY(report.isEmpty());
        QCOMPARE(doc.layerCount(), 14);
        QMap<QString, QVariantMap> params;
        for (const auto &row : doc.layers()) {
            const auto layer = row.toMap();
            if (layer["kind"] == "adjustment")
                params.insert(layer["adjustmentType"].toString(), layer["parameters"].toMap());
        }
        QCOMPARE(params.size(), 13);
        QVERIFY(params["light"]["photoshopLight"].toBool());
        QCOMPARE(params["light"]["exposure"].toDouble(), 1.25);
        QCOMPARE(params["light"]["contrast"].toInt(), -22);
        QCOMPARE(params["color"]["temperature"].toInt(), 20);
        QCOMPARE(params["clarityDehaze"]["dehaze"].toInt(), 21);
        QCOMPARE(params["grain"]["size"].toDouble(), 4.5);
        QCOMPARE(params["grain"]["seed"].toInt(), -9);
        QVERIFY(params["exposure"]["photoshopExposure"].toBool());
        QCOMPARE(params["exposure"]["offset"].toDouble(), -.125);
        QCOMPARE(params["blackWhite"]["tintColor"].toString(), QString("#4080e0"));
        QCOMPARE(params["photoFilter"]["density"].toInt(), 65);
        QCOMPARE(params["photoFilter"]["color"].toString(), QString("#00ff00"));
        QCOMPARE(params["channelMixer"]["monoConstant"].toInt(), -12);
        QCOMPARE(params["threshold"]["level"].toInt(), 169);
        QCOMPARE(params["gradientMap"]["shadowColor"].toString(), QString("#0000ff"));
        QCOMPARE(params["gradientMap"]["highlightColor"].toString(), QString("#00ff00"));
        QVERIFY(params.contains("invert"));
        QImage sample(1, 1, QImage::Format_RGB32);
        sample.fill(QColor(64, 96, 160));
        QCOMPARE(
            PhotoCrap::Filters::applyColorLookup(sample, params["colorLookup"]).pixelColor(0, 0),
            QColor(191, 96, 160));
        QCOMPARE(PhotoCrap::Filters::applyPosterize(sample, params["posterize"]).pixelColor(0, 0),
                 QColor(64, 97, 161));
    }

    void photoshopColourChartMatchesReference()
    {
        using namespace PhotoCrap::Filters;
        // Isolated Photoshop adjustment layers on a synthetic RGB chart.
        QImage patch(1, 1, QImage::Format_RGB32);
        patch.fill(QColor(99, 8, 206));
        const auto check = [](const QImage &out, QColor expected, int tolerance) {
            const auto actual = out.pixelColor(0, 0);
            const int  error  = std::max({std::abs(actual.red() - expected.red()),
                                          std::abs(actual.green() - expected.green()),
                                          std::abs(actual.blue() - expected.blue())});
            QVERIFY2(error <= tolerance, qPrintable(actual.name() + " vs " + expected.name()));
        };
        check(applyHueSaturation(patch, {{"photoshopAdjustment", true},
                                         {"hue", 49},
                                         {"saturation", -38},
                                         {"lightness", -28}}),
              QColor(122, 33, 98), 2);
        check(applyColorBalance(patch, {{"photoshopAdjustment", true},
                                        {"cyanRed", -39},
                                        {"yellowBlue", 18},
                                        {"preserveLuminosity", true}}),
              QColor(80, 10, 214), 2);
        check(applyPhotoFilter(patch, {{"photoshopAdjustment", true},
                                       {"color", "#19c919"},
                                       {"density", 25},
                                       {"preserveLuminosity", true}}),
              QColor(89, 18, 181), 2);
        check(applyGradientMap(patch, {{"photoshopAdjustment", true},
                                       {"shadowColor", "#000000"},
                                       {"highlightColor", "#ffffff"},
                                       {"gradientMethod", "Smoo"}}),
              QColor(23, 23, 23), 2);
        patch.fill(QColor(128, 128, 128));
        check(applyColor(patch, {{"photoshopAdjustment", true}, {"temperature", -50}}),
              QColor(102, 146, 211), 2);
        QImage fine(2, 1, QImage::Format_RGBA64);
        fine.setPixelColor(0, 0, QColor::fromRgba64(32000, 32010, 32020, 65535));
        fine.setPixelColor(1, 0, QColor::fromRgba64(32004, 32014, 32024, 65535));
        const auto mapped = applyColor(fine, {{"photoshopAdjustment", true}, {"temperature", -50}});
        QVERIFY(mapped.pixelColor(0, 0).rgba64().green() <
                mapped.pixelColor(1, 0).rgba64().green());
    }

    void photoshopExposureMatchesReferenceSamples()
    {
        using namespace PhotoCrap::Filters;
        const QVariantMap settings{
            {"photoshopExposure", true}, {"exposure", 1.06}, {"offset", -.117}, {"gamma", .84}};
        QImage patch(1, 1, QImage::Format_RGB32);
        patch.fill(QColor(64, 96, 160));
        QCOMPARE(applyExposure(patch, settings).pixelColor(0, 0), QColor(0, 83, 199));
        const auto lut = toneMappingTable("exposure", settings);
        QCOMPARE(qRed(lut.pixel(96, 0)), 83);
        QCOMPARE(qRed(lut.pixel(160, 0)), 199);
        QImage fine(1, 1, QImage::Format_RGBA64);
        fine.setPixelColor(0, 0, QColor::fromRgba64(32001, 32002, 32003, 65535));
        const auto pixel = applyExposure(fine, settings).pixelColor(0, 0).rgba64();
        QVERIFY(pixel.red() < pixel.green() && pixel.green() < pixel.blue());
    }

    void photoshopLightPreservesTonalOrderAndHighlightDetail()
    {
        using namespace PhotoCrap::Filters;
        const QVariantMap params{{"photoshopLight", true}, {"exposure", .9}, {"contrast", -11},
                                 {"highlights", 19}, {"shadows", -41}, {"whites", -50}, {"blacks", 28}};
        QCOMPARE(toneMappingSettings("light", params).mode, ToneMappingMode::Channels);
        QImage ramp(256, 1, QImage::Format_RGB32);
        for (int x = 0; x < 256; ++x)
            ramp.setPixel(x, 0, qRgb(x, x, x));
        const auto result = applyLight(ramp, params);
        int previous = 0;
        for (int x = 0; x < 256; ++x) {
            const int v = qRed(result.pixel(x, 0));
            QVERIFY(v >= previous);
            previous = v;
        }
        QVERIFY(qRed(result.pixel(16, 0)) < 40);
        QVERIFY(qRed(result.pixel(224, 0)) - qRed(result.pixel(192, 0)) > 10);
        QVERIFY(qRed(result.pixel(248, 0)) - qRed(result.pixel(224, 0)) > 8);
        QImage fine(256, 1, QImage::Format_RGBA64);
        for (int x = 0; x < fine.width(); ++x)
            reinterpret_cast<QRgba64 *>(fine.scanLine(0))[x] =
                QRgba64::fromRgba64(42000 + x, 42000 + x, 42000 + x, 65535);
        const auto wide = applyLight(fine, params);
        const auto *pixels = reinterpret_cast<const QRgba64 *>(wide.constScanLine(0));
        QVERIFY(pixels[255].red() > pixels[0].red() + 100);
        const auto neutral = applyLight(fine, {{"photoshopLight", true}});
        QCOMPARE(neutral.pixelColor(0, 0).rgba64(), fine.pixelColor(0, 0).rgba64());
    }

    void layeredPsdOpensAndImportsAsComposite()
    {
        // Independent 8-bit PackBits fixture written by ImageMagick.
        const auto path = QFINDTESTDATA("fixtures/offset-layers.psd");
        QVERIFY(!path.isEmpty());
        const auto url = QUrl::fromLocalFile(path);
        Document document;
        QVERIFY(document.canImportLayerFiles({url}));
        QVERIFY(document.openFile(url));
        QCOMPARE(document.layerCount(), 2);
        QVERIFY(document.projectUrl().isEmpty());
        QCOMPARE(document.layers().first().toMap()["name"].toString(), QString("Patch"));
        QCOMPARE(document.layers().last().toMap()["name"].toString(), QString("Fond"));
        QCOMPARE(document.layerImage(1).pixelColor(0, 0).alpha(), 0);
        QCOMPARE(document.composite().pixelColor(2, 1), QColor(Qt::blue));
        document.setLayerVisible(1, false);
        QCOMPARE(document.composite().pixelColor(2, 1), QColor(Qt::red));
        QTemporaryDir temp;
        const auto project = QUrl::fromLocalFile(temp.filePath("import.pcr"));
        QVERIFY(document.saveProject(project));
        Document reopened;
        QVERIFY(reopened.openFile(project));
        QCOMPARE(reopened.layerCount(), 2);
        QCOMPARE(reopened.composite().pixelColor(2, 1), QColor(Qt::red));
        QVERIFY(reopened.importLayer(url));
        QCOMPARE(reopened.layerCount(), 3);
        QCOMPARE(reopened.composite().pixelColor(2, 1), QColor(Qt::blue));
        QCOMPARE(reopened.composite().pixelColor(0, 0), QColor(Qt::red));
    }

    void unsupportedPsdBlendKeepsPixelsAndReportsEachLayer()
    {
        QFile source(QFINDTESTDATA("fixtures/offset-layers.psd"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        auto data = source.readAll();
        QVERIFY(data.contains("8BIMmron"));
        data.replace("8BIMmron", "8BIMtest");
        QTemporaryDir temp;
        QFile file(temp.filePath("fallback.psd"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(data), data.size());
        file.close();
        Document doc;
        QSignalSpy report(&doc, &Document::psdImportReport);
        QVERIFY(doc.openFile(QUrl::fromLocalFile(file.fileName())));
        QCOMPARE(doc.layerCount(), 2);
        QCOMPARE(report.size(), 1);
        QCOMPARE(report.first()[1].toInt(), 2);
        const auto details = report.first()[2].toStringList();
        QCOMPARE(details.size(), 2);
        QVERIFY(details[0].contains("Fond"));
        QVERIFY(details[1].contains("Patch"));
        QCOMPARE(doc.composite().pixelColor(0, 0), QColor(Qt::red));
        QCOMPARE(doc.composite().pixelColor(2, 1), QColor(Qt::blue));
    }

    void mixedPsdReportsOnlyOmittedLayerAfterAsyncOpen()
    {
        QFile source(QFINDTESTDATA("fixtures/offset-layers.psd"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        auto data = source.readAll();
        // Photoshop marks pixel data irrelevant for layers rendered by a special feature.
        const auto key = data.indexOf("8BIMmron");
        QVERIFY(key >= 0);
        data[key + 10] = char(24);
        QTemporaryDir temp;
        QFile file(temp.filePath("partial.psd"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(data), data.size());
        file.close();
        Document doc;
        QSignalSpy report(&doc, &Document::psdImportReport);
        QSignalSpy opened(&doc, &Document::fileOpened);
        doc.requestOpenFile(QUrl::fromLocalFile(file.fileName()));
        QTRY_COMPARE(report.size(), 1);
        QCOMPARE(opened.size(), 1);
        QCOMPARE(doc.layerCount(), 1);
        QCOMPARE(doc.layers().first().toMap()["name"].toString(), QString("Patch"));
        QCOMPARE(doc.layerImage(0).pixelColor(2, 1), QColor(Qt::blue));
        QCOMPARE(doc.layerImage(0).pixelColor(0, 0).alpha(), 0);
        QCOMPARE(report.first()[0].toString(), QString("partial.psd"));
        const auto notes = report.first()[2].toStringList();
        QCOMPARE(notes.size(), 1);
        QVERIFY(notes.first().contains("Fond"));
    }

    void masked16BitLayerAndMergedTransparency()
    {
        const auto path = QFINDTESTDATA("fixtures/masked-16bit.psd");
        QVERIFY(!path.isEmpty());
        const auto url = QUrl::fromLocalFile(path);
        Document doc;
        QVERIFY(doc.openFile(url));
        QCOMPARE(doc.channelBits(), 16);
        const auto layer = doc.layers().first().toMap();
        QCOMPARE(layer["name"].toString(), QString("Essai é"));
        QVERIFY(layer["hasMask"].toBool());
        QCOMPARE(layer["opacity"].toDouble(), 128 / 255.);
        QCOMPARE(doc.composite().pixelColor(0, 0).alpha(), 0);
        QVERIFY(doc.composite().pixelColor(1, 0).alpha() > 0);
        QVERIFY(doc.newDocument(2, 1, true, 16));
        QVERIFY(doc.importLayer(url));
        const auto color = doc.layerImage(1).pixelColor(1, 0).rgba64();
        QCOMPARE(color.red(), 0);
        QCOMPARE(color.blue(), 65535);
        QCOMPARE(color.alpha(), 32768);
    }

    void zipPsdPreserves16BitSamplesAndRejectsTruncation()
    {
        // Flattened grayscale image, two rows, ZIP with 16-bit delta prediction.
        const auto header = QByteArray::fromHex(
            "38425053 0001 000000000000 0001 00000002 00000002 0010 0001 "
            "00000000 00000000 00000000 0003");
        const auto encoded = qCompress(QByteArray::fromHex("1234 0001 9876 ffff")).mid(4);
        QTemporaryDir temp;
        QFile file(temp.filePath("gray.PSD"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(header + encoded);
        file.close();
        Document doc;
        const auto url = QUrl::fromLocalFile(file.fileName());
        QVERIFY(doc.openFile(url));
        QCOMPARE(doc.channelBits(), 16);
        QCOMPARE(doc.layerImage(0).pixelColor(1, 0).rgba64().red(), 0x1235);
        QCOMPARE(doc.layerImage(0).pixelColor(1, 1).rgba64().green(), 0x9875);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(header + encoded.first(4));
        file.close();
        QVERIFY(!doc.openFile(url));
        QCOMPARE(doc.layerCount(), 1);
        QCOMPARE(doc.layerImage(0).pixelColor(1, 1).rgba64().green(), 0x9875);
    }

    void paintOnModelSurvivesSaveAndRotation()
    {
        Document document;
        QVERIFY(document.newDocument(240, 160, true, 16));
        QVERIFY(document.importModel3DData(Model3DTest::modelFixture(), "Surface", {}));
        QTRY_VERIFY_WITH_TIMEOUT(!document.operationBusy(), 5000);
        const auto original = document.layerImage(1);
        const auto before   = document.historyIndex();
        document.setTool("brush");
        document.setColor(Qt::green);
        document.setBrushSize(12);
        document.beginStroke(5., 5.);
        document.endStroke();
        QCOMPARE(document.historyIndex(), before);

        const auto originalPreview = document.displayComposite();
        document.beginStroke(104.5, 88.5);
        document.continueStroke(112.5, 88.5);
        QTRY_VERIFY_WITH_TIMEOUT(document.displayComposite() != originalPreview, 5000);
        QCOMPARE(document.layerImage(1), original);
        document.cancelStroke();
        QTRY_COMPARE_WITH_TIMEOUT(document.displayComposite(), originalPreview, 5000);
        QCOMPARE(document.historyIndex(), before);

        document.beginStroke(104.5, 88.5);
        document.continueStroke(112.5, 88.5);
        document.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!document.operationBusy(), 5000);
        QCOMPARE(document.historyIndex(), before + 1);
        QCOMPARE(document.layerCount(), 2);
        const auto painted = document.layerImage(1);
        const auto ink     = painted.pixelColor(108, 88);
        QVERIFY(ink.green() > ink.red() && ink.alpha() == 255);
        QCOMPARE(painted.pixelColor(99, 99), original.pixelColor(99, 99));
        document.undo();
        QCOMPARE(document.layerImage(1), original);
        document.redo();
        QCOMPARE(document.layerImage(1), painted);

        QTemporaryDir dir;
        const auto    file = QUrl::fromLocalFile(dir.filePath("Peinture.pcr"));
        QVERIFY(document.saveProject(file));
        Document reopened;
        QVERIFY(reopened.openFile(file));
        QCOMPARE(reopened.layerImage(1), painted);
        const auto parameters = reopened.layers().first().toMap().value("parameters").toMap();
        const auto light      = parameters.value("light3d").toMap();
        auto       pose       = parameters.value("pose3d").toMap();
        pose["roll"]          = 90.;
        QVERIFY(reopened.editModel3D(1, pose, light));
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 5000);
        const auto rotated = reopened.layerImage(1).pixelColor(128, 91);
        QVERIFY(rotated.green() > rotated.red() && rotated.alpha() == 255);
        pose["yaw"] = 180.;
        QVERIFY(reopened.editModel3D(1, pose, light));
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 5000);
        const auto back = reopened.layerImage(1).pixelColor(111, 91);
        QVERIFY(back.red() > back.green() && back.alpha() == 255);
    }

    void denseGLBRetainsAllTriangles()
    {
        namespace Model3D = PhotoCrap::Model3D;
        const auto bytes  = Model3DTest::modelFixture(Model3DTest::Pose::Original, 200001);
        QString    error;
        const auto asset = Model3D::load(bytes, {}, &error);
        QVERIFY2(asset, qPrintable(error));
        QCOMPARE(Model3D::encoded(*asset), bytes);
        QCOMPARE(Model3D::storageBytes(*asset) - bytes.size(), 200001 * 12 + 3 * 32 + 4 * 4 * 4);
        QVERIFY(asset->primitives.first().vertexColors.isEmpty());

        const auto coloredBytes = Model3DTest::modelFixture(Model3DTest::Pose::Original, 1, true);
        const auto colored      = Model3D::load(coloredBytes, {}, &error);
        QVERIFY2(colored, qPrintable(error));
        QCOMPARE(colored->primitives.first().vertexColors.last(), QVector4D(.5f, .25f, 1.f, .5f));
        QCOMPARE(Model3D::storageBytes(*colored) - coloredBytes.size(), 12 + 3 * 48 + 4 * 4 * 4);
    }

    void denseModelPreviewPreservesTexturedGeometry()
    {
        namespace Model3D        = PhotoCrap::Model3D;
        const auto         asset = std::make_shared<Model3D::Asset>();
        Model3D::Primitive surface;
        surface.material.unlit     = true;
        surface.material.alphaMask = true;
        surface.material.texture   = QImage(256, 256, QImage::Format_ARGB32);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x) {
                const QRgb color = ((x / 4) ^ (y / 4)) & 1 ? qRgb(232, 80, 40) : qRgb(40, 144, 216);
                surface.material.texture.setPixel(x, y, (x > 100 && x < 156) ? 0 : color);
            }
        constexpr int cells  = 320;
        constexpr int stride = cells + 1;
        for (int y = 0; y <= cells; ++y)
            for (int x = 0; x <= cells; ++x) {
                const float     u = float(x) / cells, v = float(y) / cells;
                const float     px = u - .5f, py = v - .5f;
                Model3D::Vertex vertex;
                vertex.position = {px * .65f, py * .65f, .4f * (px * px + py * py)};
                vertex.uv       = {u, v};
                surface.vertices.append(vertex);
            }
        for (quint32 y = 0; y < cells; ++y)
            for (quint32 x = 0; x < cells; ++x) {
                const auto topLeft = y * stride + x;
                surface.triangles.append({topLeft, topLeft + 1, topLeft + stride});
                surface.triangles.append({topLeft + 1, topLeft + stride + 1, topLeft + stride});
            }
        asset->primitives.append(std::move(surface));
        Model3D::Primitive backing;
        backing.material.unlit = true;
        backing.material.color = {1.f, .8f, 0.f, 1.f};
        for (const auto &corner : {QVector3D(-.25f, -.25f, -.1f), QVector3D(.25f, -.25f, -.1f),
                                   QVector3D(-.25f, .25f, -.1f), QVector3D(.25f, .25f, -.1f)}) {
            Model3D::Vertex vertex;
            vertex.position = corner;
            backing.vertices.append(vertex);
        }
        backing.triangles    = {{0, 1, 2}, {1, 3, 2}};
        backing.vertexColors = {{1, .7f, 1, 1}, {.4f, 1, 1, 1}, {.8f, .5f, 1, 1}, {1, 1, 1, 1}};
        asset->primitives.append(std::move(backing));
        const QSize canvas(640, 960);
        const QRect bounds(QPoint{}, canvas);
        const auto  format = QImage::Format_ARGB32_Premultiplied;
        auto        pose   = Model3D::pose(canvas);
        pose["yaw"]        = 30.;
        const QVariantMap params{{"pose3d", pose}, {"light3d", Model3D::light()}};
        if (qEnvironmentVariableIsSet("PHOTOCRAP_MODEL_PREVIEW_PROBE")) {
            QElapsedTimer timer;
            timer.start();
            for (const bool parallel : {false, true}) {
                timer.restart();
                for (int yaw : {10, 20, 30, 40, 50, 60, 70, 80}) {
                    pose["yaw"] = yaw;
                    const QVariantMap frame{{"pose3d", pose}, {"light3d", Model3D::light()}};
                    const auto        image =
                        parallel ? Model3D::renderSoftwarePreview(*asset, frame, canvas, format)
                                 : Model3D::render(*asset, frame, canvas, format);
                    QVERIFY(!image.isNull());
                }
                qInfo() << "CPU rotation, eight frames (ms), parallel:" << parallel
                        << timer.elapsed();
            }
        }
        const auto reference = Model3D::render(*asset, params, canvas, format).region(bounds);
        const auto preview   = Model3D::renderPreview(asset, params, canvas, format).region(bounds);
        QVERIFY(!reference.isNull() && !preview.isNull());
        QCOMPARE(Model3D::renderSoftwarePreview(*asset, params, canvas, format).region(bounds),
                 reference);
        const auto deep = QImage::Format_RGBA64_Premultiplied;
        QCOMPARE(Model3D::renderSoftwarePreview(*asset, params, canvas, deep).region(bounds),
                 Model3D::render(*asset, params, canvas, deep).region(bounds));
        int visible = 0, different = 0;
        for (int y = 0; y < canvas.height(); ++y) {
            const auto *expected = reinterpret_cast<const QRgb *>(reference.constScanLine(y));
            const auto *actual   = reinterpret_cast<const QRgb *>(preview.constScanLine(y));
            for (int x = 0; x < canvas.width(); ++x) {
                if (!qAlpha(expected[x]) && !qAlpha(actual[x]))
                    continue;
                ++visible;
                if (qAlpha(expected[x]) != qAlpha(actual[x]) ||
                    qAbs(qRed(expected[x]) - qRed(actual[x])) > 2 ||
                    qAbs(qGreen(expected[x]) - qGreen(actual[x])) > 2 ||
                    qAbs(qBlue(expected[x]) - qBlue(actual[x])) > 2)
                    ++different;
            }
        }
        QVERIFY(visible > 10000);
        QVERIFY2(different < visible / 100,
                 qPrintable(QString("%1 mismatched pixels out of %2").arg(different).arg(visible)));
    }

    void large3DRenderMatchesNativeCropAcrossBands()
    {
        namespace Model3D = PhotoCrap::Model3D;
        const auto asset  = Model3D::load(Model3DTest::modelFixture());
        QVERIFY(asset);
        const QSize canvas(2800, 2200);
        const QRect crop(700, 890, 1150, 600);
        const auto  format = QImage::Format_RGBA64_Premultiplied;
        QVariantMap params{
            {"pose3d", Model3D::fittedPose(*asset, QRectF(QPointF{}, QSizeF(canvas)))},
            {"light3d", Model3D::light()}};
        const auto image = Model3D::render(*asset, params, canvas, format);
        QVERIFY(!image.isNull());
        QVERIFY(
            Model3D::transformPlacement(params, QTransform::fromTranslate(-crop.x(), -crop.y())));
        const auto reference = Model3D::render(*asset, params, crop.size(), format);
        QVERIFY(!reference.isNull());
        const auto native = reference.region(QRect(QPoint{}, crop.size()));
        QVERIFY(native.pixelColor(20, 20).alpha() > 0);
        QCOMPARE(image.region(crop), native);
    }

    void skinnedModelUsesDefaultJointPose()
    {
        QTemporaryDir dir;
        Document      document;
        QVERIFY(document.newDocument(240, 160, true, 16));
        using Model3DTest::Pose;
        int layer = 0;
        for (auto pose : {Pose::Skinned, Pose::Deformed, Pose::Original}) {
            QFile glb(dir.filePath(QString::number(int(pose)) + ".glb"));
            QVERIFY(glb.open(QIODevice::WriteOnly));
            const auto fixture = Model3DTest::modelFixture(pose);
            QVERIFY(!fixture.isEmpty());
            QCOMPARE(glb.write(fixture), fixture.size());
            glb.close();
            QVERIFY(document.importModel3D(QUrl::fromLocalFile(glb.fileName())));
            QTRY_VERIFY_WITH_TIMEOUT(!document.operationBusy(), 5000);
            QCOMPARE(document.activeLayer(), ++layer);
        }
        QCOMPARE(document.layerImage(1), document.layerImage(2));
        QVERIFY(document.layerImage(1) != document.layerImage(3));
    }

    void editable3DModelsShareLightingAndSurviveProjectReload()
    {
        QTemporaryDir dir;
        const auto    path = dir.filePath("Objet.glb");
        QFile         glb(path);
        QVERIFY(glb.open(QIODevice::WriteOnly));
        const auto fixture = Model3DTest::modelFixture();
        QVERIFY(!fixture.isEmpty());
        QCOMPARE(glb.write(fixture), fixture.size());
        glb.close();

        Document d;
        QVERIFY(d.newDocument(240, 160, true, 16));
        const auto url = QUrl::fromLocalFile(path);
        QVERIFY(d.importModel3D(url));
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.layers().first().toMap()["kind"].toString(), QString("model3d"));
        const auto first = d.layerImage(1);
        QCOMPARE(first.format(), QImage::Format_RGBA64_Premultiplied);
        const auto sample = first.pixelColor(108, 88);
        QCOMPARE(sample.alpha(), 255);
        QVERIFY(sample.red() > sample.green() && sample.green() > sample.blue());
        QVERIFY(sample.rgba64().red() % 257 || sample.rgba64().green() % 257 ||
                sample.rgba64().blue() % 257);
        QCOMPARE(first.pixelColor(0, 0).alpha(), 0);

        QVERIFY(d.importModel3D(url));
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(d.layers().size(), 3);
        const auto info = d.layers().first().toMap()["parameters"].toMap();
        auto       pose = info["pose3d"].toMap(), light = info["light3d"].toMap();
        pose["x"]   = 177.;
        pose["yaw"] = 42.;
        QVERIFY(d.editModel3D(2, pose, light));
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        const auto second = d.layerImage(2);
        QVERIFY(second != first);
        QCOMPARE(d.layerImage(1), first);
        d.addLayerMask(true);
        QVERIFY(!d.maskImage(2).isNull());
        d.setEditingMask(false);
        light["color"]         = "#4080ff";
        light["ambient"]       = 12.;
        const auto history     = d.historyIndex();
        const auto beforeLight = d.displayComposite();
        d.previewModel3D(2, pose, light);
        QTRY_VERIFY_WITH_TIMEOUT(d.displayComposite() != beforeLight, 5000);
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.layerImage(1), first);
        QCOMPARE(d.layerImage(2), second);
        d.cancelModel3DPreview();
        QTRY_COMPARE_WITH_TIMEOUT(d.displayComposite(), beforeLight, 5000);
        QVERIFY(d.editModel3D(2, pose, light));
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(d.historyIndex(), history + 1);
        QVERIFY(d.layerImage(1) != first);
        QVERIFY(d.layerImage(2) != second);
        QVERIFY(!d.maskImage(2).isNull());
        d.undo();
        QCOMPARE(d.layerImage(1), first);
        QCOMPARE(d.layerImage(2), second);
        d.redo();

        const auto project = QUrl::fromLocalFile(dir.filePath("Scene.pcr"));
        QVERIFY(d.saveProject(project));
        QVERIFY(glb.remove());
        Document reopened;
        QVERIFY(reopened.openFile(project));
        QCOMPARE(reopened.layers().size(), 3);
        QCOMPARE(reopened.layerImage(2), d.layerImage(2));
        QCOMPARE(reopened.layerImage(1), d.layerImage(1));
        pose["yaw"] = -42.;
        QVERIFY(reopened.editModel3D(2, pose, light));
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 5000);
        QVERIFY(reopened.layerImage(2) != d.layerImage(2));
        QCOMPARE(reopened.layerImage(1), d.layerImage(1));

        // A model that has left the canvas must still be movable from its mesh.
        reopened.setEditingMask(false);
        reopened.setTool("move");
        const auto visibleModel = reopened.layerImage(2);
        const int  beforeMove   = reopened.historyIndex();
        reopened.beginStroke(120, 80);
        reopened.continueStroke(520, 80);
        reopened.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 5000);
        QCOMPARE(reopened.historyIndex(), beforeMove + 1);
        QVERIFY(reopened.layerImage(2) != visibleModel);
        reopened.beginStroke(120, 80);
        reopened.continueStroke(-280, 80);
        reopened.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 5000);
        QCOMPARE(reopened.layerImage(2), visibleModel);
        reopened.undo();
        QVERIFY(reopened.layerImage(2) != visibleModel);
        reopened.redo();
        QCOMPARE(reopened.layerImage(2), visibleModel);
        const int beforeCopy = reopened.historyIndex();
        reopened.beginStroke(120, 80, true);
        reopened.continueStroke(135, 85);
        reopened.cancelStroke();
        QCOMPARE(reopened.historyIndex(), beforeCopy);
        QCOMPARE(reopened.layers().size(), 3);
        reopened.beginStroke(120, 80, true);
        reopened.continueStroke(135, 85);
        reopened.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 5000);
        QCOMPARE(reopened.historyIndex(), beforeCopy + 1);
        QCOMPARE(reopened.activeLayer(), 3);
        QCOMPARE(reopened.layers().size(), 4);
        QCOMPARE(reopened.layerImage(2), visibleModel);
        QVERIFY(reopened.layerImage(3) != visibleModel);
    }

    void insertionAndNativeDepth_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<int>("insertion");
        for (int bits : {8, 16})
            for (int insertion : {0, 1, 2})
                QTest::newRow(qPrintable(QString("%1-bit-at-%2").arg(bits).arg(insertion)))
                    << bits << insertion;
    }
    void insertionAndNativeDepth()
    {
        QFETCH(int, bits);
        QFETCH(int, insertion);
        QTemporaryDir dir;
        QImage        image(8, 6,
                            bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                       : QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor::fromRgba64(12345, 23456, 34567, 45678));
        const auto file = QUrl::fromLocalFile(dir.filePath("Portrait.png"));
        QVERIFY(image.save(file.toLocalFile()));
        Document d;
        d.setBackgroundEditThreshold(-1);
        QVERIFY(d.newDocument(16, 12, true));
        d.addLayer();
        d.renameLayer(0, "Bottom");
        d.renameLayer(1, "Top");
        d.addLayerMask();
        d.setEditingMask(true);
        const auto before  = d.composite();
        const int  history = d.historyIndex();
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.requestImportLayers({file}, insertion);
        QVERIFY(d.operationBusy());
        QCOMPARE(d.layers().size(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QVERIFY(errors.isEmpty());
        QCOMPARE(d.channelBits(), bits);
        QCOMPARE(d.layers().size(), 3);
        QCOMPARE(d.activeLayer(), insertion);
        QCOMPARE(d.historyIndex(), history + 1);
        const auto layer = d.layers().at(2 - insertion).toMap();
        QCOMPARE(layer["name"].toString(), "Portrait");
        QVERIFY(!layer["editingMask"].toBool());
        const auto imported = d.layerImage(insertion);
        QCOMPARE(imported.pixelColor(0, 0).alpha(), 0);
        const auto decoded = QImage(file.toLocalFile()).convertToFormat(image.format());
        QCOMPARE(imported.format(), image.format());
        for (int y = 0; y < decoded.height(); ++y)
            for (int x = 0; x < decoded.width(); ++x)
                QCOMPARE(imported.pixelColor(x + 4, y + 3), decoded.pixelColor(x, y));
        d.undo();
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.channelBits(), 8);
        QCOMPARE(d.composite(), before);
        d.redo();
        QCOMPARE(d.layers().size(), 3);
        QCOMPARE(d.layerImage(insertion), imported);
    }
    void batchOrderAndFailureAreAtomic()
    {
        QTemporaryDir dir;
        QList<QUrl>   files;
        for (const auto name : {"First", "Second"}) {
            const auto file = QUrl::fromLocalFile(dir.filePath(QString(name) + ".png"));
            QImage     image(8, 6, QImage::Format_RGB32);
            image.fill(Qt::red);
            QVERIFY(image.save(file.toLocalFile()));
            files.append(file);
        }
        Document d;
        QVERIFY(d.newDocument(16, 12, true, 16));
        const int history = d.historyIndex();
        d.requestImportLayers(files);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(d.layers().size(), 3);
        QCOMPARE(d.layers()[0].toMap()["name"].toString(), "First");
        QCOMPARE(d.layers()[1].toMap()["name"].toString(), "Second");
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        QCOMPARE(d.layers().size(), 1);
        const auto before = d.composite();
        const auto bad    = QUrl::fromLocalFile(dir.filePath("broken.png"));
        QFile      broken(bad.toLocalFile());
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("not an image");
        broken.close();
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.requestImportLayers({files[0], bad});
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.composite(), before);
        QVERIFY(d.canRedo());
    }
    void rawImportAndRejectedFiles()
    {
        QTemporaryDir dir;
        const auto    raw = QUrl::fromLocalFile(dir.filePath("Camera.dng"));
        QFile         file(raw.toLocalFile());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(RawTestImage::dng());
        file.close();
        Document d;
        QVERIFY(d.newDocument(128, 96, true));
        QVERIFY(d.canImportLayerFiles({raw}));
        QVERIFY(!d.canImportLayerFiles({}));
        QVERIFY(!d.canImportLayerFiles({QUrl("https://example.com/image.png")}));
        QVERIFY(!d.canImportLayerFiles({QUrl::fromLocalFile(dir.path())}));
        QVERIFY(!d.canImportLayerFiles({QUrl::fromLocalFile(dir.filePath("missing.png"))}));
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.requestImportLayers({raw});
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QVERIFY(errors.isEmpty());
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.channelBits(), 16);
        QCOMPARE(d.layers()[0].toMap()["name"].toString(), "Camera");
    }
    void cancellationLeavesStackUntouched()
    {
        QTemporaryDir dir;
        const auto    file = QUrl::fromLocalFile(dir.filePath("Image.png"));
        QImage        image(8, 6, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(file.toLocalFile()));
        Document d;
        QVERIFY(d.newDocument(16, 12, true));
        const int history = d.historyIndex();
        auto     *pool    = QThreadPool::globalInstance();
        pool->waitForDone();
        const int maximum = pool->maxThreadCount();
        pool->setMaxThreadCount(1);
        QSemaphore entered, release;
        pool->start([&] {
            entered.release();
            release.acquire();
        });
        entered.acquire();
        d.requestImportLayers({file});
        const bool busy = d.operationBusy();
        d.cancelOperation();
        release.release();
        pool->waitForDone();
        pool->setMaxThreadCount(maximum);
        QVERIFY(busy);
        QTRY_VERIFY(!d.operationBusy());
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.historyIndex(), history);
    }
    void groupBoundaries()
    {
        QTemporaryDir dir;
        const auto    file = QUrl::fromLocalFile(dir.filePath("Image.png"));
        QImage        image(8, 6, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(file.toLocalFile()));
        Document d;
        QVERIFY(d.newDocument(16, 12, true));
        d.addLayer();
        d.selectLayer(0, true);
        d.groupSelectedLayers();
        const auto group = d.layers()[0].toMap()["groupId"].toString();
        QVERIFY(!group.isEmpty());
        d.requestImportLayers({file}, 1);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(d.layers()[2].toMap()["groupId"].toString(), group);
        d.requestImportLayers({file}, 0);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QVERIFY(d.layers().last().toMap()["groupId"].toString().isEmpty());
    }
};
QTEST_MAIN(LayerImportTest)
#include "LayerImportTest.moc"
