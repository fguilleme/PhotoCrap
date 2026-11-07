#include "Document.h"
#include "ImageUtils.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QPainter>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

namespace
{
void fixture(Document &_document, int _bits, bool _opaque = false)
{
    _document.setBackgroundEditThreshold(-1);
    QVERIFY(_document.newDocument(137, 523, !_opaque, _bits));
    _document.setColor(QColor::fromRgba64(23456, 45678, 12345, 45231));
    _document.setBrushSize(89);
    _document.setBrushHardness(.45);
    _document.beginStroke(43, 240);
    _document.continueStroke(73, 290);
    _document.endStroke();
}
void top(Document &_document)
{
    _document.addLayer();
    _document.setColor(QColor::fromRgba64(45678, 12345, 56789, 54321));
    _document.beginStroke(83, 250);
    _document.continueStroke(80, 350);
    _document.endStroke();
}
void select(Document &_document)
{
    _document.setTool("ellipticalSelect");
    _document.beginStroke(20, 225);
    _document.continueStroke(100, 330);
    _document.endStroke();
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
QVariantMap info(const Document &_document, int _index)
{
    for (const auto &layer : _document.layers())
        if (layer.toMap().value("layerIndex").toInt() == _index)
            return layer.toMap();
    return {};
}
} // namespace
class LayerMergeTest : public QObject
{
    Q_OBJECT
private slots:
    void downBakesNativeMasksBlendsAndAdjustments_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<QString>("kind");
        for (int bits : {8, 16})
            for (const QString kind : {"pixel", "invert", "gaussianBlur", "solidColor"})
                QTest::newRow(qPrintable(QString::number(bits) + kind)) << bits << kind;
    }
    void downBakesNativeMasksBlendsAndAdjustments()
    {
        QFETCH(int, bits);
        QFETCH(QString, kind);
        Document d;
        fixture(d, bits);
        if (kind == "pixel") {
            top(d);
            d.setLayerBlend(1, 8);
        } else
            d.addAdjustmentLayer(kind);
        select(d);
        d.addLayerMask();
        d.setEditingMask(false);
        d.beginLayerPreview();
        d.previewLayerParameter("_maskDensity", 63);
        d.previewLayerParameter("_maskFeather", 3);
        d.finishLayerPreview();
        d.setLayerOpacity(1, .73);
        const auto expected = d.composite();
        const auto before0 = d.layerImage(0), before1 = d.layerImage(1), mask = d.maskImage(1);
        const auto selection = d.selectionMask();
        const auto region    = d.selectionRegion();
        const int  history   = d.historyIndex();
        QVERIFY(d.layerMergeActions().value("down").toBool());
        d.mergeDown();
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(info(d, 0).value("kind").toString(), QString("pixel"));
        QCOMPARE(d.composite(), expected);
        QCOMPARE(d.layerImage(0), expected);
        QVERIFY(d.maskImage(0).isNull());
        QCOMPARE(d.selectionRegion(), region);
        QCOMPARE(d.selectionMask(), selection);
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.layerImage(0), before0);
        QCOMPARE(d.layerImage(1), before1);
        QCOMPARE(d.maskImage(1), mask);
        d.redo();
        QCOMPARE(d.composite(), expected);
    }
    void everyBlendModeUsesNativeComposite()
    {
        for (int bits : {8, 16})
            for (int blend = 0; blend < 27; ++blend) {
                Document d;
                fixture(d, bits);
                top(d);
                d.setLayerBlend(1, blend);
                d.setLayerOpacity(0, .61);
                const auto expected = d.composite();
                d.mergeDown();
                QCOMPARE(d.layers().size(), 1);
                QCOMPARE(d.composite(), expected);
            }
    }
    void textAndVectorShapesBecomePixelsAndUndoRestoresEditability()
    {
        for (const QString tool : {"text", "pen"}) {
            Document d;
            fixture(d, 16);
            d.setTool(tool);
            if (tool == "text") {
                d.beginStroke(20, 20);
                d.endStroke();
                d.setText("Fusion");
                d.finishTextEdit();
            } else {
                d.setPenMode("shape");
                for (const auto point :
                     {QPoint(20, 20), QPoint(100, 20), QPoint(100, 100), QPoint(20, 100)}) {
                    d.beginPenGesture(point.x(), point.y(), 2);
                    d.endPenGesture();
                }
                d.closePenPath();
            }
            QCOMPARE(d.layers().size(), 2);
            const auto kind     = info(d, 1).value("kind");
            const auto expected = d.composite();
            d.mergeDown();
            QCOMPARE(d.layers().size(), 1);
            QCOMPARE(info(d, 0).value("kind").toString(), QString("pixel"));
            QCOMPARE(d.composite(), expected);
            d.undo();
            QCOMPARE(info(d, 1).value("kind"), kind);
            QCOMPARE(d.composite(), expected);
        }
    }
    void selectedLayersLeaveUnselectedLayersIntact()
    {
        for (int bits : {8, 16}) {
            Document d;
            fixture(d, bits);
            top(d);
            top(d);
            d.setLayerVisible(1, false);
            const auto expected  = d.composite();
            const auto untouched = d.layerImage(1);
            const auto id        = info(d, 1).value("name");
            d.setActiveLayer(0);
            d.selectLayer(2, true);
            d.mergeLayers();
            QCOMPARE(d.layers().size(), 2);
            QCOMPARE(d.activeLayer(), 1);
            QCOMPARE(d.layerImage(0), untouched);
            QCOMPARE(info(d, 0).value("name"), id);
            QVERIFY(!info(d, 0).value("visible").toBool());
            QCOMPARE(d.layerImage(1), expected);
            QCOMPARE(d.composite(), expected);
            d.undo();
            QCOMPARE(d.layers().size(), 3);
            QCOMPARE(d.selectedLayerCount(), 2);
        }
    }
    void visiblePreservesHiddenLayersAndTheirFolders()
    {
        Document d;
        fixture(d, 16);
        top(d);
        d.selectLayer(0, true);
        d.groupSelectedLayers();
        QCOMPARE(d.activeLayer(), 2);
        d.setLayerVisible(0, false);
        const auto hiddenPixels = d.layerImage(0);
        d.setActiveLayer(2);
        top(d);
        const auto expected = d.composite();
        d.mergeLayers("visible");
        QCOMPARE(d.layers().size(), 3); // Hidden child, its folder, merged root.
        QCOMPARE(d.layerImage(0), hiddenPixels);
        QCOMPARE(info(d, 1).value("kind").toString(), QString("group"));
        QCOMPARE(info(d, 0).value("groupId"), info(d, 1).value("groupId"));
        QVERIFY(info(d, 2).value("groupId").toString().isEmpty());
        QCOMPARE(d.composite(), expected);
        d.undo();
        d.setLayerVisible(2, false);
        const auto groupHidden = d.composite();
        top(d);
        const auto allVisible = d.composite();
        d.mergeLayers("visible");
        QCOMPARE(d.layerImage(0), hiddenPixels);
        QVERIFY(!info(d, 2).value("visible").toBool());
        QCOMPARE(d.composite(), allVisible);
        Q_UNUSED(groupHidden);
    }
    void groupAndClippingMerge_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<bool>("clipping");
        for (int bits : {8, 16})
            for (bool clipping : {false, true})
                QTest::newRow(qPrintable(QString::number(bits) + (clipping ? "clip" : "group")))
                    << bits << clipping;
    }
    void groupAndClippingMerge()
    {
        QFETCH(int, bits);
        QFETCH(bool, clipping);
        Document d;
        fixture(d, bits);
        top(d);
        if (clipping) {
            d.setClippingMask(true);
            d.setActiveLayer(0);
        } else {
            d.selectLayer(0, true);
            d.groupSelectedLayers();
            d.toggleGroupCollapsed(d.activeLayer());
        }
        const auto expected = d.composite();
        const int  history  = d.historyIndex();
        QVERIFY(d.layerMergeActions().value(clipping ? "clipping" : "group").toBool());
        d.mergeLayers(clipping ? "clipping" : "selected");
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.composite(), expected);
        QVERIFY(!info(d, 0).value("clippingMask").toBool());
        QVERIFY(info(d, 0).value("groupId").toString().isEmpty());
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        if (!clipping) {
            QVERIFY(info(d, d.activeLayer()).value("groupCollapsed").toBool());
            d.toggleGroupCollapsed(d.activeLayer());
        }
        QCOMPARE(d.layers().size(), clipping ? 2 : 3);
    }
    void stampLeavesOriginalLayersAndClipboardUntouched()
    {
        for (int bits : {8, 16})
            for (const QString mode : {"stampSelected", "stampVisible"}) {
                Document d;
                fixture(d, bits);
                top(d);
                d.selectLayer(0, true);
                const auto expected = d.composite();
                const auto first = d.layerImage(0), second = d.layerImage(1);
                const auto metadata0 = info(d, 0), metadata1 = info(d, 1);
                d.setLayerLocked(0, true);
                d.setLayerLocked(1, true);
                QGuiApplication::clipboard()->setText("keep clipboard");
                d.mergeLayers(mode);
                QCOMPARE(d.layers().size(), 3);
                QCOMPARE(d.layerImage(2), expected);
                QCOMPARE(d.layerImage(0), first);
                QCOMPARE(d.layerImage(1), second);
                QVERIFY(info(d, 0).value("locked").toBool());
                QVERIFY(info(d, 1).value("locked").toBool());
                QCOMPARE(info(d, 0).value("parameters"), metadata0.value("parameters"));
                QCOMPARE(info(d, 1).value("parameters"), metadata1.value("parameters"));
                QCOMPARE(QGuiApplication::clipboard()->text(), QString("keep clipboard"));
                d.undo();
                QCOMPARE(d.layers().size(), 2);
            }
    }
    void flattenDiscardsHiddenAndFillsTransparencyWhite()
    {
        for (int bits : {8, 16}) {
            Document d;
            fixture(d, bits);
            top(d);
            d.setLayerVisible(1, false);
            const auto original = d.composite();
            auto       expected = ImageUtils::blank(original.size(), original.format());
            expected.fill(Qt::white);
            QPainter p(&expected);
            p.drawImage(0, 0, original);
            p.end();
            const int history = d.historyIndex();
            d.setDisplayChannel(2);
            d.mergeLayers("flatten");
            QCOMPARE(d.layers().size(), 1);
            QCOMPARE(d.layerImage(0), expected);
            QCOMPARE(d.layerImage(0).pixelColor(0, 0), QColor(Qt::white));
            QCOMPARE(d.historyIndex(), history + 1);
            d.undo();
            QCOMPARE(d.layers().size(), 2);
            QVERIFY(!info(d, 1).value("visible").toBool());
            QCOMPARE(d.composite(), original);
        }
    }
    void invalidTargetsAndLockedLayersDoNotChangeHistory()
    {
        Document d;
        fixture(d, 8);
        d.addAdjustmentLayer("solidColor");
        top(d);
        const auto before  = d.composite();
        const auto history = d.historyIndex();
        QVERIFY(!d.layerMergeActions().value("down").toBool());
        d.mergeDown();
        QCOMPARE(d.historyIndex(), history);
        d.setActiveLayer(0);
        d.selectLayer(2, true);
        d.setLayerLocked(0, true);
        const auto lockedHistory = d.historyIndex();
        QVERIFY(!d.layerMergeActions().value("selected").toBool());
        d.mergeLayers();
        QCOMPARE(d.historyIndex(), lockedHistory);
        QCOMPARE(d.composite(), before);
    }
    void partialClippingSelectionRequiresItsBase()
    {
        Document d;
        fixture(d, 8);
        top(d);
        d.setClippingMask(true);
        const auto before  = d.composite();
        const int  history = d.historyIndex();
        d.mergeLayers("stampSelected");
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.composite(), before);
    }
    void asynchronousMergeIsAtomicAndCancellable()
    {
        for (bool cancel : {false, true}) {
            Document d;
            fixture(d, 16);
            top(d);
            const auto expected = d.composite();
            const auto first    = d.layerImage(0);
            const int  history  = d.historyIndex();
            d.setBackgroundEditThreshold(0);
            {
                PoolBlock block;
                d.mergeDown();
                QVERIFY(d.operationBusy());
                QCOMPARE(d.layers().size(), 2);
                QCOMPARE(d.layerImage(0), first);
                if (cancel)
                    d.cancelOperation();
            }
            QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
            QCOMPARE(d.layers().size(), cancel ? 2 : 1);
            QCOMPARE(d.composite(), expected);
            QCOMPARE(d.historyIndex(), history + (cancel ? 0 : 1));
            if (!cancel) {
                d.undo();
                QCOMPARE(d.layers().size(), 2);
            }
        }
    }
    void uniformNativeAlphaBelowOneEightBitStepSurvivesMerge()
    {
        Document d;
        d.setBackgroundEditThreshold(-1);
        QVERIFY(d.newDocument(16, 16, true, 16));
        QImage native(16, 16, QImage::Format_RGBA64_Premultiplied);
        native.fill(QColor::fromRgba64(0, 0, 0, 1));
        QGuiApplication::clipboard()->setImage(native);
        d.paste();
        d.addLayer();
        const auto expected = d.composite();
        QCOMPARE(reinterpret_cast<const QRgba64 *>(expected.constScanLine(0))[0].alpha(),
                 ushort(1));
        d.mergeDown();
        QCOMPARE(d.composite(), expected);
        d.undo();
        QCOMPARE(d.composite(), expected);
    }
    void compactDocumentsAndProjectRoundTrip()
    {
        QTemporaryDir dir;
        Document      d;
        d.setBackgroundEditThreshold(-1);
        QVERIFY(d.newDocument(2000, 1500, false, 16));
        d.addLayer();
        d.mergeDown();
        QVERIFY(d.memoryUsage().value("currentLayerHeapBytes").toLongLong() < 1024);
        QCOMPARE(d.layerImage(0).pixelColor(1999, 1499), QColor(Qt::white));
        const auto path = QUrl::fromLocalFile(dir.filePath("merged.pcr"));
        QVERIFY(d.saveProject(path));
        Document loaded;
        QVERIFY(loaded.openFile(path));
        QCOMPARE(loaded.channelBits(), 16);
        QCOMPARE(loaded.composite(), d.composite());
    }
};
QTEST_MAIN(LayerMergeTest)
#include "LayerMergeTest.moc"
