#include "Document.h"
#include "tools/ContentFill.h"
#include "tools/RegionHealing.h"
#include <QFile>
#include <QPainter>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

namespace
{
QImage fixture()
{
    QImage image(240, 180, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            image.setPixelColor(x, y, QColor(100 + y % 8, 130 + y % 8, 160 + y % 8, 190));
    QPainter painter(&image);
    painter.fillRect(46, 46, 8, 8, QColor(230, 20, 30));
    return image;
}
void select(Document &_doc)
{
    _doc.setTool("ellipticalSelect");
    _doc.beginStroke(30, 30);
    _doc.continueStroke(70, 70);
    _doc.endStroke();
}
} // namespace

class ContentMoveFinalizationTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QThreadPool::globalInstance()->setMaxThreadCount(2);
    }
    void softSelectionsMatchRasterReference_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<int>("dx");
        QTest::newRow("move-overlap") << 0 << 4;
        QTest::newRow("move-separated") << 0 << 90;
        QTest::newRow("extend-overlap") << 1 << 4;
        QTest::newRow("extend-separated") << 1 << 90;
    }
    void softSelectionsMatchRasterReference()
    {
        QFETCH(int, mode);
        QFETCH(int, dx);
        QTemporaryDir dir;
        const auto    path = dir.filePath("move.png");
        QVERIFY(fixture().save(path));
        Document doc;
        QVERIFY(doc.openFile(QUrl::fromLocalFile(path)));
        select(doc);
        doc.modifySelection("feather", 3);
        QTRY_VERIFY_WITH_TIMEOUT(!doc.selectionBusy(), 5000);
        const auto original = doc.selectionRegion();
        const auto coverage = doc.selectionMask();
        const auto before   = doc.layerImage(0);
        const int  history  = doc.historyIndex();
        doc.setTool("contentAwareMove");
        doc.setContentMoveMode(mode);
        doc.setContentMoveColor(6);
        doc.setContentMoveStructure(4);
        doc.beginStroke(50, 50);
        doc.continueStroke(50 + dx, 50);
        doc.endStroke();
        QVERIFY(doc.contentMoveBusy());
        QCOMPARE(doc.layerImage(0), before);
        QCOMPARE(doc.historyIndex(), history);
        const auto destination = original.translated(dx, 0);
        auto       cancel      = std::make_shared<std::atomic_bool>(false);
        // Build an independent full-image reference by painting the two repair
        // outputs in order; the worker assembles separate local patches instead.
        QImage expected = before.copy();
        if (mode == 0) {
            const auto fill =
                ContentFill::reconstruct(before, original, original | destination, 4, cancel);
            QVERIFY(!fill.isNull());
            QPainter painter(&expected);
            painter.setClipRegion(original);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.drawImage(original.boundingRect().topLeft(), fill);
        }
        const auto moved = RegionHealing::blend(before, before, destination, QPoint(-dx, 0), 4, .6,
                                                false, 1, cancel);
        QVERIFY(!moved.isNull());
        {
            QPainter painter(&expected);
            painter.setClipRegion(destination);
            painter.drawImage(destination.boundingRect().topLeft(), moved);
        }
        QImage combined = coverage.copy();
        {
            QPainter painter(&combined);
            painter.setCompositionMode(QPainter::CompositionMode_Lighten);
            painter.drawImage(QPoint(dx, 0), coverage);
        }
        SelectionCoverage::blend(expected, before, combined,
                                 (original | destination).boundingRect());
        QVERIFY(expected != before);
        QTRY_VERIFY_WITH_TIMEOUT(!doc.contentMoveBusy(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!doc.patchDragging(), 5000);
        QCOMPARE(doc.layerImage(0), expected);
        QCOMPARE(doc.historyIndex(), history + 1);
        QCOMPARE(doc.selectionRegion(), destination);
        const QRegion affected = original | destination;
        for (int y = 0; y < before.height(); ++y)
            for (int x = 0; x < before.width(); ++x)
                if (!affected.contains(QPoint(x, y)))
                    QCOMPARE(doc.layerImage(0).pixel(x, y), before.pixel(x, y));
        doc.undo();
        QCOMPARE(doc.layerImage(0), before);
        QCOMPARE(doc.selectionMask(), coverage);
        doc.redo();
        QCOMPARE(doc.layerImage(0), expected);
    }
    void settingsAndUndoRejectLateResults_data()
    {
        QTest::addColumn<QString>("action");
        for (const auto &action : {"undo", "redo", "tool", "mode", "structure", "color", "layers"})
            QTest::newRow(action) << QString::fromLatin1(action);
    }
    void settingsAndUndoRejectLateResults()
    {
        QFETCH(QString, action);
        QTemporaryDir dir;
        const auto    path = dir.filePath("cancel.png");
        QVERIFY(fixture().save(path));
        Document doc;
        QVERIFY(doc.openFile(QUrl::fromLocalFile(path)));
        select(doc);
        QVERIFY(doc.saveProject(QUrl::fromLocalFile(dir.filePath("initial.pcr"))));
        QVERIFY(!doc.modified());
        const auto selection = doc.selectionRegion();
        const auto before    = doc.layerImage(0);
        const int  history   = doc.historyIndex();
        doc.setTool("contentAwareMove");
        doc.beginStroke(50, 50);
        doc.continueStroke(140, 50);
        doc.endStroke();
        QVERIFY(doc.contentMoveBusy());
        QVERIFY(doc.modified());
        if (action == "undo")
            doc.undo();
        else if (action == "redo")
            doc.redo();
        else if (action == "tool")
            doc.setTool("brush");
        else if (action == "mode")
            doc.setContentMoveMode(1);
        else if (action == "structure")
            doc.setContentMoveStructure(7);
        else if (action == "color")
            doc.setContentMoveColor(10);
        else
            doc.setContentMoveSampleAllLayers(true);
        QVERIFY(!doc.contentMoveBusy());
        QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
        QCoreApplication::processEvents();
        QCOMPARE(doc.layerImage(0), before);
        QCOMPARE(doc.historyIndex(), history);
        QCOMPARE(doc.selectionRegion(), selection);
        QVERIFY(doc.patchPreviewImage().isNull());
        QVERIFY(!doc.modified());
    }
    void pendingWorkProtectsSaveAndKeepsOnlyTheLatestGesture()
    {
        QTemporaryDir dir;
        const auto    path = dir.filePath("latest.png");
        QVERIFY(fixture().save(path));
        Document doc;
        QVERIFY(doc.openFile(QUrl::fromLocalFile(path)));
        select(doc);
        QVERIFY(doc.saveProject(QUrl::fromLocalFile(dir.filePath("initial.pcr"))));
        QVERIFY(!doc.modified());
        const int  history = doc.historyIndex();
        const auto before  = doc.layerImage(0);
        doc.setTool("contentAwareMove");
        doc.setContentMoveMode(1);
        doc.setContentMoveStructure(7);
        doc.beginStroke(50, 50);
        doc.continueStroke(100, 50);
        doc.endStroke();
        QVERIFY(doc.contentMoveBusy());
        QVERIFY(doc.modified());
        const auto project  = QUrl::fromLocalFile(dir.filePath("latest.pcr"));
        const auto exported = QUrl::fromLocalFile(dir.filePath("export.png"));
        QVERIFY(!doc.saveProject(project));
        QVERIFY(!doc.exportImage(exported));
        QVERIFY(!QFile::exists(project.toLocalFile()));
        QVERIFY(!QFile::exists(exported.toLocalFile()));
        doc.cancelStroke();
        QVERIFY(!doc.modified());
        doc.beginStroke(50, 50);
        doc.continueStroke(150, 50);
        doc.endStroke();
        QVERIFY(doc.contentMoveBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!doc.contentMoveBusy(), 5000);
        QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
        QCoreApplication::processEvents();
        QCOMPARE(doc.historyIndex(), history + 1);
        QCOMPARE(doc.layerImage(0).pixelColor(150, 50), before.pixelColor(50, 50));
        QCOMPARE(doc.layerImage(0).pixelColor(100, 50), before.pixelColor(100, 50));
        QVERIFY(doc.saveProject(project));
        QVERIFY(!doc.modified());
        Document reopened;
        QVERIFY(reopened.openFile(project));
        QCOMPARE(reopened.layerImage(0), doc.layerImage(0));
        doc.undo();
        QCOMPARE(doc.layerImage(0), before);
    }
};
QTEST_MAIN(ContentMoveFinalizationTest)
#include "ContentMoveFinalizationTest.moc"
