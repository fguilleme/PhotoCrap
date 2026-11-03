#include "Document.h"
#include <QElapsedTimer>
#include <QSemaphore>
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
void populate(Document &d, int bits)
{
    d.setBackgroundEditThreshold(-1);
    d.newDocument(80, 60, false, bits);
    d.setColor(QColor(157, 31, 79));
    d.setBrushSize(20);
    d.beginStroke(18, 21);
    d.continueStroke(60, 40);
    d.endStroke();
    d.addLayer();
    d.setColor(QColor(31, 126, 203, 153));
    d.setBrushSize(15);
    d.beginStroke(30, 10);
    d.continueStroke(55, 28);
    d.endStroke();
}
void operation(Document &d, const QString &name)
{
    if (name == "resize")
        d.resizeImage(53, 41);
    else if (name == "crop")
        d.cropToSelection();
    else if (name == "depth")
        d.requestChannelBits(d.channelBits() == 8 ? 16 : 8);
    else if (name == "fill")
        d.fillWithColor(true, true);
    else if (name.startsWith("bucket")) {
        d.setTool("fill");
        d.setBrushOpacity(.63);
        d.setColor(QColor::fromRgba64(14321, 32123, 54321, 41234));
        d.beginStroke(10, 15);
    } else if (name == "clear")
        d.clearSelected();
    else if (name == "merge")
        d.mergeDown();
    else if (name == "invertMask")
        d.invertLayerMask();
    else if (name == "filter")
        d.applyFilter("blur", 3);
    else
        d.transformImage(name);
}
} // namespace
class DocumentOperationsTest : public QObject
{
    Q_OBJECT
private slots:
    void bucketCanBeCancelledOrSupersededBeforeAFirstDrag()
    {
        Document d;
        populate(d, 8);
        d.setBackgroundEditThreshold(0);
        const auto before  = d.layerImage(d.activeLayer());
        const auto history = d.history();
        {
            BlockPool block;
            for (const auto &tool : {"move", "redEye", "rectangle"}) {
                d.setTool("fill");
                d.setColor(Qt::green);
                d.beginStroke(4, 4);
                QVERIFY(d.operationBusy());
                QCOMPARE(d.layerImage(d.activeLayer()), before);
                QCOMPARE(d.history(), history);
                d.setShapeNewLayer(false);
                d.setTool(tool);
                d.beginStroke(30, 30);
                QVERIFY(!d.operationBusy());
                d.cancelStroke();
                QCOMPARE(d.layerImage(d.activeLayer()), before);
                QCOMPARE(d.history(), history);
            }
            d.setTool("fill");
            d.beginStroke(4, 4);
            QVERIFY(d.operationBusy());
            d.cancelOperation();
            QVERIFY(!d.operationBusy());
            QCOMPARE(d.history(), history);
            d.beginStroke(4, 4);
            QVERIFY(d.operationBusy());
            QVERIFY(d.newDocument(27, 19, true));
            QVERIFY(!d.operationBusy());
        }
        QCoreApplication::processEvents();
        QCOMPARE(d.width(), 27);
        QCOMPARE(d.height(), 19);
        QCOMPARE(d.composite().pixelColor(0, 0).alpha(), 0);
        QCOMPARE(d.historyIndex(), 0);
    }
    void bucketCapturesInkAndOpacityAndKeepsLaterToolSettings()
    {
        Document   reference, asynchronous;
        const auto ink = QColor::fromRgba64(51321, 12345, 23456, 45678);
        for (auto *d : {&reference, &asynchronous}) {
            d->setBackgroundEditThreshold(-1);
            QVERIFY(d->newDocument(40, 30, false, 16));
            d->setTool("fill");
            d->setColor(ink);
            d->setBrushOpacity(.37);
        }
        reference.beginStroke(4, 4);
        {
            BlockPool block;
            asynchronous.setBackgroundEditThreshold(0);
            asynchronous.beginStroke(4, 4);
            QVERIFY(asynchronous.operationBusy());
            asynchronous.setColor(Qt::blue);
            asynchronous.setBrushOpacity(.91);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!asynchronous.operationBusy(), 10000);
        QCOMPARE(asynchronous.composite(), reference.composite());
        QCOMPARE(asynchronous.color(), QColor(Qt::blue));
        QCOMPARE(asynchronous.brushOpacity(), qreal(.91));
    }
    void sameColourBucketDoesNotQueueOrCommit()
    {
        Document d;
        QVERIFY(d.newDocument(40, 30, false, 16));
        d.setTool("fill");
        d.setColor(Qt::white);
        d.setBrushOpacity(1);
        d.setBackgroundEditThreshold(0);
        const auto history = d.history();
        QSignalSpy operations(&d, &Document::operationChanged);
        d.beginStroke(4, 4);
        QVERIFY(!d.operationBusy());
        QCOMPARE(operations.size(), 0);
        QCOMPARE(d.history(), history);
    }
    void exactAndAtomic_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<QString>("name");
        for (int bits : {8, 16})
            for (const QString name :
                 {"resize", "crop", "depth", "fill", "bucket", "bucketMask", "bucketSoft", "clear",
                  "merge", "invertMask", "filter", "rotateLeft", "rotateRight", "flipH", "flipV"})
                QTest::newRow(qPrintable(QString::number(bits) + name)) << bits << name;
    }
    void exactAndAtomic()
    {
        QFETCH(int, bits);
        QFETCH(QString, name);
        Document reference, asynchronous;
        for (auto *d : {&reference, &asynchronous}) {
            populate(*d, bits);
            if (name == "crop" || name == "clear") {
                d->setTool("select");
                d->beginStroke(7, 9);
                d->continueStroke(65, 48);
                d->endStroke();
                QVERIFY(d->hasSelection());
            }
            if (name == "bucketSoft") {
                d->setTool("ellipticalSelect");
                d->beginStroke(0, 0);
                d->continueStroke(70, 50);
                d->endStroke();
                d->modifySelection("feather", 2);
            }
            if (name == "invertMask" || name == "bucketMask") {
                d->addLayerMask();
                d->setColor(Qt::black);
                d->beginStroke(10, 10);
                d->endStroke();
            }
        }
        const QImage before  = asynchronous.composite();
        const auto   history = asynchronous.history();
        const QSize  size    = QSize(asynchronous.width(), asynchronous.height());
        QSignalSpy   errors(&asynchronous, &Document::errorOccurred);
        QSignalSpy   commits(&asynchronous, &Document::historyChanged);
        asynchronous.setBackgroundEditThreshold(0);
        operation(reference, name);
        operation(asynchronous, name);
        QVERIFY(asynchronous.operationBusy());
        QVERIFY(asynchronous.pixelEditBusy());
        QCOMPARE(asynchronous.composite(), before);
        QCOMPARE(asynchronous.history(), history);
        QTRY_VERIFY_WITH_TIMEOUT(!asynchronous.operationBusy(), 10000);
        QCOMPARE(errors.size(), 0);
        QCOMPARE(commits.size(), 1);
        QCOMPARE(QSize(asynchronous.width(), asynchronous.height()),
                 QSize(reference.width(), reference.height()));
        QCOMPARE(asynchronous.channelBits(), reference.channelBits());
        QCOMPARE(asynchronous.composite(), reference.composite());
        QCOMPARE(asynchronous.layers().size(), reference.layers().size());
        for (int i = 0; i < reference.layers().size(); ++i) {
            QCOMPARE(asynchronous.layerImage(i), reference.layerImage(i));
            QCOMPARE(asynchronous.maskImage(i), reference.maskImage(i));
        }
        const QImage after = asynchronous.composite();
        asynchronous.undo();
        QCOMPARE(QSize(asynchronous.width(), asynchronous.height()), size);
        QCOMPARE(asynchronous.composite(), before);
        asynchronous.redo();
        QCOMPARE(asynchronous.composite(), after);
    }
    void cancelDiscardAndLifetime()
    {
        BlockPool block;
        Document  d;
        populate(d, 16);
        d.setBackgroundEditThreshold(0);
        const auto before  = d.composite();
        const auto history = d.history();
        d.resizeImage(37, 29);
        QVERIFY(d.operationBusy());
        d.cancelOperation();
        QVERIFY(!d.operationBusy());
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.history(), history);
        d.applyFilter("invert");
        QVERIFY(d.operationBusy());
        d.undo();
        QVERIFY(!d.operationBusy());
        QCOMPARE(d.history(), history);
        d.transformImage("rotateRight");
        QVERIFY(d.operationBusy());
        d.setActiveLayer(0);
        QVERIFY(!d.operationBusy());
        d.resizeImage(11, 13);
        QVERIFY(d.operationBusy());
        QVERIFY(d.newDocument(27, 19, true, 8));
        QVERIFY(!d.operationBusy());
        auto transient = std::make_unique<Document>();
        populate(*transient, 8);
        transient->setBackgroundEditThreshold(0);
        transient->resizeImage(31, 23);
        QElapsedTimer timer;
        timer.start();
        transient.reset();
        QVERIFY(timer.elapsed() < 1000); // Destruction must not wait for the blocked worker.
        block.release.release();
        block.pool->waitForDone();
        QTest::qWait(50);
        QCOMPARE(QSize(d.width(), d.height()), QSize(27, 19));
        QCOMPARE(d.channelBits(), 8);
        QCOMPARE(d.history().size(), 1);
    }
    void liveGesturesSupersedeQueuedEdits_data()
    {
        QTest::addColumn<QString>("gesture");
        for (const char *name : {"parameter", "blend", "pen", "slice"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void liveGesturesSupersedeQueuedEdits()
    {
        QFETCH(QString, gesture);
        BlockPool block;
        Document  d;
        populate(d, 16);
        if (gesture == "parameter")
            d.addAdjustmentLayer("brightnessContrast");
        if (gesture == "pen")
            d.setTool("pen");
        if (gesture == "slice")
            d.setTool("slice");
        d.setBackgroundEditThreshold(0);
        d.resizeImage(41, 29);
        QVERIFY(d.operationBusy());
        if (gesture == "parameter")
            d.previewLayerParameter("brightness", 35);
        else if (gesture == "blend")
            d.previewLayerBlend(d.activeLayer(), 3);
        else if (gesture == "pen")
            d.beginPenGesture(12, 15);
        else {
            d.beginSliceGesture(9, 9);
            d.updateSliceGesture(40, 30);
        }
        // The old worker must be invalidated before release/commit changes the ID.
        QVERIFY(!d.operationBusy());
        if (gesture == "pen")
            d.endPenGesture();
        else if (gesture == "slice")
            d.endSliceGesture();
        else
            d.finishLayerPreview();
        const auto layers = d.layers();
        const auto paths = d.penPaths(), slices = d.slices();
        const auto history = d.history();
        block.release.release();
        block.pool->waitForDone();
        QTest::qWait(100);
        QCOMPARE(QSize(d.width(), d.height()), QSize(80, 60));
        QCOMPARE(d.layers(), layers);
        QCOMPARE(d.penPaths(), paths);
        QCOMPARE(d.slices(), slices);
        QCOMPARE(d.history(), history);
    }
    void backgroundGeometryFinalizesAnExistingTextEdit()
    {
        Document d;
        d.newDocument(80, 60, true, 16);
        d.setTool("text");
        d.beginStroke(5, 5);
        d.endStroke();
        d.setText("Keep");
        QVERIFY(d.textEditing());
        d.setBackgroundEditThreshold(0);
        d.resizeImage(40, 30);
        QVERIFY(d.operationBusy());
        QVERIFY(!d.textEditing());
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(QSize(d.width(), d.height()), QSize(40, 30));
        d.undo();
        QCOMPARE(QSize(d.width(), d.height()), QSize(80, 60));
        QCOMPARE(d.text(), QString("Keep"));
    }

    void invalidOperationKeepsDocument()
    {
        Document d;
        populate(d, 8);
        d.setBackgroundEditThreshold(0);
        const auto before  = d.composite();
        const auto history = d.history();
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.resizeImage(100000, 100000);
        QVERIFY(!d.operationBusy());
        QCOMPARE(errors.size(), 1);
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.history(), history);
    }
};
QTEST_MAIN(DocumentOperationsTest)
#include "DocumentOperationsTest.moc"
