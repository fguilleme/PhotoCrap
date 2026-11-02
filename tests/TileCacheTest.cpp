#include "render/TileCache.h"
#include <QRandomGenerator>
#include <QtTest>
#include <algorithm>

class TileCacheTest : public QObject
{
    Q_OBJECT
private slots:
    void visibleTilesAndRecentHistory()
    {
        using PhotoCrap::Render::TileCacheEntry;
        const QList<TileCacheEntry> entries{
            {1, 4, 1, true}, {2, 4, 9, false}, {3, 4, 8, false}, {4, 4, 2, false}};
        QCOMPARE(PhotoCrap::Render::retainedTiles(entries, 0), QSet<quint64>{1});
        QCOMPARE(PhotoCrap::Render::retainedTiles(entries, 3), QSet<quint64>{1});
        QCOMPARE(PhotoCrap::Render::retainedTiles(entries, 8), (QSet<quint64>{1, 2}));
        QCOMPARE(PhotoCrap::Render::retainedTiles(entries, 12), (QSet<quint64>{1, 2, 3}));
        QCOMPARE(PhotoCrap::Render::retainedTiles(entries, 16), (QSet<quint64>{1, 2, 3, 4}));
        auto moved       = entries;
        moved[0].visible = false;
        moved[0].lastUse = 20;
        moved[3].visible = true;
        QCOMPARE(PhotoCrap::Render::retainedTiles(moved, 8), (QSet<quint64>{1, 4}));
    }
    void differentFootprintsAndTies()
    {
        // A photo+mask pair costs twice a photo, while border tiles can be smaller.
        const QList<PhotoCrap::Render::TileCacheEntry> entries{
            {0, 8, 4, true}, {1, 8, 3, false}, {2, 4, 2, false}, {3, 4, 2, false}};
        QCOMPARE(PhotoCrap::Render::retainedTiles(entries, 12), (QSet<quint64>{0, 2}));
        QCOMPARE(PhotoCrap::Render::retainedTiles(entries, 16), (QSet<quint64>{0, 1}));
        QCOMPARE(PhotoCrap::Render::retainedTiles(entries, -1), QSet<quint64>{0});
        QVERIFY(PhotoCrap::Render::retainedTiles({}, 32).isEmpty());
    }
    void oversizedVisibleSetStaysComplete()
    {
        const QList<PhotoCrap::Render::TileCacheEntry> entries{
            {0, 8, 1, true}, {1, 8, 1, true}, {2, 1, 3, false}};
        QCOMPARE(PhotoCrap::Render::retainedTiles(entries, 2), (QSet<quint64>{0, 1}));
    }
    void matchesReferenceAcrossBudgets()
    {
        using namespace PhotoCrap::Render;
        QRandomGenerator random(14096);
        for (int sample = 0; sample < 500; ++sample) {
            QList<TileCacheEntry> entries;
            for (int i = 0; i < 80; ++i)
                entries.append({quint64(i), qint64(random.bounded(25)) - 2,
                                quint64(random.bounded(10)), random.bounded(4) == 0});
            const auto    unsorted = entries;
            const qint64  budget   = qint64(random.bounded(1200)) - 5;
            QSet<quint64> expected;
            qint64        remaining = qMax(qint64(0), budget);
            for (const auto &entry : entries) {
                if (entry.visible) {
                    expected.insert(entry.key);
                    remaining = qMax(qint64(0), remaining - qMax(qint64(0), entry.bytes));
                }
            }
            std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
                return a.lastUse == b.lastUse ? a.key < b.key : a.lastUse > b.lastUse;
            });
            for (const auto &entry : entries) {
                if (!entry.visible && entry.bytes > 0 && entry.bytes <= remaining) {
                    expected.insert(entry.key);
                    remaining -= entry.bytes;
                }
            }
            QCOMPARE(retainedTiles(unsorted, budget), expected);
        }
    }
};
QTEST_GUILESS_MAIN(TileCacheTest)
#include "TileCacheTest.moc"
