#include "tools/SliceDescriptionCache.h"
#include "Document.h"
#include <QRegion>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

namespace
{
QRect recordRect(const QVariant &_value)
{
    const auto r = _value.toMap();
    return {r["x"].toInt(), r["y"].toInt(), r["width"].toInt(), r["height"].toInt()};
}
} // namespace
class SliceDescriptionCacheTest : public QObject
{
    Q_OBJECT
private slots:
    void preservesManualOrderAndAutomaticCoverage()
    {
        PhotoCrap::Tools::SliceDescriptionCache cache;
        const QSize                             size(512, 320);
        const QVector<QRect> manuals{{10, 20, 80, 70}, {50, 40, 120, 80}, {450, 270, 62, 50}};
        const auto           records = cache.records(size, manuals, true);
        QRegion              covered, automatic;
        for (const auto &r : manuals)
            covered += r;
        for (int i = 0; i < records.size(); ++i) {
            const auto value = records[i].toMap();
            QCOMPARE(value["index"].toInt(), i);
            QCOMPARE(value["automatic"].toBool(), i >= manuals.size());
            if (i < manuals.size())
                QCOMPARE(recordRect(records[i]), manuals[i]);
            else {
                const auto rect = recordRect(records[i]);
                QVERIFY((QRegion(rect) & covered).isEmpty());
                QVERIFY((QRegion(rect) & automatic).isEmpty());
                automatic += rect;
            }
        }
        QVERIFY(automatic == QRegion(QRect(QPoint{}, size)).subtracted(covered));
        const auto manualOnly = cache.records(size, manuals, false);
        QCOMPARE(manualOnly.size(), manuals.size());
        for (int i = 0; i < manuals.size(); ++i)
            QCOMPARE(manualOnly[i], records[i]);
    }
    void equalKeysReuseTheSharedRecordBuffer()
    {
        PhotoCrap::Tools::SliceDescriptionCache cache;
        const QVector<QRect>                    manuals{{10, 20, 80, 70}, {450, 270, 62, 50}};
        const auto                              held     = cache.records({512, 320}, manuals, true);
        const auto                             *buffer   = held.constData();
        const auto                              revision = cache.revision();
        QVERIFY(revision > 0);
        for (int i = 0; i < 64; ++i) {
            const QVector<QRect> equal(manuals.cbegin(), manuals.cend());
            QCOMPARE(cache.records({512, 320}, equal, true).constData(), buffer);
            QCOMPARE(cache.revision(), revision);
        }
        const auto bigger = cache.records({600, 400}, manuals, true);
        QVERIFY(bigger.constData() != buffer);
        QVERIFY(bigger != held);
        QVERIFY(cache.revision() > revision);
        QCOMPARE(cache.records({512, 320}, manuals, true), held);
        auto moved = manuals;
        moved[0].translate(20, 10);
        const auto changed = cache.records({512, 320}, moved, true);
        QVERIFY(changed != held);
        QCOMPARE(recordRect(changed[0]), moved[0]);
        std::reverse(moved.begin(), moved.end());
        const auto reordered = cache.records({512, 320}, moved, true);
        QCOMPARE(recordRect(reordered[0]), moved[0]);
        QCOMPARE(recordRect(reordered[1]), moved[1]);
    }
    void fragmentedAutomaticSlicesRemainCappedAndReusable()
    {
        QVector<QRect> manuals;
        QRegion        covered;
        for (int i = 0; i < 128; ++i) {
            manuals << QRect(2 + i * 4, 0, 1, 2048) << QRect(0, 2 + i * 4, 2048, 1);
        }
        for (const auto &r : manuals)
            covered += r;
        PhotoCrap::Tools::SliceDescriptionCache cache;
        const auto records = cache.records({2048, 2048}, manuals, true);
        QCOMPARE(records.size(), 1024);
        for (int i = 256; i < records.size(); ++i) {
            QCOMPARE(records[i].toMap()["index"].toInt(), i);
            QVERIFY(records[i].toMap()["automatic"].toBool());
            QVERIFY((QRegion(recordRect(records[i])) & covered).isEmpty());
        }
        QCOMPARE(cache.records({2048, 2048}, manuals, true).constData(), records.constData());
    }
    void documentDraftsReuseRecordsAndEditsInvalidateThem()
    {
        QTemporaryDir directory;
        for (int bits : {8, 16}) {
            Document d;
            QVERIFY(d.newDocument(81, 61, true, bits));
            const auto pixels = d.layerImage(0);
            d.setTool("slice");
            d.beginSliceGesture(5, 5);
            d.updateSliceGesture(46, 36);
            d.endSliceGesture();
            d.setSliceShowAuto(true);
            const auto  held     = d.slices();
            const auto *buffer   = held.constData();
            const auto  revision = d.sliceDescriptionRevision();
            QCOMPARE(d.sliceDescriptionSnapshot().constData(), buffer);
            d.setTool("sliceSelect");
            d.beginSliceGesture(20, 20, 0);
            for (int i = 1; i <= 10; ++i) {
                d.updateSliceGesture(20 + i, 20 + i);
                QCOMPARE(d.slices().constData(), buffer);
                QCOMPARE(d.sliceDescriptionRevision(), revision);
                QCOMPARE(d.sliceDescriptionSnapshot().constData(), buffer);
            }
            d.endSliceGesture(true);
            QCOMPARE(d.slices().constData(), buffer);
            QCOMPARE(d.sliceDescriptionRevision(), revision);
            d.beginSliceGesture(20, 20, 0);
            d.updateSliceGesture(25, 25);
            d.endSliceGesture();
            const auto moved = d.slices();
            QVERIFY(moved.constData() != buffer);
            const auto committedRevision = d.sliceDescriptionRevision();
            QVERIFY(committedRevision > revision);
            QCOMPARE(recordRect(moved[0]), QRect(10, 10, 41, 31));
            d.undo();
            QCOMPARE(d.slices(), held);
            QVERIFY(d.sliceDescriptionRevision() > committedRevision);
            d.redo();
            QCOMPARE(d.slices(), moved);
            QCOMPARE(d.layerImage(0), pixels);
            const auto project =
                QUrl::fromLocalFile(directory.filePath(QString::number(bits) + ".pcr"));
            QVERIFY(d.saveProject(project));
            QVERIFY(d.newDocument(47, 33, true, bits));
            d.setSliceShowAuto(true);
            QCOMPARE(d.slices().size(), 1);
            QCOMPARE(recordRect(d.slices()[0]), QRect(0, 0, 47, 33));
            QVERIFY(d.openFile(project));
            d.setSliceShowAuto(true);
            QCOMPARE(d.slices(), moved);
        }
    }
};
QTEST_MAIN(SliceDescriptionCacheTest)
#include "SliceDescriptionCacheTest.moc"
