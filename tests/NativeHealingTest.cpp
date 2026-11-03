#include "Document.h"
#include "tools/ContentFill.h"
#include "tools/RegionHealing.h"
#include <QColorSpace>
#include <QPainter>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

namespace
{
struct BlockPool {
    QThreadPool *pool    = QThreadPool::globalInstance();
    int          maximum = pool->maxThreadCount();
    QSemaphore   entered, release;
    BlockPool()
    {
        pool->waitForDone();
        pool->setMaxThreadCount(1);
        pool->start([this] {
            entered.release();
            release.acquire();
        });
        entered.acquire();
    }
    ~BlockPool()
    {
        release.release();
        pool->waitForDone();
        pool->setMaxThreadCount(maximum);
    }
};
QImage fixture(bool _pupil = false, bool _object = false)
{
    QImage image(180, 160, QImage::Format_RGBA64_Premultiplied);
    image.setColorSpace(QColorSpace::SRgb);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const ushort tone = ushort(40001 + (y % 4) * 37 + (x % 3) * 11);
            reinterpret_cast<QRgba64 *>(image.scanLine(y))[x] =
                QRgba64::fromRgba64(tone, tone, tone, 65535);
        }
    QPainter painter(&image);
    if (_pupil) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor::fromRgba64(60001, 11335, 12349, 65535));
        painter.drawEllipse(QRect(76, 76, 16, 16));
    } else
        painter.fillRect(QRect(82, 82, 4, 4), _object
                                                  ? QColor::fromRgba64(54321, 12345, 23456, 65535)
                                                  : QColor(Qt::black));
    return image;
}
void select(Document &_document)
{
    _document.setTool("select");
    _document.beginStroke(76, 76);
    _document.continueStroke(92, 92);
    _document.endStroke();
}
} // namespace
class NativeHealingTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QThreadPool::globalInstance()->setMaxThreadCount(2);
    }
    void toolsRepairNativePixels_data()
    {
        QTest::addColumn<QString>("tool");
        QTest::addColumn<bool>("all");
        for (const QString tool :
             {QString("healing"), QString("spotHealing"), QString("remove"), QString("patch"),
              QString("contentAwareMove"), QString("redEye")})
            for (bool all : {false, true})
                QTest::newRow(qPrintable(tool + (all ? "-composite" : "-active"))) << tool << all;
    }
    void toolsRepairNativePixels()
    {
        QFETCH(QString, tool);
        QFETCH(bool, all);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("native.png");
        QVERIFY(fixture(tool == "redEye", tool == "contentAwareMove").save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setBackgroundEditThreshold(-1);
        QVERIFY(d.toolSupportsDepth(tool));
        QCOMPARE(d.channelBits(), 16);
        d.setHealingSampleMode(all ? 2 : 0);
        d.setSpotHealingSampleAllLayers(all);
        d.setRemoveSampleAllLayers(all);
        d.setContentMoveSampleAllLayers(all);
        d.setBrushSize(24);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        d.setHealingSource(34, 84);
        if (tool == "patch" || tool == "contentAwareMove")
            select(d);
        const auto original  = d.layerImage(0);
        const auto selection = d.selectionRegion();
        const int  history   = d.historyIndex();
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.setTool(tool);
        QCOMPARE(d.tool(), tool);
        QRegion affected(QRect(68, 68, 32, 32));
        if (tool == "patch" || tool == "contentAwareMove") {
            const int dx = tool == "patch" ? -50 : 50;
            affected     = tool == "patch" ? selection : selection | selection.translated(dx, 0);
            d.beginStroke(84, 84);
            d.continueStroke(84 + dx, 84);
            d.endStroke();
            QTRY_VERIFY_WITH_TIMEOUT(!d.patchBusy() && !d.contentMoveBusy(), 10000);
            QTRY_VERIFY_WITH_TIMEOUT(!d.patchDragging(), 10000);
        } else if (tool == "remove") {
            d.setRemoveAfterEachStroke(false);
            d.beginStroke(84, 84);
            d.endStroke();
            d.applyRemoval();
            QTRY_VERIFY_WITH_TIMEOUT(!d.removeBusy(), 10000);
        } else {
            d.beginStroke(84, 84);
            d.endStroke();
        }
        QVERIFY(errors.isEmpty());
        const auto after = d.layerImage(0);
        QCOMPARE(after.format(), QImage::Format_RGBA64_Premultiplied);
        QCOMPARE(d.channelBits(), 16);
        QVERIFY(after != original);
        QCOMPARE(d.historyIndex(), history + 1);
        bool changedWithNativeDetail = false;
        for (int y = 0; y < after.height(); ++y)
            for (int x = 0; x < after.width(); ++x) {
                const auto a = reinterpret_cast<const QRgba64 *>(after.constScanLine(y))[x];
                const auto b = reinterpret_cast<const QRgba64 *>(original.constScanLine(y))[x];
                if (!affected.contains(QPoint(x, y)))
                    QCOMPARE(a, b);
                if (a != b && (a.red() % 257 || a.green() % 257 || a.blue() % 257))
                    changedWithNativeDetail = true;
            }
        QVERIFY(changedWithNativeDetail);
        if (tool != "redEye" && tool != "contentAwareMove")
            QVERIFY(after.pixelColor(84, 84).rgba64().red() > 35000);
        if (tool == "contentAwareMove")
            QCOMPARE(after.pixelColor(134, 84).rgba64(), original.pixelColor(84, 84).rgba64());
        d.undo();
        QCOMPARE(d.layerImage(0), original);
        d.redo();
        QCOMPARE(d.layerImage(0), after);
        const auto project = QUrl::fromLocalFile(directory.filePath("native.pcr"));
        QVERIFY(d.saveProject(project));
        Document loaded;
        QVERIFY(loaded.openFile(project));
        QCOMPARE(loaded.channelBits(), 16);
        QCOMPARE(loaded.layerImage(0), after);
    }
    void nativeBrushModesAndSoftSelections_data()
    {
        QTest::addColumn<QString>("tool");
        QTest::addColumn<int>("mode");
        for (const QString tool : {QString("healing"), QString("spotHealing")})
            for (int mode = 0; mode < 8; ++mode)
                QTest::newRow(qPrintable(tool + QString::number(mode))) << tool << mode;
    }
    void nativeBrushModesAndSoftSelections()
    {
        QFETCH(QString, tool);
        QFETCH(int, mode);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("modes.png");
        QVERIFY(fixture().save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        select(d);
        d.modifySelection("feather", 3);
        QTRY_VERIFY_WITH_TIMEOUT(!d.selectionBusy(), 5000);
        const auto selection = d.selectionRegion();
        const auto coverage  = d.selectionMask();
        const auto before    = d.layerImage(0);
        const auto history   = d.historyIndex();
        d.setHealingSource(34, 84);
        d.setHealingMode(mode);
        d.setSpotHealingMode(mode);
        d.setHealingDiffusion(4);
        d.setBrushSize(27);
        d.setBrushHardness(.51);
        d.setBrushFlow(.57);
        d.setBrushOpacity(.63);
        d.setBrushSmoothing(0);
        d.setTool(tool);
        d.beginStroke(84.25, 84.75);
        d.continueStroke(86.5, 85.25);
        d.endStroke();
        const auto after = d.layerImage(0);
        QCOMPARE(after.format(), QImage::Format_RGBA64_Premultiplied);
        QCOMPARE(d.historyIndex(), history + (after != before ? 1 : 0));
        for (int y = 0; y < after.height(); ++y)
            for (int x = 0; x < after.width(); ++x) {
                const auto a = reinterpret_cast<const QRgba64 *>(after.constScanLine(y))[x];
                const auto b = reinterpret_cast<const QRgba64 *>(before.constScanLine(y))[x];
                if (!selection.contains(QPoint(x, y)) || coverage.constScanLine(y)[x] == 0)
                    QCOMPARE(a, b);
            }
        if (after != before) {
            d.undo();
            QCOMPARE(d.layerImage(0), before);
            d.redo();
            QCOMPARE(d.layerImage(0), after);
        }
        d.beginStroke(84, 84);
        d.continueStroke(90, 84);
        d.cancelStroke();
        QCOMPARE(d.layerImage(0), after);
    }
    void compositedRepairsCanPaintAnEmptyNativeLayer_data()
    {
        QTest::addColumn<QString>("tool");
        for (const QString tool : {QString("healing"), QString("spotHealing"), QString("remove"),
                                   QString("contentAwareMove")})
            QTest::newRow(qPrintable(tool)) << tool;
    }
    void compositedRepairsCanPaintAnEmptyNativeLayer()
    {
        QFETCH(QString, tool);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("layers.png");
        QVERIFY(fixture(false, tool == "contentAwareMove").save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        const auto base = d.layerImage(0);
        d.addLayer();
        const auto empty = d.layerImage(1);
        d.setHealingSource(34, 84);
        d.setHealingSampleMode(1);
        d.setSpotHealingSampleAllLayers(true);
        d.setRemoveSampleAllLayers(true);
        d.setContentMoveSampleAllLayers(true);
        d.setBrushSize(24);
        d.setBrushHardness(1);
        d.setBrushSmoothing(0);
        if (tool == "contentAwareMove")
            select(d);
        const auto history = d.historyIndex();
        d.setTool(tool);
        d.beginStroke(84, 84);
        if (tool == "contentAwareMove")
            d.continueStroke(134, 84);
        d.endStroke();
        if (tool == "remove")
            d.applyRemoval();
        QTRY_VERIFY_WITH_TIMEOUT(!d.removeBusy() && !d.contentMoveBusy(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!d.patchDragging(), 10000);
        const auto after = d.layerImage(1);
        QVERIFY(after != empty);
        QCOMPARE(after.format(), QImage::Format_RGBA64_Premultiplied);
        QCOMPARE(d.layerImage(0), base);
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        QCOMPARE(d.layerImage(1), empty);
        d.redo();
        QCOMPARE(d.layerImage(1), after);
    }
    void nativeBrushesAndRedEyeKeepTheirTileFootprint()
    {
        for (const QString tool : {QString("healing"), QString("spotHealing"), QString("redEye")}) {
            Document d;
            QVERIFY(d.newDocument(6000, 4000, false, 16));
            d.setTool("pencil");
            d.setBrushSize(9);
            d.setColor(tool == "redEye" ? QColor::fromRgba64(60001, 11335, 12349, 65535)
                                        : QColor(Qt::black));
            d.beginStroke(256.5, 256.5);
            d.endStroke();
            const auto before = d.gpuLayerStack().layers.first().image;
            const auto heap   = d.memoryUsage()["tileHeapBytes"].toLongLong();
            d.setTool(tool);
            d.setHealingSource(100, 100);
            d.setBrushSize(24);
            d.setBrushHardness(1);
            d.beginStroke(256.5, 256.5);
            d.endStroke();
            QVERIFY(d.memoryUsage()["tileHeapBytes"].toLongLong() - heap <= 4LL * 256 * 256 * 8);
            const auto after = d.gpuLayerStack().layers.first().image;
            QVERIFY(after.region({250, 250, 14, 14}) != before.region({250, 250, 14, 14}));
            QCOMPARE(after.region({1000, 1000, 1, 1}), before.region({1000, 1000, 1, 1}));
            d.undo();
            QCOMPARE(d.gpuLayerStack().layers.first().image.region({250, 250, 14, 14}),
                     before.region({250, 250, 14, 14}));
        }
    }
    void queuedNativeRepairsDiscardReplacedDocuments_data()
    {
        QTest::addColumn<QString>("tool");
        for (const QString tool :
             {QString("remove"), QString("patch"), QString("contentAwareMove")})
            QTest::newRow(qPrintable(tool)) << tool;
    }
    void queuedNativeRepairsDiscardReplacedDocuments()
    {
        QFETCH(QString, tool);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("pending.png");
        QVERIFY(fixture().save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        if (tool != "remove")
            select(d);
        d.setTool(tool);
        d.setRemoveAfterEachStroke(false);
        d.setBrushSize(24);
        {
            BlockPool blocked;
            d.beginStroke(84, 84);
            if (tool != "remove")
                d.continueStroke(134, 84);
            d.endStroke();
            if (tool == "remove")
                d.applyRemoval();
            QVERIFY(d.removeBusy() || d.patchBusy() || d.contentMoveBusy());
            QVERIFY(d.newDocument(23, 19, false, 16));
        }
        QCoreApplication::processEvents();
        QVERIFY(!d.removeBusy() && !d.patchBusy() && !d.contentMoveBusy());
        QCOMPARE(d.channelBits(), 16);
        QCOMPARE(d.width(), 23);
        QCOMPARE(d.height(), 19);
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.historyIndex(), 0);
        QVERIFY(!d.modified());
        QCOMPARE(d.composite().pixelColor(10, 10), QColor(Qt::white));
    }
    void nativeRegionBlendMatchesSourceOverAndCopiesLowBits()
    {
        QImage image(43, 37, QImage::Format_RGBA64_Premultiplied);
        image.setColorSpace(QColorSpace::SRgb);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                image.setPixelColor(
                    x, y,
                    QColor::fromRgba64(ushort(14001 + x * 379), ushort(15003 + y * 419),
                                       ushort(37001 + x * 181), ushort(31001 + y * 431)));
        const QRegion region(QRect(8, 9, 11, 13));
        const QPoint  offset(17, 0);
        auto          cancel = std::make_shared<std::atomic_bool>(false);
        const auto    copied =
            RegionHealing::blend(image, image, region, offset, 4, 0, false, 0, cancel);
        QCOMPARE(copied, image.copy(region.boundingRect().translated(offset)));
        QImage expected = image.copy(region.boundingRect());
        {
            QPainter painter(&expected);
            painter.drawImage(QPoint{}, copied);
        }
        QCOMPARE(RegionHealing::blend(image, image, region, offset, 4, 0, true, 0, cancel),
                 expected);
        cancel->store(true);
        QVERIFY(RegionHealing::blend(image, image, region, offset, 4, 1, true, 0, cancel).isNull());
    }
    void nativeBoundaryFallbackPreservesOriginalSamples()
    {
        QImage image(21, 17, QImage::Format_RGBA64_Premultiplied);
        image.setColorSpace(QColorSpace::SRgb);
        image.fill(QColor::fromRgba64(40001, 31003, 22007, 51001));
        const QRegion region(QRect(0, 1, 21, 16));
        auto          cancel = std::make_shared<std::atomic_bool>(false);
        const auto    fill   = ContentFill::reconstruct(image, region, region, 4, cancel);
        QCOMPARE(fill, image.copy(region.boundingRect()));
        cancel->store(true);
        QVERIFY(ContentFill::reconstruct(image, region, region, 4, cancel).isNull());
    }
};
QTEST_MAIN(NativeHealingTest)
#include "NativeHealingTest.moc"
