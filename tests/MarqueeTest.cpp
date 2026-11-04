#include "Document.h"
#include <QSemaphore>
#include <QThreadPool>
#include <QtTest>
#include <limits>

namespace
{
void drag(Document &d, QPointF a, QPointF b)
{
    d.beginStroke(a.x(), a.y());
    d.continueStroke(b.x(), b.y());
    d.endStroke();
}
struct PoolBlock {
    QThreadPool *pool    = QThreadPool::globalInstance();
    int          maximum = pool->maxThreadCount();
    QSemaphore   entered, release;
    PoolBlock()
    {
        pool->waitForDone();
        pool->setMaxThreadCount(1);
        pool->start([this] {
            entered.release();
            release.acquire();
        });
        entered.acquire();
    }
    ~PoolBlock()
    {
        release.release();
        pool->waitForDone();
        pool->setMaxThreadCount(maximum);
    }
};
} // namespace
class MarqueeTest : public QObject
{
    Q_OBJECT
private slots:
    void dimensionsAndNormalGestures()
    {
        Document d;
        QVERIFY(d.newDocument(200, 180, true));
        d.setTool("select");
        QCOMPARE(d.selectionFeather(), 0);
        QVERIFY(d.selectionAntialias());
        d.setMarqueeStyle(2);
        d.setMarqueeWidth(60);
        d.setMarqueeHeight(40);
        d.beginStroke(10, 20);
        d.endStroke();
        QCOMPARE(d.selection(), QRect(10, 20, 60, 40));
        d.deselect();
        drag(d, {5, 5}, {90, 100});
        QCOMPARE(d.selection(), QRect(90, 100, 60, 40));
        d.deselect();
        d.swapMarqueeDimensions();
        QCOMPARE(d.marqueeWidth(), 40.);
        QCOMPARE(d.marqueeHeight(), 60.);
        d.setMarqueeStyle(1);
        d.setMarqueeWidth(3);
        d.setMarqueeHeight(2);
        drag(d, {10, 10}, {69, 49});
        QCOMPARE(d.selection(), QRect(10, 10, 60, 40));
        d.deselect();
        drag(d, {90, 100}, {31, 61});
        QCOMPARE(d.selection(), QRect(31, 61, 60, 40));
        d.deselect();
        d.setMarqueeStyle(0);
        d.beginStroke(10, 10, false, true);
        d.continueStroke(39, 19);
        d.endStroke();
        QCOMPARE(d.selection(), QRect(10, 10, 30, 30));
        d.setToolModifiers(false, false, false);
        d.setMarqueeWidth(std::numeric_limits<double>::infinity());
        QCOMPARE(d.marqueeWidth(), 3.);
        d.setMarqueeHeight(0);
        QCOMPARE(d.marqueeHeight(), 2.);
        d.setSelectionFeather(-1);
        QCOMPARE(d.selectionFeather(), 0);
        d.setMarqueeStyle(123);
        QCOMPARE(d.marqueeStyle(), 0);
    }
    void antialiasAndPaint_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8-bit") << 8;
        QTest::newRow("16-bit") << 16;
    }
    void antialiasAndPaint()
    {
        QFETCH(int, bits);
        Document d;
        QVERIFY(d.newDocument(80, 70, true, bits));
        d.setTool("ellipticalSelect");
        drag(d, {12, 10}, {57, 48});
        int    partial = 0;
        QPoint sample;
        for (int y = 0; y < 70; ++y)
            for (int x = 0; x < 80; ++x)
                if (d.selectionCoverage(x, y) > 0 && d.selectionCoverage(x, y) < 255) {
                    ++partial;
                    sample = {x, y};
                }
        QVERIFY(partial > 30);
        const int  coverage = d.selectionCoverage(sample.x(), sample.y());
        const auto before   = d.selectionMask();
        d.setColor(Qt::red);
        d.fillWithColor(true);
        QVERIFY(qAbs(d.composite().pixelColor(sample).alpha() - coverage) <= 1);
        d.undo();
        QCOMPARE(d.selectionMask(), before);
        d.deselect();
        d.setSelectionAntialias(false);
        drag(d, {12, 10}, {57, 48});
        for (int y = 0; y < 70; ++y)
            for (int x = 0; x < 80; ++x)
                QVERIFY(d.selectionCoverage(x, y) == 0 || d.selectionCoverage(x, y) == 255);
    }
    void featherCombinationAndHistory()
    {
        Document d;
        QVERIFY(d.newDocument(100, 90, true));
        d.setTool("select");
        drag(d, {12, 14}, {39, 41});
        const auto original = d.selectionMask();
        const auto history  = d.historyIndex();
        d.setSelectionFeather(3);
        d.setSelectionMode(1);
        drag(d, {55, 30}, {72, 52});
        d.setSelectionMode(0);
        QVERIFY(d.selectionBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!d.selectionBusy(), 4000);
        QCOMPARE(d.historyIndex(), history + 1);
        QVERIFY(!d.modified());
        QCOMPARE(d.selectionCoverage(20, 20), 255);
        QVERIFY(d.selectionCoverage(53, 39) > 0 && d.selectionCoverage(53, 39) < 255);
        const auto combined = d.selectionMask();
        d.undo();
        QCOMPARE(d.selectionMask(), original);
        d.redo();
        QCOMPARE(d.selectionMask(), combined);
        d.deselect();
        d.reselect();
        QCOMPARE(d.selectionMask(), combined);
    }
    void softModesUseTheNewCoverage()
    {
        Document reference;
        reference.newDocument(80, 70, true);
        reference.setTool("ellipticalSelect");
        drag(reference, {10, 10}, {45, 46});
        const auto a = reference.selectionMask();
        reference.deselect();
        drag(reference, {30, 19}, {66, 55});
        const auto b = reference.selectionMask();
        for (int mode : {1, 2, 3}) {
            Document d;
            d.newDocument(80, 70, true);
            d.setTool("ellipticalSelect");
            drag(d, {10, 10}, {45, 46});
            const auto history = d.historyIndex();
            d.setSelectionMode(mode);
            drag(d, {30, 19}, {66, 55});
            QCOMPARE(d.historyIndex(), history + 1);
            for (int y = 0; y < 70; ++y)
                for (int x = 0; x < 80; ++x) {
                    const int aa = a.constScanLine(y)[x], bb = b.constScanLine(y)[x];
                    QCOMPARE(d.selectionCoverage(x, y), mode == 1   ? qMax(aa, bb)
                                                        : mode == 2 ? qMax(0, aa - bb)
                                                                    : qMin(aa, bb));
                }
            d.undo();
            QCOMPARE(d.selectionMask(), a);
        }
    }
    void cancelAndStaleFeather()
    {
        Document d;
        d.newDocument(100, 90, true);
        d.setTool("select");
        drag(d, {10, 10}, {30, 30});
        const auto original = d.selectionMask();
        const auto history  = d.historyIndex();
        {
            PoolBlock blocked;
            d.setSelectionFeather(20);
            drag(d, {50, 50}, {65, 65});
            QVERIFY(d.selectionBusy());
            d.cancelSelectionOperation();
            QCOMPARE(d.selectionMask(), original);
            QCOMPARE(d.historyIndex(), history);
        }
        QCoreApplication::processEvents();
        QCOMPARE(d.selectionMask(), original);
        {
            PoolBlock blocked;
            drag(d, {50, 50}, {65, 65});
            QVERIFY(d.selectionBusy());
            d.newDocument(64, 64, true);
        }
        QCoreApplication::processEvents();
        QVERIFY(!d.hasSelection());
        QCOMPARE(d.width(), 64);
    }
    void featheredRowMovesOnlyVertically()
    {
        Document d;
        d.newDocument(80, 70, true);
        d.setTool("singleRowSelect");
        d.setSelectionFeather(2);
        d.beginStroke(20, 30);
        d.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!d.selectionBusy(), 4000);
        const auto mask    = d.selectionMask();
        const int  history = d.historyIndex();
        drag(d, {20, 30}, {35, 35});
        QCOMPARE(d.historyIndex(), history + 1);
        QCOMPARE(d.selectionCoverage(5, 35), int(mask.constScanLine(30)[5]));
        d.undo();
        QCOMPARE(d.selectionMask(), mask);
    }
    void lassoAndSinglePixelOptions()
    {
        Document d;
        d.newDocument(80, 70, true);
        d.setSelectionFeather(2);
        d.setTool("polygonalLasso");
        d.addPolygonLassoPoint(10, 10);
        d.addPolygonLassoPoint(50, 22);
        d.addPolygonLassoPoint(30, 55);
        d.finishPolygonLasso();
        QTRY_VERIFY_WITH_TIMEOUT(!d.selectionBusy(), 4000);
        QVERIFY(d.selectionCoverage(29, 10) > 0 && d.selectionCoverage(29, 10) < 255);
        d.deselect();
        d.setSelectionFeather(0);
        d.setMarqueeStyle(2);
        d.setMarqueeWidth(24);
        d.setMarqueeHeight(18);
        d.setTool("singleRowSelect");
        d.beginStroke(20, 35);
        d.endStroke();
        QCOMPARE(d.selection(), QRect(0, 35, 80, 1));
    }
};
QTEST_MAIN(MarqueeTest)
#include "MarqueeTest.moc"
