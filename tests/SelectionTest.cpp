#include "Document.h"
#include "selection/SelectionGeometry.h"
#include <QFile>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTimer>
#include <QtTest>
#include <limits>

namespace
{
struct SelectionPoolBlock {
    QThreadPool *pool    = QThreadPool::globalInstance();
    int          maximum = pool->maxThreadCount();
    QSemaphore   entered, release;
    SelectionPoolBlock()
    {
        pool->waitForDone();
        pool->setMaxThreadCount(1);
        pool->start([this] {
            entered.release();
            release.acquire();
        });
        entered.acquire();
    }
    ~SelectionPoolBlock()
    {
        release.release();
        pool->waitForDone();
        pool->setMaxThreadCount(maximum);
    }
};
void wandFixture(Document &_doc, int _bits)
{
    _doc.setBackgroundEditThreshold(-1);
    QVERIFY(_doc.newDocument(517, 523, false, _bits));
    _doc.setBrushSize(41);
    _doc.setBrushHardness(.6);
    _doc.setColor(QColor(210, 40, 70, 187));
    _doc.beginStroke(255, 255);
    _doc.endStroke();
    _doc.addAdjustmentLayer("boxBlur");
    _doc.setActiveLayer(0);
    _doc.setTool("ellipticalSelect");
    _doc.beginStroke(244, 244);
    _doc.continueStroke(281, 282);
    _doc.endStroke();
    _doc.setTool("magicWand");
    _doc.setSelectionTolerance(25);
}
void rectangle(Document &_document, int _x, int _y, int _w, int _h)
{
    _document.setTool("select");
    _document.beginStroke(_x, _y);
    _document.continueStroke(_x + _w, _y + _h);
    _document.endStroke();
}
QRegion reference(const QRegion &_region, const QSize &_size, const QString &_operation,
                  int _radius)
{
    QRegion output;
    for (int y = 0; y < _size.height(); ++y)
        for (int x = 0; x < _size.width(); ++x) {
            int selected = 0, total = 0, nearestInside = 1000000000, nearestOutside = 1000000000;
            for (int yy = 0; yy < _size.height(); ++yy)
                for (int xx = 0; xx < _size.width(); ++xx) {
                    const bool in       = _region.contains(QPoint(xx, yy));
                    const int  distance = (xx - x) * (xx - x) + (yy - y) * (yy - y);
                    if (in)
                        nearestInside = qMin(nearestInside, distance);
                    else
                        nearestOutside = qMin(nearestOutside, distance);
                    if (qAbs(xx - x) <= _radius && qAbs(yy - y) <= _radius) {
                        ++total;
                        selected += in;
                    }
                }
            const bool match = _operation == "expand"     ? nearestInside <= _radius * _radius
                               : _operation == "contract" ? nearestOutside > _radius * _radius
                               : _operation == "border"
                                   ? nearestInside <= (_radius / 2) * (_radius / 2) &&
                                         nearestOutside <= ((_radius + 1) / 2) * ((_radius + 1) / 2)
                                   : 2 * selected > total ||
                                         (2 * selected == total && _region.contains(QPoint(x, y)));
            if (match)
                output |= QRect(x, y, 1, 1);
        }
    return output;
}
} // namespace
class SelectionTest : public QObject
{
    Q_OBJECT
private slots:
    void backgroundWandMatchesSynchronousSelection_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<bool>("all");
        QTest::addColumn<bool>("contiguous");
        QTest::addColumn<int>("mode");
        QTest::addColumn<bool>("soft");
        QTest::addColumn<bool>("warm");
        for (int bits : {8, 16})
            for (bool all : {false, true})
                for (bool contiguous : {false, true})
                    for (int mode : {0, 1, 2, 3})
                        for (bool soft : {false, true})
                            for (bool warm : {false, true})
                                QTest::newRow(qPrintable(QString("%1-%2-%3-%4-%5-%6")
                                                             .arg(bits)
                                                             .arg(all)
                                                             .arg(contiguous)
                                                             .arg(mode)
                                                             .arg(soft)
                                                             .arg(warm)))
                                    << bits << all << contiguous << mode << soft << warm;
    }
    void backgroundWandMatchesSynchronousSelection()
    {
        QFETCH(int, bits);
        QFETCH(bool, all);
        QFETCH(bool, contiguous);
        QFETCH(int, mode);
        QFETCH(bool, soft);
        QFETCH(bool, warm);
        Document doc, reference;
        wandFixture(doc, bits);
        wandFixture(reference, bits);
        for (Document *current : {&doc, &reference}) {
            if (soft) {
                current->modifySelection("feather", 2);
                QTRY_VERIFY_WITH_TIMEOUT(!current->selectionBusy(), 5000);
            }
            current->setSelectionSampleAllLayers(all);
            current->setSelectionContiguous(contiguous);
            current->setSelectionMode(mode);
        }
        doc.setLayerVisible(0, false);
        doc.setLayerVisible(0, true);
        QTemporaryDir directory;
        QVERIFY(doc.saveProject(QUrl::fromLocalFile(directory.filePath("clean.pcr"))));
        const auto beforeRegion = doc.selectionRegion();
        const auto beforeMask   = doc.selectionMask();
        const auto beforePixels = doc.layerImage(0);
        const int  history      = doc.historyIndex();
        reference.beginStroke(255, 255);
        const auto expectedRegion = reference.selectionRegion();
        const auto expectedMask   = reference.selectionMask();
        if (warm)
            (void)doc.composite();
        doc.setBackgroundEditThreshold(0);
        {
            SelectionPoolBlock blocked;
            doc.beginStroke(255, 255);
            QVERIFY(doc.selectionBusy());
            QCOMPARE(doc.selectionRegion(), beforeRegion);
            QCOMPARE(doc.selectionMask(), beforeMask);
            QCOMPARE(doc.historyIndex(), history);
            bool tick = false;
            QTimer::singleShot(0, &doc, [&] { tick = true; });
            QTRY_VERIFY_WITH_TIMEOUT(tick, 1000);
            QVERIFY(doc.selectionBusy());
        }
        QTRY_VERIFY_WITH_TIMEOUT(!doc.selectionBusy(), 10000);
        QCOMPARE(doc.selectionRegion(), expectedRegion);
        QCOMPARE(doc.selectionMask(), expectedMask);
        QCOMPARE(doc.layerImage(0), beforePixels);
        QVERIFY(!doc.modified());
        if (beforeRegion != expectedRegion || beforeMask != expectedMask) {
            QCOMPARE(doc.historyIndex(), history + 1);
            doc.undo();
            QCOMPARE(doc.selectionRegion(), beforeRegion);
            QCOMPARE(doc.selectionMask(), beforeMask);
            doc.redo();
            QCOMPARE(doc.selectionRegion(), expectedRegion);
            QCOMPARE(doc.selectionMask(), expectedMask);
        }
    }
    void backgroundWandSharesAnAlreadyFinishedNativeComposite()
    {
        Document doc;
        QVERIFY(doc.newDocument(517, 523, false, 16));
        doc.setBrushSize(41);
        doc.setColor(Qt::red);
        doc.beginStroke(255, 255);
        doc.endStroke();
        doc.setTool("magicWand");
        doc.setSelectionSampleAllLayers(true);
        const QImage &cache = doc.composite();
        const auto    key   = cache.cacheKey();
        QVERIFY(cache.isDetached());
        doc.setBackgroundEditThreshold(0);
        {
            SelectionPoolBlock blocked;
            doc.beginStroke(255, 255);
            QVERIFY(doc.selectionBusy());
            QVERIFY(!cache.isDetached()); // The queued source shares these finished pixels.
            QCOMPARE(cache.cacheKey(), key);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!doc.selectionBusy(), 10000);
        QVERIFY(cache.isDetached());
        QCOMPARE(cache.cacheKey(), key);
    }

    void backgroundWandPreservesSubmittedOptions()
    {
        Document doc, reference;
        wandFixture(doc, 16);
        wandFixture(reference, 16);
        doc.setSelectionMode(1);
        reference.setSelectionMode(1);
        reference.beginStroke(255, 255);
        doc.setBackgroundEditThreshold(0);
        {
            SelectionPoolBlock blocked;
            doc.beginStroke(255, 255);
            QVERIFY(doc.selectionBusy());
            doc.setSelectionMode(2);
            doc.setSelectionTolerance(255);
            doc.setSelectionContiguous(false);
            doc.setSelectionSampleAllLayers(true);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!doc.selectionBusy(), 10000);
        QCOMPARE(doc.selectionRegion(), reference.selectionRegion());
        QCOMPARE(doc.selectionMode(), 2);
        QCOMPARE(doc.selectionTolerance(), 255);
        QVERIFY(!doc.selectionContiguous());
        QVERIFY(doc.selectionSampleAllLayers());
    }
    void backgroundWandCancellationAndStaleResults_data()
    {
        QTest::addColumn<QString>("action");
        QTest::addColumn<bool>("all");
        for (const QString action :
             {QString("cancel"), QString("undo"), QString("redo"), QString("replace"),
              QString("selection"), QString("pixels"), QString("gesture"), QString("new-wand")})
            for (bool all : {false, true})
                QTest::newRow(qPrintable(action + (all ? "-composite" : "-active")))
                    << action << all;
    }
    void backgroundWandCancellationAndStaleResults()
    {
        QFETCH(QString, action);
        QFETCH(bool, all);
        Document doc;
        wandFixture(doc, 16);
        doc.setSelectionSampleAllLayers(all);
        doc.setBackgroundEditThreshold(0);
        QRegion expected = doc.selectionRegion();
        int     history  = doc.historyIndex();
        {
            SelectionPoolBlock blocked;
            doc.beginStroke(255, 255);
            QVERIFY(doc.selectionBusy());
            if (action == "cancel")
                doc.cancelSelectionOperation();
            else if (action == "undo")
                doc.undo();
            else if (action == "redo")
                doc.redo();
            else if (action == "replace") {
                QVERIFY(doc.newDocument(31, 23, true));
                expected = doc.selectionRegion();
                history  = doc.historyIndex();
            } else if (action == "selection") {
                doc.selectAll();
                expected = doc.selectionRegion();
                history  = doc.historyIndex();
            } else if (action == "pixels") {
                doc.setLayerOpacity(0, .5);
                history = doc.historyIndex();
            } else if (action == "gesture") {
                doc.setTool("select");
                doc.beginStroke(10, 10);
                doc.continueStroke(20, 20);
                doc.endStroke();
                expected = doc.selectionRegion();
                history  = doc.historyIndex();
            } else {
                doc.beginStroke(500, 500);
                QVERIFY(doc.selectionBusy());
                expected = QRegion(QRect(0, 0, 517, 523));
            }
        }
        QTRY_VERIFY_WITH_TIMEOUT(!doc.selectionBusy(), 10000);
        if (action == "new-wand") {
            Document reference;
            wandFixture(reference, 16);
            reference.setSelectionSampleAllLayers(all);
            reference.beginStroke(500, 500);
            expected = reference.selectionRegion();
            QCOMPARE(doc.historyIndex(), history + 1);
        } else
            QCOMPARE(doc.historyIndex(), history);
        QCOMPARE(doc.selectionRegion(), expected);
        if (action == "replace") {
            QCOMPARE(doc.width(), 31);
            QCOMPARE(doc.height(), 23);
        }
    }
    void backgroundWandCanOutliveItsDocument_data()
    {
        QTest::addColumn<bool>("all");
        QTest::newRow("active") << false;
        QTest::newRow("composite") << true;
    }
    void backgroundWandCanOutliveItsDocument()
    {
        QFETCH(bool, all);
        SelectionPoolBlock blocked;
        auto               doc = std::make_unique<Document>();
        wandFixture(*doc, 16);
        doc->setSelectionSampleAllLayers(all);
        doc->setBackgroundEditThreshold(0);
        doc->beginStroke(255, 255);
        QVERIFY(doc->selectionBusy());
        doc.reset();
    }

    void lassoGuidesFollowOnlyActiveSelectionGestures()
    {
        Document doc;
        QVERIFY(doc.newDocument(120, 100));
        for (const auto &tool : {QString("lasso"), QString("patch"), QString("contentAwareMove")}) {
            doc.deselect();
            doc.setTool(tool);
            doc.beginStroke(10, 10);
            QVERIFY(doc.lassoPreviewActive());
            doc.continueStroke(70, 10);
            doc.continueStroke(50, 70);
            QVERIFY(doc.hasSelection());
            QVERIFY(doc.lassoPreviewActive());
            doc.endStroke();
            QVERIFY(!doc.lassoPreviewActive());
            QVERIFY(doc.hasSelection());
            doc.setTool("brush");
            doc.beginStroke(35, 30);
            QVERIFY(!doc.lassoPreviewActive());
            doc.cancelStroke();
        }
        doc.deselect();
        doc.setTool("polygonalLasso");
        doc.addPolygonLassoPoint(10, 10);
        doc.addPolygonLassoPoint(70, 10);
        doc.addPolygonLassoPoint(50, 70);
        QVERIFY(doc.lassoPreviewActive());
        doc.finishPolygonLasso();
        QVERIFY(!doc.lassoPreviewActive());
        const auto completed = doc.selectionPoints();
        doc.nudgeTool(1, 0);
        QVERIFY(!doc.lassoPreviewActive());
        QCOMPARE(doc.selectionPoints(), completed.translated(1, 0));
        doc.setSelectionMode(1);
        doc.setTool("lasso");
        doc.beginStroke(80, 10);
        doc.continueStroke(100, 10);
        doc.continueStroke(100, 40);
        QVERIFY(doc.lassoPreviewActive());
        doc.endStroke();
        QCOMPARE(doc.selectionShape(), 6);
        QVERIFY(!doc.lassoPreviewActive());
        doc.setSelectionMode(0);
        doc.setTool("polygonalLasso");
        doc.addPolygonLassoPoint(10, 10);
        QVERIFY(doc.lassoPreviewActive());
        doc.cancelPolygonLasso();
        QVERIFY(!doc.lassoPreviewActive());
    }
    void geometryMatchesPixelReference()
    {
        const QSize   size(17, 13);
        const QRegion shapes[] = {QRegion(QRect(3, 2, 9, 8)).subtracted(QRegion(QRect(6, 5, 2, 2))),
                                  QRegion(QRect(0, 0, 5, 7)) | QRegion(QRect(10, 8, 5, 4)) |
                                      QRegion(QRect(7, 4, 1, 1)),
                                  QRegion(QRect(QPoint{}, size))};
        auto          cancel   = std::make_shared<std::atomic_bool>(false);
        for (const auto &shape : shapes)
            for (const auto &operation :
                 {QString("expand"), QString("contract"), QString("border"), QString("smooth")})
                for (int radius : {1, 2, 4}) {
                    const auto result =
                        SelectionGeometry::modify(shape, size, operation, radius, cancel);
                    QVERIFY2(result.valid, qPrintable(operation));
                    // Qt can decompose the same region into different vertical bands.
                    QVERIFY2(
                        result.region.xored(reference(shape, size, operation, radius)).isEmpty(),
                        qPrintable(operation + QString::number(radius)));
                }
    }
    void parametersAndCancellation()
    {
        auto          cancel = std::make_shared<std::atomic_bool>(false);
        const QRegion shape(QRect(2, 2, 4, 4));
        QVERIFY(!SelectionGeometry::modify(shape, QSize(10, 10), "expand", 0, cancel).valid);
        QVERIFY(!SelectionGeometry::modify(shape, QSize(10, 10), "expand", 101, cancel).valid);
        QVERIFY(!SelectionGeometry::modify(shape, QSize(10, 10), "feather", 1, cancel).valid);
        QVERIFY(
            !SelectionGeometry::modify(shape, QSize(100000, 100000), "expand", 1, cancel).valid);
        cancel->store(true);
        QVERIFY(!SelectionGeometry::modify(shape, QSize(10, 10), "expand", 1, cancel).valid);
    }
    void historyAndSavedContentIdentity()
    {
        QTemporaryDir dir;
        Document      d;
        QVERIFY(d.newDocument(80, 60, true, 16));
        QVERIFY(d.saveProject(QUrl::fromLocalFile(dir.filePath("selection.pcr"))));
        QVERIFY(!d.modified());
        const auto image = d.composite();
        QSignalSpy imageChanges(&d, &Document::imageChanged);
        rectangle(d, 20, 20, 20, 15);
        const auto original = d.selectionRegion();
        QCOMPARE(d.historyIndex(), 1);
        QVERIFY(!d.modified());
        QVERIFY(!d.canReselect());
        d.deselect();
        QVERIFY(!d.hasSelection());
        QVERIFY(d.canReselect());
        d.undo();
        QCOMPARE(d.selectionRegion(), original);
        QVERIFY(!d.canReselect());
        QVERIFY(!d.modified());
        d.redo();
        QVERIFY(!d.hasSelection());
        QVERIFY(d.canReselect());
        d.reselect();
        QCOMPARE(d.selectionRegion(), original);
        d.invertSelection();
        QCOMPARE(d.selectionRegion(), QRegion(QRect(0, 0, 80, 60)).subtracted(original));
        QVERIFY(d.memoryUsage()["selectionBytes"].toLongLong() > 0);
        d.undo();
        QCOMPARE(d.selectionRegion(), original);
        QCOMPARE(imageChanges.size(), 0);
        QCOMPARE(d.composite().cacheKey(), image.cacheKey());
        d.setColor(Qt::red);
        d.fillWithColor(true);
        QVERIFY(d.modified());
        d.deselect();
        d.undo(); // Deselect only, keeping edited pixels dirty.
        QVERIFY(d.modified());
        d.undo(); // Fill: selection remains, persisted content is back at the saved state.
        QVERIFY(!d.modified());
        QCOMPARE(d.selectionRegion(), original);
        d.redo();
        QVERIFY(d.modified());
        QVERIFY(d.saveProject(QUrl::fromLocalFile(dir.filePath("selection.pcr"))));
        d.selectAll();
        d.undo();
        QVERIFY(!d.modified());
    }
    void asyncOperationsStayOffThePixelPath()
    {
        Document d;
        QVERIFY(d.newDocument(160, 120, true, 16));
        rectangle(d, 40, 30, 50, 40);
        const auto original = d.selectionRegion();
        const auto pixels   = d.composite();
        const int  before   = d.historyIndex();
        QSignalSpy changes(&d, &Document::imageChanged);
        d.modifySelection("expand", 5);
        QVERIFY(d.selectionBusy());
        QCOMPARE(d.selectionRegion(), original);
        QTRY_VERIFY_WITH_TIMEOUT(!d.selectionBusy(), 5000);
        const auto expanded = d.selectionRegion();
        QVERIFY(expanded.contains(QPoint(35, 50)));
        QVERIFY(!expanded.contains(QPoint(35, 25)));
        QCOMPARE(d.historyIndex(), before + 1);
        d.undo();
        QCOMPARE(d.selectionRegion(), original);
        d.redo();
        QCOMPARE(d.selectionRegion(), expanded);
        QCOMPARE(d.composite().cacheKey(), pixels.cacheKey());
        QCOMPARE(changes.size(), 0);
        const int current = d.historyIndex();
        d.modifySelection("contract", 100);
        QTRY_VERIFY_WITH_TIMEOUT(!d.selectionBusy(), 5000);
        QVERIFY(!d.hasSelection());
        QCOMPARE(d.historyIndex(), current + 1);
        d.undo();
        QCOMPARE(d.selectionRegion(), expanded);
    }
    void obsoleteAndCanceledJobsCannotReplaceTheSelection()
    {
        Document d;
        QVERIFY(d.newDocument(2400, 1600, true, 16));
        rectangle(d, 400, 300, 1500, 1000);
        const auto original = d.selectionRegion();
        const int  history  = d.historyIndex();
        d.modifySelection("smooth", 100);
        d.cancelSelectionOperation();
        QVERIFY(!d.selectionBusy());
        QTRY_VERIFY_WITH_TIMEOUT(d.findChildren<QFutureWatcherBase *>().isEmpty(), 5000);
        QCOMPARE(d.selectionRegion(), original);
        QCOMPARE(d.historyIndex(), history);
        d.modifySelection("expand", 100);
        d.selectAll();
        const auto replacement = d.selectionRegion();
        QTRY_VERIFY_WITH_TIMEOUT(d.findChildren<QFutureWatcherBase *>().isEmpty(), 5000);
        QCOMPARE(d.selectionRegion(), replacement);
        d.modifySelection("contract", 100);
        d.setTool("pen");
        d.beginPenGesture(100, 100);
        QVERIFY(!d.selectionBusy());
        d.endPenGesture();
        QTRY_VERIFY_WITH_TIMEOUT(d.findChildren<QFutureWatcherBase *>().isEmpty(), 5000);
        QCOMPARE(d.selectionRegion(), replacement);
        d.modifySelection("contract", 100);
        d.newDocument(40, 30, true);
        QTRY_VERIFY_WITH_TIMEOUT(d.findChildren<QFutureWatcherBase *>().isEmpty(), 5000);
        QVERIFY(!d.hasSelection());
        QCOMPARE(d.historyIndex(), 0);
    }
    void visibilityIsTransientAndNewSelectionsShowTheirEdges()
    {
        Document d;
        d.newDocument(50, 40, true);
        rectangle(d, 4, 4, 20, 20);
        const auto selection = d.selectionRegion();
        const auto history   = d.historyIndex();
        d.setSelectionEdgesVisible(false);
        QVERIFY(!d.selectionEdgesVisible());
        QCOMPARE(d.selectionRegion(), selection);
        QCOMPARE(d.historyIndex(), history);
        d.selectAll();
        QVERIFY(d.selectionEdgesVisible());
        d.undo();
        QCOMPARE(d.selectionRegion(), selection);
    }
    void retainedSelectionStorageIsAccountedWithoutPixelCopies()
    {
        Document d;
        d.newDocument(100, 80, true);
        const auto image = d.composite();
        rectangle(d, 5, 5, 10, 10);
        rectangle(d, 25, 25, 20, 20);
        rectangle(d, 55, 30, 30, 30);
        const auto memory = d.memoryUsage();
        QVERIFY(memory["selectionBytes"].toLongLong() > 0);
        QVERIFY(memory["historySelectionBytes"].toLongLong() > 0);
        QCOMPARE(memory["historyBytes"], memory["historySelectionBytes"]);
        QCOMPARE(d.composite().cacheKey(), image.cacheKey());
    }
    void penConversionIsTransientButPathsRemainPersistent()
    {
        QTemporaryDir dir;
        Document      d;
        d.newDocument(80, 60, true);
        d.setTool("pen");
        for (const QPoint p : {QPoint(10, 10), QPoint(60, 10), QPoint(60, 45)}) {
            d.beginPenGesture(p.x(), p.y());
            d.endPenGesture();
        }
        d.closePenPath();
        QVERIFY(d.saveProject(QUrl::fromLocalFile(dir.filePath("paths.pcr"))));
        d.penPathToSelection();
        QVERIFY(!d.modified());
        QCOMPARE(d.activePenPath(), -1);
        d.undo();
        QCOMPARE(d.activePenPath(), 0);
        QVERIFY(!d.modified());
        d.undo(); // Closed path becomes open: a persistent change.
        QVERIFY(d.modified());
        d.redo();
        QVERIFY(!d.modified());
    }
    void featherCreatesActualPartialCoverageAndKeepsSixteenBitPixels()
    {
        Document d;
        d.newDocument(120, 100, true, 16);
        rectangle(d, 30, 25, 50, 45);
        const auto hard    = d.selectionRegion();
        const int  history = d.historyIndex();
        d.modifySelection("feather", 5);
        QVERIFY(d.selectionBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!d.selectionBusy(), 5000);
        const auto mask = d.selectionMask();
        QVERIFY(d.selectionCoverage(30, 45) > 50 && d.selectionCoverage(30, 45) < 200);
        QVERIFY(d.selectionCoverage(27, 45) > 0);
        QCOMPARE(d.selectionCoverage(0, 0), 0);
        QCOMPARE(d.selectionCoverage(55, 45), 255);
        QCOMPARE(d.historyIndex(), history + 1);
        d.setColor(QColor::fromRgba64(40001, 17003, 9001, 65535));
        d.fillWithColor(true);
        const auto filled = d.layerImage(0).convertToFormat(QImage::Format_RGBA64_Premultiplied);
        const auto center = reinterpret_cast<const QRgba64 *>(filled.constScanLine(45))[55];
        QCOMPARE(center.red(), quint16(40001));
        const auto edge = reinterpret_cast<const QRgba64 *>(filled.constScanLine(45))[30];
        QVERIFY(edge.alpha() > 0 && edge.alpha() < 65535);
        QVERIFY(qAbs(int(edge.alpha()) - int(mask.constScanLine(45)[30]) * 257) < 3);
        QCOMPARE(d.layerImage(0).pixelColor(0, 0).alpha(), 0);
        d.undo();
        d.undo();
        QCOMPARE(d.selectionRegion(), hard);
        d.redo();
        QCOMPARE(d.selectionMask(), mask);
        d.invertSelection();
        QTRY_VERIFY(!d.selectionBusy());
        QCOMPARE(d.selectionCoverage(30, 45), 255 - int(mask.constScanLine(45)[30]));
        d.undo();
        QCOMPARE(d.selectionMask(), mask);
        d.modifySelection("expand", 2);
        QTRY_VERIFY(!d.selectionBusy());
        QVERIFY(d.selectionCoverage(27, 45) > 0 && d.selectionCoverage(27, 45) < 255);
        d.undo();
        QCOMPARE(d.selectionMask(), mask);
        d.deselect();
        d.reselect();
        QCOMPARE(d.selectionMask(), mask);
        d.addLayerMask();
        QCOMPARE(d.maskImage(0), mask);
        d.undo();
        d.addAdjustmentLayer("invert");
        QCOMPARE(d.maskImage(d.activeLayer()), mask);
    }
    void featherBrushAndMaskFillRespectCoverageAndUndo()
    {
        Document d;
        d.newDocument(100, 80, true);
        rectangle(d, 30, 20, 40, 40);
        d.modifySelection("feather", 4);
        QTRY_VERIFY(!d.selectionBusy());
        const int  coverage = d.selectionCoverage(30, 40);
        const auto mask     = d.selectionMask();
        d.setTool("brush");
        d.setBrushSize(80);
        d.setBrushHardness(1);
        d.setBrushOpacity(1);
        d.setBrushFlow(1);
        d.setColor(Qt::red);
        d.beginStroke(45, 40);
        d.endStroke();
        QVERIFY(qAbs(d.layerImage(0).pixelColor(30, 40).alpha() - coverage) <= 1);
        d.undo();
        QCOMPARE(d.layerImage(0).pixelColor(30, 40).alpha(), 0);
        d.addLayerMask();
        d.fillMaskWithColor(Qt::black);
        const int expected = qRound(coverage * (1. - coverage / 255.));
        QVERIFY(qAbs(d.maskImage(0).constScanLine(40)[30] - expected) <= 1);
        d.undo();
        QCOMPARE(d.maskImage(0), mask);
        d.setEditingMask(false);
        d.setTool("select");
        d.nudgeTool(1, 0);
        QCOMPARE(d.selectionCoverage(31, 40), coverage);
        d.undo();
        QCOMPARE(d.selectionMask(), mask);
        d.setSelectionMode(2);
        d.beginStroke(0, 0);
        d.continueStroke(32, 79);
        d.endStroke();
        QCOMPARE(d.selectionCoverage(30, 40), 0);
        QCOMPARE(d.selectionCoverage(40, 40), int(mask.constScanLine(40)[40]));
        d.undo();
        QCOMPARE(d.selectionMask(), mask);
    }
    void tinyFeatheredSelectionsRemainActiveWithoutVisibleAnts()
    {
        Document d;
        d.newDocument(70, 60, true);
        rectangle(d, 30, 25, 1, 1);
        d.modifySelection("feather", 8);
        QTRY_VERIFY(!d.selectionBusy());
        QVERIFY(d.hasSelection());
        QVERIFY(d.selectionOutline().isEmpty());
        d.setColor(Qt::white);
        d.fillWithColor(true);
        QVERIFY(d.layerImage(0).pixelColor(30, 25).alpha() > 0);
    }
    void colorRangeUsesNativeSamplesAndProgressiveTolerance()
    {
        QImage image(4, 1, QImage::Format_RGBA64);
        auto  *pixels                             = reinterpret_cast<QRgba64 *>(image.scanLine(0));
        pixels[0]                                 = QRgba64::fromRgba64(30000, 10000, 10000, 65535);
        pixels[1]                                 = QRgba64::fromRgba64(30100, 10000, 10000, 65535);
        pixels[2]                                 = QRgba64::fromRgba64(32000, 10000, 10000, 65535);
        pixels[3]                                 = QRgba64::fromRgba64(30000, 10000, 10000, 0);
        auto                               cancel = std::make_shared<std::atomic_bool>(false);
        QVector<SelectionCoverage::Sample> samples{{image.pixelColor(0, 0), QPointF(0, 0), false}};
        const auto result = SelectionCoverage::colors(image, samples, {{"fuzziness", 1.}}, cancel);
        QVERIFY(result.valid);
        QCOMPARE(result.mask.constScanLine(0)[0], uchar(255));
        QVERIFY(result.mask.constScanLine(0)[1] > 0 && result.mask.constScanLine(0)[1] < 255);
        QCOMPARE(result.mask.constScanLine(0)[2], uchar(0));
        QCOMPARE(result.mask.constScanLine(0)[3], uchar(0));
        auto inverse = SelectionCoverage::colors(image, samples,
                                                 {{"fuzziness", 1.}, {"invert", true}}, cancel);
        QCOMPARE(inverse.mask.constScanLine(0)[2], uchar(255));
        QCOMPARE(inverse.mask.constScanLine(0)[3], uchar(0));
        samples.append({image.pixelColor(0, 0), QPointF(0, 0), true});
        auto removed = SelectionCoverage::colors(image, samples, {{"fuzziness", 1.}}, cancel);
        QCOMPARE(removed.mask.constScanLine(0)[0], uchar(0));
    }
    void contiguousColorRangeKeepsSeededIslandsAndTheirSoftCoverage()
    {
        for (int bits : {8, 16}) {
            QImage image(80, 40, bits == 16 ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
            image.fill(Qt::blue);
            {
                QPainter p(&image);
                p.fillRect(5, 5, 15, 15, Qt::red);
                p.fillRect(50, 5, 15, 15, Qt::red);
            }
            image.setPixelColor(19, 18, QColor(240, 0, 0));
            image.setPixelColor(20, 20, Qt::red); // Only a diagonal contact: a separate island.
            image.setPixelColor(30, 30, QColor(255, 0, 0, 0));
            auto                               cancel = std::make_shared<std::atomic_bool>(false);
            QVector<SelectionCoverage::Sample> samples{{QColor(Qt::red), QPointF(10, 10), false}};
            QVariantMap                        options{{"fuzziness", 40.}};
            const auto global = SelectionCoverage::colors(image, samples, options, cancel);
            QVERIFY(global.valid);
            QCOMPARE(global.mask.constScanLine(10)[55], uchar(255));
            options["contiguous"] = true;
            auto local            = SelectionCoverage::colors(image, samples, options, cancel);
            QVERIFY(local.valid);
            QCOMPARE(local.mask.constScanLine(10)[10], uchar(255));
            QCOMPARE(local.mask.constScanLine(10)[55], uchar(0));
            QCOMPARE(local.mask.constScanLine(20)[20], uchar(0));
            QCOMPARE(local.mask.constScanLine(18)[19], global.mask.constScanLine(18)[19]);
            QVERIFY(local.mask.constScanLine(18)[19] > 0 && local.mask.constScanLine(18)[19] < 255);
            samples.append({QColor(Qt::red), QPointF(55, 10), false});
            auto added = SelectionCoverage::colors(image, samples, options, cancel);
            QCOMPARE(added.mask.constScanLine(10)[55], uchar(255));
            samples.append({QColor(Qt::red), QPointF(55, 10), true});
            auto removed = SelectionCoverage::colors(image, samples, options, cancel);
            QCOMPARE(removed.mask.constScanLine(10)[10], uchar(255));
            QCOMPARE(removed.mask.constScanLine(10)[55], uchar(0));
            options["invert"] = true;
            auto inverse      = SelectionCoverage::colors(image, samples, options, cancel);
            QCOMPARE(inverse.mask.constScanLine(10)[10], uchar(0));
            QCOMPARE(inverse.mask.constScanLine(10)[55], uchar(255));
            QCOMPARE(inverse.mask.constScanLine(30)[30], uchar(0));
            cancel->store(true);
            QVERIFY(!SelectionCoverage::colors(image, samples, options, cancel).valid);
        }
    }
    void zoomedColorPreviewReadsNativePixelsAndMapsContiguousOptions()
    {
        QTemporaryDir dir;
        QImage        image(1400, 30, QImage::Format_RGBA64);
        image.fill(Qt::blue);
        for (int y = 0; y < 30; ++y)
            for (int x = 0; x < 1400; ++x)
                image.setPixelColor(x, y,
                                    QColor::fromRgba64(30000 + (x % 3) * 100, 10001, 20001, 65535));
        const auto path = QUrl::fromLocalFile(dir.filePath("native-colors.png"));
        QVERIFY(image.save(path.toLocalFile()));
        Document d;
        QVERIFY(d.openFile(path));
        d.beginColorSelection();
        QTRY_VERIFY_WITH_TIMEOUT(d.colorSelectionReady(), 5000);
        const auto crop = d.colorSelectionPreview(false, QRect(1001, 4, 3, 3), QSize(3, 3));
        QCOMPARE(crop.size(), QSize(3, 3));
        QCOMPARE(crop.pixelColor(0, 0).rgba64(), image.pixelColor(1001, 4).rgba64());
        QCOMPARE(crop.pixelColor(1, 0).rgba64(), image.pixelColor(1002, 4).rgba64());
        d.sampleSelectionColor(1001, 4, 0, {{"fuzziness", 0.}, {"contiguous", true}});
        QTRY_VERIFY(d.colorSelectionReady());
        const auto selected = d.colorSelectionPreview(true, QRect(1001, 4, 3, 3), QSize(3, 3));
        QCOMPARE(selected.constScanLine(0)[0], uchar(255));
        QCOMPARE(selected.constScanLine(0)[1], uchar(0));
        d.cancelColorSelection();
    }
    void colorRangePreviewCanCancelAndCommitOneSelection()
    {
        QTemporaryDir dir;
        QImage        image(90, 60, QImage::Format_RGBA64);
        image.fill(Qt::blue);
        {
            QPainter p(&image);
            p.fillRect(0, 0, 30, 60, Qt::red);
            p.fillRect(30, 0, 30, 60, Qt::green);
        }
        const auto path = QUrl::fromLocalFile(dir.filePath("colors.png"));
        QVERIFY(image.save(path.toLocalFile()));
        Document d;
        QVERIFY(d.openFile(path));
        QVERIFY(d.saveProject(QUrl::fromLocalFile(dir.filePath("colors.pcr"))));
        rectangle(d, 2, 2, 10, 10);
        const auto original = d.selectionRegion();
        const auto pixels   = d.composite();
        const int  history  = d.historyIndex();
        d.beginColorSelection();
        QTRY_VERIFY_WITH_TIMEOUT(d.colorSelectionReady(), 5000);
        d.sampleSelectionColor(5, 20, 0);
        QTRY_VERIFY(d.colorSelectionReady());
        QCOMPARE(d.selectionRegion(), original);
        QCOMPARE(d.historyIndex(), history);
        QVERIFY(!d.colorSelectionPreview(true).isNull());
        d.cancelColorSelection();
        QCOMPARE(d.selectionRegion(), original);
        d.beginColorSelection();
        QTRY_VERIFY(d.colorSelectionReady());
        d.sampleSelectionColor(5, 20, 0);
        QTRY_VERIFY(d.colorSelectionReady());
        d.sampleSelectionColor(75, 20, 1);
        QTRY_VERIFY(d.colorSelectionReady());
        d.acceptColorSelection();
        QVERIFY(!d.colorSelectionActive());
        QCOMPARE(d.selectionCoverage(5, 20), 255);
        QCOMPARE(d.selectionCoverage(75, 20), 255);
        QCOMPARE(d.selectionCoverage(45, 20), 0);
        QCOMPARE(d.historyIndex(), history + 1);
        QCOMPARE(d.composite().cacheKey(), pixels.cacheKey());
        QVERIFY(!d.modified());
        d.undo();
        QCOMPARE(d.selectionRegion(), original);
        d.redo();
        QCOMPARE(d.selectionCoverage(75, 20), 255);
        d.beginColorSelection();
        d.cancelColorSelection();
        QTRY_VERIFY_WITH_TIMEOUT(d.findChildren<QFutureWatcherBase *>().isEmpty(), 5000);
        QVERIFY(!d.colorSelectionActive());
    }
    void similarLayersSelectsTheKindWithoutChangingVisibility()
    {
        Document d;
        d.newDocument(40, 30, true);
        d.addLayer();
        d.addAdjustmentLayer("invert");
        d.setActiveLayer(0);
        const auto pixels = d.composite();
        d.selectSimilarLayers();
        QCOMPARE(d.selectedLayerCount(), 2);
        QCOMPARE(d.composite().cacheKey(), pixels.cacheKey());
    }
};
QTEST_MAIN(SelectionTest)
#include "SelectionTest.moc"
