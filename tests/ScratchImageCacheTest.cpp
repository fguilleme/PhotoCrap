#include "storage/ScratchImageCache.h"

#include <QColorSpace>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QtConcurrent>
#include <QtTest>
#include <cstring>
#include <limits>

class ScratchImageCacheTest : public QObject
{
    Q_OBJECT
    static QString blockPath(const QString &_root)
    {
        QDirIterator files(_root, {QStringLiteral("*.block")}, QDir::Files,
                           QDirIterator::Subdirectories);
        return files.hasNext() ? files.next() : QString{};
    }
private slots:
    void nativePixelsRoundTrip_data()
    {
        QTest::addColumn<int>("format");
        QTest::newRow("rgba8") << int(QImage::Format_ARGB32_Premultiplied);
        QTest::newRow("rgba16") << int(QImage::Format_RGBA64_Premultiplied);
        QTest::newRow("straight16") << int(QImage::Format_RGBA64);
        QTest::newRow("mask8-padded-rows") << int(QImage::Format_Grayscale8);
        QTest::newRow("mask16") << int(QImage::Format_Grayscale16);
    }
    void nativePixelsRoundTrip()
    {
        QFETCH(int, format);
        QTemporaryDir root;
        QVERIFY(root.isValid());
        ScratchImageCache cache({root.path(), 1024 * 1024, 0});
        QImage            original(37, 29, QImage::Format(format));
        std::memset(original.bits(), 0, original.sizeInBytes());
        original.fill(QColor::fromRgba64(12345, 34567, 56789, 45678));
        original.setPixelColor(12, 9, QColor::fromRgba64(21341, 32451, 43561, 54671));
        original.setColorSpace(QColorSpace::SRgb);
        original.setDevicePixelRatio(1.5);
        original.setDotsPerMeterX(3210);
        original.setDotsPerMeterY(4321);
        original.setOffset({3, 5});
        QVERIFY2(cache.insert("image", original),
                 qPrintable(cache.statistics()["error"].toString()));
        QImage loaded = cache.find("image");
        QCOMPARE(loaded, original);
        QCOMPARE(loaded.format(), original.format());
        QCOMPARE(loaded.bytesPerLine(), original.bytesPerLine());
        QCOMPARE(loaded.colorSpace(), original.colorSpace());
        QCOMPARE(loaded.devicePixelRatio(), original.devicePixelRatio());
        QCOMPARE(loaded.dotsPerMeterX(), original.dotsPerMeterX());
        QCOMPARE(loaded.offset(), original.offset());
        QCOMPARE(std::memcmp(loaded.constBits(), original.constBits(), original.sizeInBytes()), 0);
        QCOMPARE(cache.statistics()["mappedBytes"].toLongLong(), original.sizeInBytes());
        QImage changed = loaded;
        changed.fill(Qt::red);
        QCOMPARE(cache.find("image"), original);
        QCOMPARE(loaded, original);
        cache.clear();
        QCOMPARE(cache.statistics()["entries"].toInt(), 0);
        QVERIFY(QFile::exists(blockPath(root.path())));
        loaded = {};
        QCOMPARE(cache.statistics()["diskBytes"].toLongLong(), qint64(0));
        QVERIFY(blockPath(root.path()).isEmpty());
    }
    void mappingOutlivesCacheAndKeepsItsSession()
    {
        QTemporaryDir root;
        QImage        mapped;
        QImage        source(128, 64, QImage::Format_ARGB32_Premultiplied);
        source.fill(Qt::transparent);
        {
            ScratchImageCache cache({root.path(), 1024 * 1024, 0});
            QVERIFY(cache.insert("lease", source));
            mapped = cache.find("lease");
            QVERIFY(!mapped.isNull());
        }
        QVERIFY(!blockPath(root.path()).isEmpty());
        QCOMPARE(mapped.size(), source.size());
        mapped = {};
        QVERIFY(QDir(root.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }
    void lruAndPinnedBlocksRespectTheDiskBudget()
    {
        QTemporaryDir root;
        QImage        image(256, 128, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::blue);
        const qint64      bytes = image.sizeInBytes();
        ScratchImageCache cache({root.path(), bytes * 2 + 4096, 0});
        QVERIFY(cache.insert("a", image));
        QVERIFY(cache.insert("b", image));
        QVERIFY(!cache.find("a").isNull());
        QVERIFY(cache.insert("c", image));
        QVERIFY(cache.find("b").isNull());
        QVERIFY(!cache.find("a").isNull());
        QVERIFY(!cache.find("c").isNull());
        QVERIFY(cache.statistics()["diskBytes"].toLongLong() <= bytes * 2 + 4096);
        cache.clear();
        QCOMPARE(cache.statistics()["diskBytes"].toLongLong(), qint64(0));

        ScratchImageCache pinned({root.path(), bytes + 4096, 0});
        QVERIFY(pinned.insert("a", image));
        QImage lease = pinned.find("a");
        QVERIFY(!pinned.insert("b", image));
        QCOMPARE(lease, image);
        QVERIFY(pinned.statistics()["diskBytes"].toLongLong() <= bytes + 4096);
        lease = {};
        QVERIFY(pinned.insert("b", image));
    }
    void mappedImageCanOutliveItsWorkerThread()
    {
        QTemporaryDir     root;
        ScratchImageCache cache({root.path(), 1024 * 1024, 0});
        QImage            source(128, 64, QImage::Format_RGBA64_Premultiplied);
        source.fill(QColor::fromRgba64(12345, 34567, 56789, 45678));
        QVERIFY(cache.insert("worker", source));
        QImage mapped;
        {
            auto future = QtConcurrent::run([&] { return cache.find("worker"); });
            mapped      = future.result();
        }
        QCOMPARE(mapped, source);
        cache.clear();
        QVERIFY(cache.statistics()["diskBytes"].toLongLong() > 0);
        mapped = {};
        QCOMPARE(cache.statistics()["diskBytes"].toLongLong(), qint64(0));
    }
    void damagedBlocksAreDiscarded_data()
    {
        QTest::addColumn<QString>("damage");
        QTest::newRow("header") << QStringLiteral("header");
        QTest::newRow("pixels") << QStringLiteral("pixels");
        QTest::newRow("truncated") << QStringLiteral("truncated");
        QTest::newRow("deleted") << QStringLiteral("deleted");
    }
    void damagedBlocksAreDiscarded()
    {
        QFETCH(QString, damage);
        QTemporaryDir     root;
        ScratchImageCache cache({root.path(), 1024 * 1024, 0});
        QImage            image(128, 64, QImage::Format_RGBA64_Premultiplied);
        image.fill(Qt::green);
        QVERIFY(cache.insert("bad", image));
        QFile file(blockPath(root.path()));
        QVERIFY(file.open(QIODevice::ReadWrite));
        if (damage == "truncated")
            QVERIFY(file.resize(20));
        else {
            QVERIFY(file.seek(damage == "header" ? 0 : file.size() / 2));
            QCOMPARE(file.write("bad", 3), qint64(3));
        }
        file.close();
        if (damage == "deleted")
            QVERIFY(QFile::remove(file.fileName()));
        QVERIFY(cache.find("bad").isNull());
        QCOMPARE(cache.statistics()["entries"].toInt(), 0);
        QCOMPARE(cache.statistics()["diskBytes"].toLongLong(), qint64(0));
        QVERIFY(!cache.statistics()["error"].toString().isEmpty());
        QVERIFY(cache.insert("bad", image));
        QCOMPARE(cache.find("bad"), image);
    }
    void unavailableStorageDoesNotChangePixels()
    {
        QTemporaryDir root;
        QImage        image(128, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::red);
        const auto        key = image.cacheKey();
        ScratchImageCache disabled({root.path(), 0, 0});
        QVERIFY(!disabled.insert("a", image));
        QVERIFY(blockPath(root.path()).isEmpty());
        ScratchImageCache tooSmall({root.path(), 1, 0});
        QVERIFY(!tooSmall.insert("a", image));
        ScratchImageCache noSpace({root.path(), 1024 * 1024, std::numeric_limits<qint64>::max()});
        QVERIFY(!noSpace.insert("a", image));
        QVERIFY(blockPath(root.path()).isEmpty());
        QFile blocker(root.filePath("file"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        ScratchImageCache unavailable({blocker.fileName(), 1024 * 1024, 0});
        QVERIFY(!unavailable.insert("a", image));
        QCOMPARE(image.cacheKey(), key);
    }
    void activeSessionsAndUnrelatedFoldersSurviveCleanup()
    {
        QTemporaryDir root;
        QImage        image(128, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::blue);
        ScratchImageCache first({root.path(), 1024 * 1024, 0});
        QVERIFY(first.insert("a", image));
        const QString live   = blockPath(root.path());
        const QString orphan = root.filePath("session-abandoned");
        QVERIFY(QDir().mkpath(orphan));
        QFile marker(orphan + "/owner");
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("PhotoCrap scratch images / 1\n");
        marker.close();
        const QString unrelated = root.filePath("session-unrelated");
        QVERIFY(QDir().mkpath(unrelated));
        ScratchImageCache second({root.path(), 1024 * 1024, 0});
        QVERIFY(second.insert("b", image));
        QVERIFY(QFile::exists(live));
        QVERIFY(!QDir(orphan).exists());
        QVERIFY(QDir(unrelated).exists());
        QCOMPARE(first.find("a"), image);
    }
    void concurrentWorkersSeeOnlyCompleteBlocks()
    {
        QTemporaryDir     root;
        ScratchImageCache cache({root.path(), 1024 * 1024, 0});
        QImage            image(128, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::yellow);
        auto work   = [&] { return cache.insert("same", image) && cache.find("same") == image; };
        auto first  = QtConcurrent::run(work);
        auto second = QtConcurrent::run(work);
        const bool firstOk  = first.result();
        const bool secondOk = second.result();
        QVERIFY(firstOk);
        QVERIFY(secondOk);
        QCOMPARE(cache.statistics()["entries"].toInt(), 1);
        QCOMPARE(cache.statistics()["mappedBytes"].toLongLong(), qint64(0));
    }
    void clearingWhileWorkersReadAndWriteKeepsLeasesValid()
    {
        QTemporaryDir     root;
        ScratchImageCache cache({root.path(), 1024 * 1024, 0});
        QImage            image(256, 128, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::cyan);
        const auto work = [&](int _worker) {
            for (int i = 0; i < 30; ++i) {
                const QString key = QStringLiteral("%1/%2").arg(_worker).arg(i);
                cache.insert(key, image);
                const QImage loaded = cache.find(key);
                if (!loaded.isNull() && loaded != image)
                    return false;
            }
            return true;
        };
        auto first  = QtConcurrent::run(work, 1);
        auto second = QtConcurrent::run(work, 2);
        for (int i = 0; i < 20; ++i) {
            cache.clear();
            QTest::qWait(1);
        }
        const bool firstOk = first.result(), secondOk = second.result();
        QVERIFY(firstOk);
        QVERIFY(secondOk);
        cache.clear();
        QCOMPARE(cache.statistics()["diskBytes"].toLongLong(), qint64(0));
        QCOMPARE(cache.statistics()["mappedBytes"].toLongLong(), qint64(0));
    }
};

QTEST_GUILESS_MAIN(ScratchImageCacheTest)
#include "ScratchImageCacheTest.moc"
