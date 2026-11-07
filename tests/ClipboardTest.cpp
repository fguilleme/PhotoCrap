#include "Document.h"
#include "ImageUtils.h"
#include "editing/ImageClipboard.h"
#include <QBuffer>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QSemaphore>
#include <QThreadPool>
#include <QtTest>

namespace
{
void rectangle(Document &d, QPoint start = {8, 10}, QPoint end = {30, 36})
{
    d.setTool("select");
    d.beginStroke(start.x(), start.y());
    d.continueStroke(end.x(), end.y());
    d.endStroke();
}
void fixture(Document &d, int bits)
{
    d.setBackgroundEditThreshold(-1);
    QVERIFY(d.newDocument(80, 64, true, bits));
    d.setColor(QColor::fromRgba64(23456, 45678, 12345, 65535));
    d.fillWithColor(true);
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
class ClipboardTest : public QObject
{
    Q_OBJECT
private slots:
    void cutPasteSelection_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<QString>("tool");
        QTest::addColumn<bool>("soft");
        for (int bits : {8, 16})
            for (const auto &tool : {"select", "ellipticalSelect", "lasso"})
                for (bool soft : {false, true})
                    QTest::newRow(
                        qPrintable(QString::number(bits) + tool + (soft ? "-soft" : "-hard")))
                        << bits << QString(tool) << soft;
    }
    void cutPasteSelection()
    {
        QFETCH(int, bits);
        QFETCH(QString, tool);
        QFETCH(bool, soft);
        Document d;
        fixture(d, bits);
        d.setSelectionAntialias(false);
        d.setTool(tool);
        d.beginStroke(8, 10);
        if (tool == "lasso") {
            d.continueStroke(30, 12);
            d.continueStroke(26, 36);
            d.continueStroke(9, 31);
            d.continueStroke(8, 10);
        } else
            d.continueStroke(30, 36);
        d.endStroke();
        if (soft) {
            d.modifySelection("feather", 2);
            QTRY_VERIFY(!d.selectionBusy());
        }
        QVERIFY(d.hasSelection());
        const auto before   = d.layerImage(0);
        const auto region   = d.selectionRegion();
        const auto coverage = d.selectionMask();
        const auto bounds   = region.boundingRect();
        const auto history  = d.historyIndex();
        d.cut();
        QVERIFY(!d.hasSelection());
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.historyIndex(), history + 1);
        const auto clip = PhotoCrap::Clipboard::read();
        QCOMPARE(clip.origin, bounds.topLeft());
        QCOMPARE(clip.image.size(), bounds.size());
        QCOMPARE(clip.image.format(), d.workingFormat());
        const auto cut = d.layerImage(0);
        for (int y = 0; y < d.height(); ++y)
            for (int x = 0; x < d.width(); ++x) {
                const int amount = !region.contains({x, y}) ? 0
                                   : coverage.isNull()      ? 255
                                                            : coverage.constScanLine(y)[x];
                const int alpha  = cut.pixelColor(x, y).alpha();
                QVERIFY2(qAbs(alpha - (255 - amount)) <= 1,
                         "cut removes only the actual selected coverage");
                if (bounds.contains({x, y}))
                    QVERIFY(qAbs(clip.image.pixelColor(x - bounds.x(), y - bounds.y()).alpha() -
                                 amount) <= 1);
                if (!amount)
                    QCOMPARE(cut.pixelColor(x, y), before.pixelColor(x, y));
            }
        d.pasteSpecial("place");
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.activeLayer(), 1);
        QCOMPARE(d.layerImage(0), cut);
        QCOMPARE(d.layerImage(1).copy(bounds), clip.image);
        QCOMPARE(d.layerImage(1).pixelColor(79, 63).alpha(), 0);
        d.undo();
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.layerImage(0), cut);
        d.undo();
        QCOMPARE(d.layerImage(0), before);
        QCOMPARE(d.selectionRegion(), region);
        QCOMPARE(d.selectionMask(), coverage);
        d.redo();
        d.redo();
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.layerImage(1).copy(bounds), clip.image);
    }
    void activeCopyIsIndependentOfMergedAndDisplayChannel()
    {
        Document d;
        fixture(d, 16);
        const auto bottom = d.layerImage(0);
        d.addLayer();
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        d.setActiveLayer(0);
        d.setDisplayChannel(1);
        rectangle(d);
        const auto bounds  = d.selectionRegion().boundingRect();
        const int  history = d.historyIndex();
        d.setLayerLocked(0, true);
        d.copy();
        QCOMPARE(PhotoCrap::Clipboard::read().image, bottom.copy(bounds));
        const auto copy = PhotoCrap::Clipboard::read().image;
        d.cut();
        QCOMPARE(d.layerImage(0), bottom);
        QCOMPARE(PhotoCrap::Clipboard::read().image, copy);
        d.copyMerged();
        QCOMPARE(PhotoCrap::Clipboard::read().image.pixelColor(3, 3), QColor(Qt::blue));
        QCOMPARE(d.historyIndex(), history + 1); // Only locking is an edit.
        d.setLayerLocked(0, false);
        d.addLayer();
        const auto merged = PhotoCrap::Clipboard::read().image;
        d.copy();
        QCOMPARE(PhotoCrap::Clipboard::read().image, merged); // Empty copy keeps the clipboard.
    }
    void pasteCentersWithoutClippingToDestinationSelection()
    {
        Document source;
        fixture(source, 16);
        rectangle(source);
        source.copy();
        const auto clip = PhotoCrap::Clipboard::read();
        Document   d;
        d.setBackgroundEditThreshold(-1);
        QVERIFY(d.newDocument(120, 100, true, 16));
        d.paste();
        const QPoint center((120 - clip.image.width()) / 2, (100 - clip.image.height()) / 2);
        QCOMPARE(d.layerImage(1).copy(QRect(center, clip.image.size())), clip.image);
        rectangle(d, {90, 70}, {94, 74});
        const auto selection = d.selectionRegion().boundingRect();
        d.paste();
        const QPoint at =
            selection.topLeft() + QPoint((selection.width() - clip.image.width()) / 2,
                                         (selection.height() - clip.image.height()) / 2);
        QCOMPARE(d.layerImage(2).copy(QRect(at, clip.image.size())), clip.image);
        QVERIFY(!d.hasSelection());
        d.pasteSpecial("place");
        QCOMPARE(d.layerImage(3).copy(QRect(clip.origin, clip.image.size())), clip.image);
        QCOMPARE(d.clipboardInfo().value("bits").toInt(), 16);
        QCOMPARE(d.clipboardImageSize(), clip.image.size());
        Document eight;
        eight.setBackgroundEditThreshold(-1);
        QVERIFY(eight.newDocument(120, 100, true, 8));
        eight.paste();
        QCOMPARE(eight.layerImage(1).format(), eight.workingFormat());
        QCOMPARE(eight.layerImage(1).pixelColor(center),
                 clip.image.convertToFormat(eight.workingFormat()).pixelColor(0, 0));
    }
    void pasteInsideAndOutsideCreateEditableMasks()
    {
        QImage image(80, 64, QImage::Format_RGBA64_Premultiplied);
        image.fill(Qt::red);
        PhotoCrap::Clipboard::write({image, {}, image.size(), true});
        for (const QString mode : {"inside", "outside"}) {
            Document d;
            fixture(d, 16);
            d.setTool("ellipticalSelect");
            d.beginStroke(12, 12);
            d.continueStroke(50, 48);
            d.endStroke();
            d.modifySelection("feather", 2);
            QTRY_VERIFY(!d.selectionBusy());
            const auto selection = d.selectionMask();
            const auto before    = d.layerImage(0);
            const int  history   = d.historyIndex();
            d.pasteSpecial(mode);
            QCOMPARE(d.layers().size(), 2);
            QVERIFY(!d.editingMask());
            QVERIFY(!d.hasSelection());
            auto expected = selection;
            if (mode == "outside")
                expected.invertPixels();
            QCOMPARE(d.maskImage(1), expected);
            // Contents remain under the mask rather than being destructively clipped.
            QCOMPARE(d.layerImage(1).pixelColor(10, 10), QColor(Qt::red));
            QCOMPARE(d.layerImage(0), before);
            QCOMPARE(d.historyIndex(), history + 1);
            d.undo();
            QCOMPARE(d.layers().size(), 1);
            QCOMPARE(d.selectionMask(), selection);
        }
    }
    void disjointAndInvertedSelectionsKeepHoles()
    {
        for (bool invert : {false, true}) {
            Document d;
            fixture(d, 16);
            d.setSelectionAntialias(false);
            rectangle(d, {4, 4}, {14, 14});
            d.setSelectionMode(1);
            rectangle(d, {40, 38}, {55, 50});
            if (invert)
                d.invertSelection();
            const auto selected = d.selectionRegion();
            d.cut();
            const auto pixels = d.layerImage(0);
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 80; ++x)
                    QCOMPARE(pixels.pixelColor(x, y).alpha(), selected.contains({x, y}) ? 0 : 255);
            d.pasteSpecial("place");
            const auto pasted = d.layerImage(1);
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 80; ++x)
                    QCOMPARE(pasted.pixelColor(x, y).alpha(), selected.contains({x, y}) ? 255 : 0);
        }
    }
    void selectedMaskCopyCutPasteDoesNotCreateLayer()
    {
        Document d;
        fixture(d, 16);
        d.addLayerMask();
        d.setEditingMask(true);
        rectangle(d);
        const auto bounds = d.selectionRegion().boundingRect();
        const auto pixels = d.layerImage(0);
        const auto mask   = d.maskImage(0);
        d.copy();
        QCOMPARE(PhotoCrap::Clipboard::read().image.pixelColor(0, 0), QColor(Qt::white));
        d.cut();
        QCOMPARE(d.maskImage(0).pixelColor(bounds.center()), QColor(Qt::black));
        QCOMPARE(d.layerImage(0), pixels);
        d.pasteSpecial("place");
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.maskImage(0), mask);
        QVERIFY(d.editingMask());
        d.undo();
        QCOMPARE(d.maskImage(0).pixelColor(bounds.center()), QColor(Qt::black));
    }
    void viaCopyAndCutPreservePositionAndClipboard()
    {
        QGuiApplication::clipboard()->setText("unchanged");
        for (bool cut : {false, true}) {
            Document d;
            fixture(d, 16);
            rectangle(d);
            const auto bounds  = d.selectionRegion().boundingRect();
            const auto before  = d.layerImage(0);
            const auto history = d.historyIndex();
            d.layerViaSelection(cut);
            QCOMPARE(d.layers().size(), 2);
            QCOMPARE(d.layerImage(1).copy(bounds), before.copy(bounds));
            QCOMPARE(d.layerImage(0).pixelColor(bounds.center()).alpha(), cut ? 0 : 255);
            QCOMPARE(QGuiApplication::clipboard()->text(), QString("unchanged"));
            QCOMPARE(d.historyIndex(), history + 1);
            d.undo();
            QCOMPARE(d.layerImage(0), before);
            QCOMPARE(d.layers().size(), 1);
            QVERIFY(d.hasSelection());
        }
    }
    void nativeClipboardSurvivesProcessBoundaryAndExternalImages()
    {
        QImage image(3, 2, QImage::Format_RGBA64_Premultiplied);
        image.fill(QColor::fromRgba64(12345, 34567, 4567, 53123));
        PhotoCrap::Clipboard::write({image, {17, 19}, {80, 64}, true});
        const auto *source = QGuiApplication::clipboard()->mimeData();
        const auto  native = source->data("application/x-photocrap-native-png");
        QVERIFY(!native.isEmpty());
        const auto metadata = source->data("application/x-photocrap-selection");
        QGuiApplication::clipboard()->clear();
        auto *mime = new QMimeData;
        mime->setData("application/x-photocrap-native-png", native);
        mime->setData("application/x-photocrap-selection", metadata);
        QGuiApplication::clipboard()->setMimeData(mime);
        const auto decoded = PhotoCrap::Clipboard::read();
        QCOMPARE(decoded.image.convertToFormat(image.format()), image);
        QCOMPARE(decoded.origin, QPoint(17, 19));
        QVERIFY(decoded.positioned);
        QGuiApplication::clipboard()->setImage(image);
        const auto external = PhotoCrap::Clipboard::read();
        QCOMPARE(external.image, image);
        QVERIFY(!external.positioned);
        QGuiApplication::clipboard()->setText("not pixels");
        QVERIFY(!PhotoCrap::Clipboard::hasImage());
        QVERIFY(PhotoCrap::Clipboard::read().image.isNull());
    }
    void asynchronousCutThenImmediatePaste_data()
    {
        QTest::addColumn<bool>("cut");
        QTest::newRow("cut") << true;
        QTest::newRow("copy") << false;
    }
    void asynchronousCutThenImmediatePaste()
    {
        QFETCH(bool, cut);
        Document d;
        fixture(d, 16);
        rectangle(d);
        const auto bounds = d.selectionRegion().boundingRect();
        const auto before = d.layerImage(0);
        d.setBackgroundEditThreshold(0);
        {
            PoolBlock block;
            if (cut)
                d.cut();
            else
                d.copy();
            QVERIFY(d.operationBusy());
            QVERIFY(d.clipboardTransferPending());
            d.pasteSpecial("place");
            QCOMPARE(d.layers().size(), 1);
        }
        QTRY_COMPARE_WITH_TIMEOUT(d.layers().size(), 2, 10000);
        QTRY_VERIFY(!d.operationBusy());
        QVERIFY(!d.clipboardTransferPending());
        QCOMPARE(d.layerImage(1).copy(bounds), before.copy(bounds));
        QCOMPARE(d.layerImage(0).pixelColor(bounds.center()).alpha(), cut ? 0 : 255);
    }
    void cancelledCutDropsQueuedPasteAndNeverChangesPixels()
    {
        Document d;
        fixture(d, 8);
        rectangle(d);
        const auto before = d.layerImage(0);
        d.setBackgroundEditThreshold(0);
        {
            PoolBlock block;
            d.cut();
            d.paste();
            d.cancelOperation();
        }
        QTRY_VERIFY(!d.operationBusy());
        QTest::qWait(50);
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.layerImage(0), before);
        QVERIFY(d.hasSelection());
        QVERIFY(!d.clipboardTransferPending());
    }
    void copyingDoesNotOverwriteNewerExternalClipboard()
    {
        Document d;
        fixture(d, 8);
        rectangle(d);
        d.setBackgroundEditThreshold(0);
        {
            PoolBlock block;
            d.copy();
            QGuiApplication::clipboard()->setText("newer");
        }
        QTRY_VERIFY(!d.operationBusy());
        QVERIFY(!d.clipboardTransferPending());
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("newer"));
    }
};
QTEST_MAIN(ClipboardTest)
#include "ClipboardTest.moc"
