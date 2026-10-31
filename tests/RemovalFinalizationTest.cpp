#include "Document.h"
#include "tools/ContentFill.h"
#include <QFile>
#include <QPainter>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

namespace
{
QImage fixture()
{
    QImage image(220, 160, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const int grey = y % 6 < 3 ? 160 : 170;
            image.setPixelColor(x, y, QColor(grey, grey, grey, 200));
        }
    QPainter painter(&image);
    painter.fillRect(54, 54, 8, 8, Qt::red);
    return image;
}
void mark(Document &_doc)
{
    _doc.setTool("remove");
    _doc.setRemoveAfterEachStroke(false);
    _doc.setBrushSize(100);
    _doc.beginStroke(60, 60);
    _doc.endStroke();
    _doc.setBrushSize(8);
    _doc.beginStroke(75, 60, true);
    _doc.endStroke();
}
} // namespace

class RemovalFinalizationTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QThreadPool::globalInstance()->setMaxThreadCount(2);
    }
    void pixelsMatchRasterReference_data()
    {
        QTest::addColumn<bool>("sampleAll");
        QTest::addColumn<bool>("soft");
        QTest::newRow("active-hard") << false << false;
        QTest::newRow("active-soft") << false << true;
        QTest::newRow("empty-hard") << true << false;
        QTest::newRow("empty-soft") << true << true;
    }
    void pixelsMatchRasterReference()
    {
        QFETCH(bool, sampleAll);
        QFETCH(bool, soft);
        QTemporaryDir dir;
        const auto    path = dir.filePath("remove.png");
        QVERIFY(fixture().save(path));
        Document doc;
        QVERIFY(doc.openFile(QUrl::fromLocalFile(path)));
        const auto base = doc.layerImage(0);
        if (sampleAll)
            doc.addLayer();
        if (soft) {
            doc.setTool("ellipticalSelect");
            doc.beginStroke(30, 30);
            doc.continueStroke(90, 90);
            doc.endStroke();
            doc.modifySelection("feather", 5);
            QTRY_VERIFY_WITH_TIMEOUT(!doc.selectionBusy(), 5000);
        }
        doc.setRemoveSampleAllLayers(sampleAll);
        mark(doc);
        const int  active = doc.activeLayer(), history = doc.historyIndex();
        const auto before        = doc.layerImage(active);
        const auto selectionMask = doc.selectionMask();
        const auto coverage      = soft ? selectionMask : QImage{};
        const auto region        = doc.removeRegion();
        QVERIFY(!region.contains(QPoint(75, 60)));
        const auto sample = sampleAll ? doc.composite() : before;
        auto       cancel = std::make_shared<std::atomic_bool>(false);
        const auto fill   = ContentFill::reconstruct(sample, region, region, 4, cancel);
        QVERIFY(!fill.isNull());
        // Full-image reference; production assembles a local patch and applies
        // coverage only to disjoint marked spans on its worker.
        QImage expected = before.copy();
        {
            QPainter painter(&expected);
            painter.setClipRegion(region);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.drawImage(region.boundingRect().topLeft(), fill);
        }
        SelectionCoverage::blend(expected, before, coverage, region.boundingRect());
        QVERIFY(expected != before);
        doc.applyRemoval();
        QVERIFY(doc.removeBusy());
        QCOMPARE(doc.layerImage(active), before);
        QCOMPARE(doc.historyIndex(), history);
        QTRY_VERIFY_WITH_TIMEOUT(!doc.removeBusy(), 5000);
        QCOMPARE(doc.layerImage(active), expected);
        QCOMPARE(doc.historyIndex(), history + 1);
        QVERIFY(!doc.removeHasPending());
        QCOMPARE(doc.layerImage(active).pixel(75, 60), before.pixel(75, 60));
        if (sampleAll)
            QCOMPARE(doc.layerImage(0), base);
        doc.undo();
        QCOMPARE(doc.layerImage(active), before);
        QCOMPARE(doc.selectionMask(), selectionMask);
        doc.redo();
        QCOMPARE(doc.layerImage(active), expected);
    }
    void cancellationRejectsLateResults_data()
    {
        QTest::addColumn<QString>("action");
        for (const auto &action : {"undo", "redo", "tool", "sampling"})
            QTest::newRow(action) << QString::fromLatin1(action);
    }
    void cancellationRejectsLateResults()
    {
        QFETCH(QString, action);
        QTemporaryDir dir;
        const auto    path = dir.filePath("cancel.png");
        QVERIFY(fixture().save(path));
        Document doc;
        QVERIFY(doc.openFile(QUrl::fromLocalFile(path)));
        QVERIFY(doc.saveProject(QUrl::fromLocalFile(dir.filePath("initial.pcr"))));
        QVERIFY(!doc.modified());
        const auto before  = doc.layerImage(0);
        const int  history = doc.historyIndex();
        mark(doc);
        const auto region = doc.removeRegion();
        doc.applyRemoval();
        QVERIFY(doc.removeBusy());
        QVERIFY(doc.modified());
        if (action == "undo")
            doc.undo();
        else if (action == "redo")
            doc.redo();
        else if (action == "tool")
            doc.setTool("brush");
        else
            doc.setRemoveSampleAllLayers(true);
        QVERIFY(!doc.removeBusy());
        QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
        QCoreApplication::processEvents();
        QCOMPARE(doc.layerImage(0), before);
        QCOMPARE(doc.historyIndex(), history);
        QVERIFY(!doc.modified());
        QCOMPARE(doc.removeRegion(), action == "sampling" ? region : QRegion{});
    }
    void pendingWorkCannotBeSavedAsAnOldImage()
    {
        QTemporaryDir dir;
        const auto    path = dir.filePath("save.png");
        QVERIFY(fixture().save(path));
        Document doc;
        QVERIFY(doc.openFile(QUrl::fromLocalFile(path)));
        const auto project = QUrl::fromLocalFile(dir.filePath("initial.pcr"));
        QVERIFY(doc.saveProject(project));
        mark(doc);
        doc.applyRemoval();
        QVERIFY(doc.removeBusy());
        const auto pending  = QUrl::fromLocalFile(dir.filePath("pending.pcr"));
        const auto exported = QUrl::fromLocalFile(dir.filePath("export.png"));
        QVERIFY(!doc.saveProject(pending));
        QVERIFY(!doc.exportImage(exported));
        QVERIFY(!QFile::exists(pending.toLocalFile()));
        QVERIFY(!QFile::exists(exported.toLocalFile()));
        doc.cancelRemoval();
        QVERIFY(!doc.modified());
        mark(doc);
        doc.applyRemoval();
        QTRY_VERIFY_WITH_TIMEOUT(!doc.removeBusy(), 5000);
        QVERIFY(doc.modified());
        QVERIFY(doc.saveProject(project));
        QVERIFY(!doc.modified());
        Document reopened;
        QVERIFY(reopened.openFile(project));
        QCOMPARE(reopened.layerImage(0), doc.layerImage(0));
    }
    void compactUniformNoOpAndFailureKeepTheDocumentClean()
    {
        Document doc;
        QVERIFY(doc.newDocument(180, 120, false));
        mark(doc);
        const int history = doc.historyIndex();
        QVERIFY(!doc.modified());
        doc.applyRemoval();
        QVERIFY(doc.removeBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!doc.removeBusy(), 5000);
        QCOMPARE(doc.historyIndex(), history);
        QVERIFY(!doc.modified());
        QVERIFY(!doc.removeHasPending());
        doc.setBrushSize(600);
        doc.beginStroke(90, 60);
        doc.endStroke();
        QCOMPARE(doc.removeRegion(), QRegion(QRect(0, 0, 180, 120)));
        doc.applyRemoval();
        QTRY_VERIFY_WITH_TIMEOUT(!doc.removeBusy(), 5000);
        QCOMPARE(doc.historyIndex(), history);
        QVERIFY(!doc.modified());
        // An unrepairable region keeps its marks available for correction.
        QVERIFY(doc.removeHasPending());
        doc.cancelRemoval();
        QVERIFY(!doc.removeHasPending());
    }
};
QTEST_MAIN(RemovalFinalizationTest)
#include "RemovalFinalizationTest.moc"
