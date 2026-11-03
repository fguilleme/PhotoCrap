#include "storage/TiledImage.h"
#include "storage/PixelPlane.h"
#include "storage/ScratchImageCache.h"

#include <QColorSpace>
#include <QElapsedTimer>
#include <QFile>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QtConcurrent>
#include <QtTest>
#include <limits>

class TiledImageTest : public QObject
{
    Q_OBJECT
private slots:
    void borrowedTileImagesKeepSnapshotsAndDiskLeases()
    {
        for (auto format : {QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8}) {
            TiledImage empty({6000, 4000}, format, Qt::white);
            QCOMPARE(empty.tileImage(5, 5).size(), QSize(1, 1));
            QCOMPARE(empty.tileCount(), 0);
            QCOMPARE(empty.allocatedBytes(), qint64(0));
            QVERIFY(empty.tileImage(-1, 0).isNull());
            QVERIFY(empty.tileImage(std::numeric_limits<int>::max(), 0).isNull());
            QTemporaryDir directory;
            QVERIFY(directory.isValid());
            ScratchImageCache cache({directory.path(), 8 * 1024 * 1024, 0});
            TiledImage        plane({768, 512}, format, Qt::white);
            QVERIFY(plane.paint({4, 4, 12, 12},
                                [](QPainter &p) { p.fillRect(4, 4, 12, 12, Qt::red); }));
            const QImage original = plane.tileImage(0, 0);
            QCOMPARE(original.constBits(), plane.tileImage(0, 0).constBits());
            QVERIFY(plane.offload(cache));
            QCOMPARE(plane.heapBytes(), qint64(0));
            const QImage mapped   = plane.tileImage(0, 0);
            QImage       writable = mapped;
            writable.setPixelColor(5, 5, Qt::blue);
            QCOMPARE(plane.region({5, 5, 1, 1}).pixelColor(0, 0), mapped.pixelColor(5, 5));
            const auto before = plane;
            QVERIFY(plane.paint({4, 4, 12, 12},
                                [](QPainter &p) { p.fillRect(4, 4, 12, 12, Qt::green); }));
            cache.clear();
            QCOMPARE(before.region({5, 5, 1, 1}).pixelColor(0, 0), original.pixelColor(5, 5));
            QCOMPARE(mapped.pixelColor(5, 5), original.pixelColor(5, 5));
            QVERIFY(plane.region({5, 5, 1, 1}).pixelColor(0, 0) != original.pixelColor(5, 5));
            QImage pixel(1, 1, format);
            pixel.fill(Qt::black);
            PixelPlane tiny(pixel);
            pixel.fill(Qt::red);
            QVERIFY(tiny.writeRegion({}, pixel, {1, 1}));
            QCOMPARE(tiny.tileImage(7, 3).pixelColor(0, 0), pixel.pixelColor(0, 0));
            QCOMPARE(tiny.region({123, 123, 1, 1}).pixelColor(0, 0), pixel.pixelColor(0, 0));
        }
    }
    void repeatedOnePixelWritesPreserveNativePixelsAndVersions()
    {
        for (auto format : {QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8}) {
            QImage background(1, 1, format);
            background.fill(Qt::black);
            background.setColorSpace(QColorSpace::DisplayP3);
            background.setDotsPerMeterX(4200);
            background.setDotsPerMeterY(4300);
            PixelPlane plane(background);
            QImage     first = background.copy();
            first.fill(QColor::fromRgba64(23456, 34567, 45678, 50000));
            QVERIFY(plane.writeRegion({}, first, {1, 1}));
            const auto old       = plane;
            const auto key       = plane.cacheKey();
            const auto pixelsKey = plane.regionKey({0, 0, 1, 1});
            const auto blocks    = plane.storageBlocks();
            QVERIFY(plane.freeze());
            QVERIFY(plane.writeRegion({}, first, {1, 1}));
            QCOMPARE(plane.cacheKey(), key);
            QCOMPARE(plane.storageBlocks(), blocks);
            QCOMPARE(plane.regionKey({0, 0, 1, 1}), pixelsKey);
            QImage second = background.copy();
            second.fill(QColor::fromRgba64(45678, 56789, 12345, 60000));
            QVERIFY(plane.writeRegion({}, second, {1, 1}));
            QVERIFY(plane.cacheKey() != key);
            QVERIFY(plane.regionKey({0, 0, 1, 1}) != pixelsKey);
            QCOMPARE(plane.region({0, 0, 1, 1}), second);
            QCOMPARE(old.region({0, 0, 1, 1}), first);
            const auto pixel = plane.tileImage(0, 0);
            QCOMPARE(pixel.colorSpace(), background.colorSpace());
            QCOMPARE(pixel.dotsPerMeterX(), 4200);
            QCOMPARE(pixel.dotsPerMeterY(), 4300);
            PixelPlane expanded = old;
            QVERIFY(expanded.writeRegion({258, 257}, second, {600, 500}));
            QCOMPARE(expanded.region({10, 10, 1, 1}), first);
            QCOMPARE(expanded.region({258, 257, 1, 1}), second);
            QCOMPARE(old.region({0, 0, 1, 1}), first);
        }
    }
    void identicalWritesKeepMappedTilesAndOnlyChangedTilesDetach()
    {
        for (auto format : {QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8}) {
            QTemporaryDir directory;
            QVERIFY(directory.isValid());
            ScratchImageCache cache({directory.path(), 8 * 1024 * 1024, 0});
            TiledImage        plane({768, 512}, format, Qt::white);
            bool              changed = true;
            QImage            blank(6, 6, format);
            blank.fill(Qt::white);
            const auto emptyKey = plane.regionKey({0, 0, 768, 512});
            QVERIFY(plane.writeRegion({254, 254}, blank, &changed));
            QVERIFY(!changed);
            QCOMPARE(plane.tileCount(), 0);
            QCOMPARE(plane.regionKey({0, 0, 768, 512}), emptyKey);
            QVERIFY(plane.paint({0, 0, 512, 256},
                                [](QPainter &p) { p.fillRect(0, 0, 512, 256, Qt::blue); }));
            QVERIFY(plane.offload(cache));
            QCOMPARE(plane.heapBytes(), qint64(0));
            const auto old    = plane;
            const auto oldKey = plane.regionKey({0, 0, 768, 512});
            QImage     patch  = plane.region({254, 254, 6, 6});
            QVERIFY(plane.writeRegion({254, 254}, patch, &changed));
            QVERIFY(!changed);
            QCOMPARE(plane.regionKey({0, 0, 768, 512}), oldKey);
            QCOMPARE(plane.heapBytes(), qint64(0));
            patch.setPixelColor(0, 0, Qt::green);
            QVERIFY(plane.writeRegion({254, 254}, patch, &changed));
            QVERIFY(changed);
            QCOMPARE(plane.region({254, 254, 6, 6}), patch);
            QCOMPARE(plane.heapBytes(), qint64(256) * 256 * patch.depth() / 8);
            QVERIFY(plane.regionKey({0, 0, 256, 256}) != old.regionKey({0, 0, 256, 256}));
            for (const auto untouched :
                 {QRect(256, 0, 256, 256), QRect(0, 256, 256, 256), QRect(256, 256, 256, 256)})
                QCOMPARE(plane.regionKey(untouched), old.regionKey(untouched));
            QVERIFY(plane.writeRegion({1000, 1000}, patch, &changed));
            QVERIFY(!changed);
            cache.clear();
            QCOMPARE(old.regionKey({0, 0, 768, 512}), oldKey);
        }
    }
    void identicalPixelPlaneWritesKeepLogicalVersions()
    {
        for (auto format : {QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8}) {
            QImage uniform(1, 1, format);
            uniform.fill(Qt::white);
            PixelPlane compact(uniform);
            QImage     patch(6, 6, format);
            patch.fill(Qt::white);
            const auto compactKey = compact.cacheKey();
            QVERIFY(compact.writeRegion({254, 254}, patch, {768, 512}));
            QCOMPARE(compact.size(), QSize(1, 1));
            QCOMPARE(compact.cacheKey(), compactKey);
            QImage image(768, 512, format);
            image.fill(Qt::white);
            image.setPixelColor(255, 255, Qt::red);
            PixelPlane plane(image);
            QVERIFY(plane.freeze());
            const auto key    = plane.cacheKey();
            const auto blocks = plane.storageBlocks();
            QVERIFY(plane.writeRegion({254, 254}, plane.region({254, 254, 6, 6}), image.size()));
            QCOMPARE(plane.cacheKey(), key);
            QCOMPARE(plane.storageBlocks(), blocks);
            QVERIFY(plane.writeRegion({1000, 1000}, patch, image.size()));
            QCOMPARE(plane.cacheKey(), key);
        }
    }
    void expandingUniformPlanesPreservesNativeMetadata()
    {
        for (auto format : {QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8})
            for (const auto space : {QColorSpace{}, QColorSpace(QColorSpace::DisplayP3)}) {
                QImage uniform(1, 1, format);
                uniform.fill(QColor::fromRgba64(12345, 23456, 34567, 45678));
                uniform.setColorSpace(space);
                uniform.setDevicePixelRatio(2);
                uniform.setDotsPerMeterX(4200);
                uniform.setDotsPerMeterY(4300);
                PixelPlane plane(uniform);
                const auto before = plane;
                for (const auto region : {QRect(10, 10, 1, 1), QRect(20, 20, 7, 9)}) {
                    const auto expanded = plane.region(region);
                    QCOMPARE(expanded.size(), region.size());
                    QCOMPARE(expanded.pixelColor(0, 0), uniform.pixelColor(0, 0));
                    QCOMPARE(expanded.colorSpace(), space);
                    QCOMPARE(expanded.devicePixelRatio(), uniform.devicePixelRatio());
                    QCOMPARE(expanded.dotsPerMeterX(), uniform.dotsPerMeterX());
                    QCOMPARE(expanded.dotsPerMeterY(), uniform.dotsPerMeterY());
                }
                QImage patch(3, 4, format);
                patch.fill(Qt::black);
                QVERIFY(plane.writeRegion({258, 257}, patch, {600, 500}));
                const auto outside = plane.region({10, 10, 7, 9});
                QCOMPARE(outside.colorSpace(), space);
                QCOMPARE(outside.devicePixelRatio(), uniform.devicePixelRatio());
                QCOMPARE(outside.dotsPerMeterX(), uniform.dotsPerMeterX());
                QCOMPARE(outside.dotsPerMeterY(), uniform.dotsPerMeterY());
                for (int y = 0; y < outside.height(); ++y)
                    for (int x = 0; x < outside.width(); ++x)
                        QCOMPARE(outside.pixelColor(x, y), uniform.pixelColor(0, 0));
                QCOMPARE(plane.region({258, 257, 3, 4}).pixelColor(0, 0), patch.pixelColor(0, 0));
                QCOMPARE(before.size(), QSize(1, 1));
                QCOMPARE(before.pixelColor(0, 0), uniform.pixelColor(0, 0));
                QCOMPARE(plane.tiles().tileCount(), 1);
            }
        QVERIFY(TiledImage({600, 500}, QImage{}).isNull());
        QVERIFY(TiledImage({600, 500}, QImage(2, 2, QImage::Format_ARGB32_Premultiplied)).isNull());
    }
    void realDiskFullPreservesMappedVersions()
    {
        const auto root = qEnvironmentVariable("PHOTOCRAP_FULL_DISK_DIRECTORY");
        if (root.isEmpty())
            QSKIP("Use tests/run-full-disk-test.sh for the isolated-volume test.");
        QStorageInfo storage(root);
        QVERIFY(storage.isValid());
        // Never fill an ordinary host folder, even if the environment is wrong.
        QCOMPARE(QFileInfo(storage.rootPath()).canonicalFilePath(),
                 QFileInfo(root).canonicalFilePath());
        QVERIFY(storage.bytesTotal() > 0 && storage.bytesTotal() <= 64LL * 1024 * 1024);
        QVERIFY(QFileInfo::exists(root + "/photocrap-isolated-test"));
        ScratchImageCache cache({root, 64LL * 1024 * 1024, 0});
        TiledImage        plane({768, 512}, QImage::Format_RGBA64_Premultiplied, Qt::white);
        QVERIFY(plane.paint({4, 4, 12, 12},
                            [](QPainter &p) { p.fillRect(QRect(4, 4, 12, 12), Qt::red); }));
        QVERIFY(plane.offload(cache));
        QCOMPARE(plane.heapBytes(), qint64(0));
        const auto previous         = plane;
        const auto expectedPrevious = previous.region({0, 0, 768, 512});
        QVERIFY(plane.paint({400, 20, 12, 12},
                            [](QPainter &p) { p.fillRect(QRect(400, 20, 12, 12), Qt::blue); }));
        const auto expected = plane.region({0, 0, 768, 512});
        QFile      filler(root + "/filler");
        QVERIFY(filler.open(QIODevice::WriteOnly));
        const QByteArray block(1024 * 1024, 'f');
        for (int i = 0; i < 64; ++i)
            if (filler.write(block) != block.size())
                break;
        filler.flush();
        filler.close();
        QVERIFY(filler.open(QIODevice::Append));
        const QByteArray tail(4096, 't');
        for (int i = 0; i < 256; ++i)
            if (filler.write(tail) != tail.size())
                break;
        filler.flush();
        filler.close();
        storage.refresh();
        QVERIFY(storage.bytesAvailable() < 256LL * 256 * 8);
        QVERIFY(!plane.offload(cache));
        QVERIFY(!cache.statistics()["error"].toString().isEmpty());
        QVERIFY(plane.heapBytes() > 0);
        QCOMPARE(plane.region({0, 0, 768, 512}), expected);
        QCOMPARE(previous.region({0, 0, 768, 512}), expectedPrevious);
        cache.clear();
        QCOMPARE(previous.region({0, 0, 768, 512}), expectedPrevious);
        QVERIFY(plane.paint({500, 300, 8, 8},
                            [](QPainter &p) { p.fillRect(QRect(500, 300, 8, 8), Qt::green); }));
        QCOMPARE(plane.region({500, 300, 8, 8}).pixelColor(2, 2), QColor(Qt::green));
        QCOMPARE(previous.region({0, 0, 768, 512}), expectedPrevious);
    }
    void regionalVersionsSurviveOffloadAndIgnoreRemoteEdits()
    {
        QTemporaryDir     directory;
        ScratchImageCache cache({directory.path(), 32 * 1024 * 1024, 0});
        TiledImage        plane({1200, 400}, QImage::Format_RGBA64_Premultiplied, Qt::transparent);
        QVERIFY(plane.paint({0, 0, 10, 10},
                            [](QPainter &p) { p.fillRect(QRect(0, 0, 10, 10), Qt::red); }));
        const auto key = plane.regionKey({0, 0, 256, 256});
        QVERIFY(plane.offload(cache));
        QCOMPARE(plane.regionKey({0, 0, 256, 256}), key);
        QVERIFY(plane.paint({800, 0, 10, 10},
                            [](QPainter &p) { p.fillRect(QRect(800, 0, 10, 10), Qt::green); }));
        QCOMPARE(plane.regionKey({0, 0, 256, 256}), key);
        QVERIFY(plane.paint({0, 0, 10, 10},
                            [](QPainter &p) { p.fillRect(QRect(0, 0, 10, 10), Qt::blue); }));
        QVERIFY(plane.regionKey({0, 0, 256, 256}) != key);
    }

    void regionalFastScalingMatchesQt()
    {
        QTemporaryDir     directory;
        ScratchImageCache cache({directory.path(), 64 * 1024 * 1024, 0});
        for (auto format : {QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8})
            for (QSize size : {QSize(991, 777), QSize(517, 301), QSize(1, 37), QSize(39, 1),
                               QSize(3, 7), QSize(3, 16383), QSize(16383, 3)}) {
                QImage image(size, format);
                image.setColorSpace(QColorSpace::SRgb);
                image.setDevicePixelRatio(2);
                image.setDotsPerMeterX(11811);
                image.setDotsPerMeterY(11811);
                for (int y = 0; y < size.height(); ++y)
                    for (int x = 0; x < size.width(); ++x)
                        image.setPixelColor(x, y,
                                            QColor::fromRgba64(ushort(x * 173), ushort(y * 199),
                                                               ushort(x * 37 + y * 67),
                                                               ushort(20000 + (x + y) % 40000)));
                PixelPlane plane(image);
                QVERIFY(plane.offload(cache));
                for (QSize target : {QSize(128, 73), QSize(51, 33), QSize(1, 1), QSize(2, 3),
                                     QSize(733, 413), QSize(0, 10)})
                    for (auto aspect : {Qt::IgnoreAspectRatio, Qt::KeepAspectRatio,
                                        Qt::KeepAspectRatioByExpanding}) {
                        const auto projected = image.size().scaled(target, aspect);
                        if (qint64(projected.width()) * projected.height() > 1000000)
                            continue;
                        const auto expected = image.scaled(target, aspect, Qt::FastTransformation);
                        const auto actual   = plane.scaled(target, aspect, Qt::FastTransformation);
                        if (actual != expected) {
                            qInfo() << "Scale mismatch" << format << size << target << aspect
                                    << actual.size() << expected.size() << actual.format()
                                    << expected.format();
                            bool found = false;
                            for (int y = 0; y < qMin(actual.height(), expected.height()) && !found;
                                 ++y)
                                for (int x = 0; x < qMin(actual.width(), expected.width()); ++x)
                                    if (actual.pixelColor(x, y) != expected.pixelColor(x, y)) {
                                        qInfo()
                                            << "First differing sample" << x << y
                                            << actual.pixelColor(x, y) << expected.pixelColor(x, y);
                                        found = true;
                                        break;
                                    }
                        }
                        QCOMPARE(actual, expected);
                        QCOMPARE(actual.colorSpace(), expected.colorSpace());
                        QCOMPARE(actual.devicePixelRatio(), expected.devicePixelRatio());
                    }
                QVERIFY(plane.heapBlocks().isEmpty());
            }
    }

    void denseFastPreviewBenchmark()
    {
        if (!qEnvironmentVariableIsSet("PHOTOCRAP_DENSE_MEMORY_TEST"))
            QSKIP("Optional 24 MP preview benchmark.");
        QImage image(6000, 4000, QImage::Format_RGBA64_Premultiplied);
        for (int y = 0; y < image.height(); ++y) {
            auto *row = reinterpret_cast<QRgba64 *>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x)
                row[x] = QRgba64::fromRgba64(ushort(x * 13 + y * 7), ushort(x * 5 + y * 19),
                                             ushort(x * 3 + y * 11), 65535);
        }
        PixelPlane plane(image);
        QVERIFY(plane.freeze());
        image = {};
        QElapsedTimer timer;
        QImage        regional, native;
        timer.start();
        for (int i = 0; i < 5; ++i)
            regional = plane.scaled({1000, 667}, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        const auto regionalMs = timer.nsecsElapsed() / 1e6;
        timer.restart();
        for (int i = 0; i < 5; ++i)
            native =
                QImage(plane).scaled({1000, 667}, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        const auto nativeMs = timer.nsecsElapsed() / 1e6;
        QCOMPARE(regional, native);
        qInfo() << "Five native 24 MP to 1000x667 previews ms; regional" << regionalMs
                << "materialized reference" << nativeMs;
    }

    void noOpConversionsKeepMappedStorageAndVersions()
    {
        QTemporaryDir     directory;
        ScratchImageCache cache({directory.path(), 8 * 1024 * 1024, 0});
        QImage            image(512, 512, QImage::Format_RGBA64_Premultiplied);
        image.fill(Qt::white);
        image.setPixelColor(300, 300, QColor::fromRgba64(12345, 34567, 45678, 65535));
        image.setColorSpace(QColorSpace::SRgb);
        PixelPlane plane(image);
        QVERIFY(plane.offload(cache));
        const auto before = plane;
        const auto key    = plane.cacheKey();
        QVERIFY(plane.heapBlocks().isEmpty());
        plane.convertTo(plane.format());
        plane.setColorSpace(plane.colorSpace());
        QVERIFY(plane.heapBlocks().isEmpty());
        QCOMPARE(plane.cacheKey(), key);
        QCOMPARE(plane.region(image.rect()), image);
        plane.convertTo(QImage::Format_ARGB32_Premultiplied);
        QVERIFY(plane.freeze());
        QCOMPARE(plane.region(image.rect()),
                 image.convertToFormat(QImage::Format_ARGB32_Premultiplied));
        QCOMPARE(before.region(image.rect()), image);
    }

    void pixelPlaneVersionsAndRegionalWrites()
    {
        QImage image(512, 512, QImage::Format_RGBA64_Premultiplied);
        image.fill(Qt::white);
        PixelPlane plane(image);
        QVERIFY(plane.freeze());
        const auto version  = plane.cacheKey();
        const auto snapshot = plane;
        QImage     patch(4, 4, image.format());
        patch.fill(Qt::red);
        QVERIFY(plane.writeRegion({254, 254}, patch, image.size()));
        QVERIFY(plane.cacheKey() != version);
        QCOMPARE(QImage(snapshot), image);
        QPainter painter(&image);
        painter.drawImage(254, 254, patch);
        painter.end();
        QCOMPARE(QImage(plane), image);
        QVERIFY(plane.freeze());
        const auto after = plane.cacheKey();
        (void)plane.edit(); // Merely requesting writable access is not an edit.
        QCOMPARE(plane.cacheKey(), after);
        QVERIFY(plane.freeze());
        QCOMPARE(plane.cacheKey(), after);
        QVERIFY(!plane.writeRegion({0, 0}, QImage(2, 2, QImage::Format_Mono), image.size()));
        QCOMPARE(plane.cacheKey(), after);
        QCOMPARE(QImage(plane), image);
    }
    void importedPixelsAndVersionsRemainExact()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ScratchImageCache cache({dir.path(), 32LL * 1024 * 1024, 0});
        for (auto format : {QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8}) {
            QImage source(517, 301, format);
            source.setColorSpace(QColorSpace::SRgb);
            source.setDevicePixelRatio(2);
            source.setDotsPerMeterX(11811);
            source.setDotsPerMeterY(11811);
            for (int y = 0; y < source.height(); ++y)
                for (int x = 0; x < source.width(); ++x)
                    source.setPixelColor(x, y,
                                         QColor::fromRgba64(ushort(x * 173), ushort(y * 199),
                                                            ushort(x * 37 + y * 67), 65535));
            TiledImage plane;
            QVERIFY(plane.replace(source));
            QCOMPARE(plane.region(source.rect()), source);
            QVERIFY(plane.offload(cache));
            QCOMPARE(plane.heapBytes(), qint64(0));
            const TiledImage before   = plane;
            const QImage     original = source;
            source.setPixelColor(260, 270, Qt::white);
            QVERIFY(plane.replace(source));
            QCOMPARE(before.region(original.rect()), original);
            QCOMPARE(plane.region(source.rect()), source);
            const QImage metadata = plane.region(source.rect());
            QCOMPARE(metadata.colorSpace(), source.colorSpace());
            QCOMPARE(metadata.devicePixelRatio(), source.devicePixelRatio());
            QCOMPARE(metadata.dotsPerMeterX(), source.dotsPerMeterX());
            QCOMPARE(metadata.dotsPerMeterY(), source.dotsPerMeterY());
            QVERIFY(plane.heapBytes() <= 256 * 256 * 8);
            QVERIFY(plane.paint({10, 12, 3, 4}, [](QPainter &painter) {
                painter.fillRect(QRect(10, 12, 3, 4), Qt::red);
            }));
            QPainter painter(&source);
            painter.scale(.5, .5);
            painter.fillRect(QRect(10, 12, 3, 4), Qt::red);
            painter.end();
            QCOMPARE(plane.region(source.rect()), source);
            cache.clear();
            QCOMPARE(before.region(original.rect()), original);
            QCOMPARE(plane.region(source.rect()), source);
            QVERIFY(!plane.replace(QImage(10, 10, QImage::Format_Mono)));
            QCOMPARE(plane.region(source.rect()), source);
        }
    }
    void replacingWithBackgroundReleasesTiles()
    {
        QImage image(513, 300, QImage::Format_RGBA64_Premultiplied);
        image.fill(Qt::red);
        TiledImage plane;
        QVERIFY(plane.replace(image));
        QVERIFY(plane.tileCount() > 0);
        image.fill(Qt::transparent);
        QVERIFY(plane.replace(image));
        QCOMPARE(plane.tileCount(), 0);
        QCOMPARE(plane.region(image.rect()), image);
    }
    void largeUniformPlanesAllocateOnlyTouchedTiles()
    {
        TiledImage plane({6000, 4000}, QImage::Format_RGBA64_Premultiplied, Qt::transparent);
        QVERIFY(!plane.isNull());
        QCOMPARE(plane.tileCount(), 0);
        QCOMPARE(plane.allocatedBytes(), qint64(0));
        QVERIFY(plane.paint({100, 100, 3, 4}, [](QPainter &_painter) {
            _painter.fillRect(QRect(100, 100, 3, 4), Qt::red);
        }));
        QCOMPARE(plane.tileCount(), 1);
        QCOMPARE(plane.allocatedBytes(), qint64(256 * 256 * 8));
        QCOMPARE(plane.region({100, 100, 3, 4}).pixelColor(1, 1), QColor(Qt::red));
        const QImage empty = plane.region({5680, 3900, 20, 10});
        QCOMPARE(empty.size(), QSize(20, 10));
        QCOMPARE(empty.pixelColor(5, 5).alpha(), 0);
        QCOMPARE(plane.tileCount(), 1);
        QVERIFY(plane.region({-30, -30, 5, 5}).isNull());
    }
    void tiledPaintingMatchesTheNativeCanvas_data()
    {
        QTest::addColumn<int>("format");
        QTest::addColumn<bool>("transient");
        QTest::newRow("8-bit") << int(QImage::Format_ARGB32_Premultiplied) << false;
        QTest::newRow("16-bit") << int(QImage::Format_RGBA64_Premultiplied) << false;
        QTest::newRow("mask") << int(QImage::Format_Grayscale8) << false;
        QTest::newRow("8-bit-transient") << int(QImage::Format_ARGB32_Premultiplied) << true;
        QTest::newRow("16-bit-transient") << int(QImage::Format_RGBA64_Premultiplied) << true;
        QTest::newRow("mask-transient") << int(QImage::Format_Grayscale8) << true;
    }
    void tiledPaintingMatchesTheNativeCanvas()
    {
        QFETCH(int, format);
        QFETCH(bool, transient);
        const QSize  size(701, 529);
        const auto   nativeFormat = QImage::Format(format);
        const QColor background =
            nativeFormat == QImage::Format_Grayscale8 ? QColor(Qt::white) : QColor(Qt::transparent);
        TiledImage plane(size, nativeFormat, background);
        QImage     expected(size, nativeFormat);
        expected.fill(background);
        expected.setColorSpace(QColorSpace::SRgb);
        const auto paint = [](QPainter &_painter) {
            _painter.setRenderHint(QPainter::Antialiasing);
            _painter.translate(QPointF(256.25, 256.75));
            _painter.rotate(37);
            _painter.scale(1., .63);
            QRadialGradient gradient(QPointF{}, 90.);
            gradient.setColorAt(0, QColor::fromRgba64(12345, 34567, 56789, 45678));
            gradient.setColorAt(1, Qt::transparent);
            _painter.setBrush(gradient);
            _painter.setPen(Qt::NoPen);
            _painter.drawEllipse(QPointF{}, 90., 90.);
        };
        for (int i = 0; i < 3; ++i) {
            QVERIFY(transient ? plane.paintTransient({160, 160, 193, 193}, paint)
                              : plane.paint({160, 160, 193, 193}, paint));
            QPainter painter(&expected);
            paint(painter);
        }
        const QImage actual = plane.region(QRect(QPoint{}, size));
        QCOMPARE(actual.size(), expected.size());
        QCOMPARE(actual.format(), expected.format());
        // Qt's gradient rasterizer can round by one native channel unit when
        // clipped at different device origins. Storage/copy tests remain exact.
        int maximum = 0;
        for (int y = 0; y < size.height(); ++y)
            for (int x = 0; x < size.width(); ++x) {
                if (nativeFormat == QImage::Format_Grayscale8) {
                    maximum = qMax(maximum, qAbs(int(actual.constScanLine(y)[x]) -
                                                 int(expected.constScanLine(y)[x])));
                } else if (actual.depth() == 64) {
                    const auto a = reinterpret_cast<const QRgba64 *>(actual.constScanLine(y))[x];
                    const auto b = reinterpret_cast<const QRgba64 *>(expected.constScanLine(y))[x];
                    maximum =
                        qMax(maximum,
                             qMax(qMax(qAbs(a.red() - b.red()), qAbs(a.green() - b.green())),
                                  qMax(qAbs(a.blue() - b.blue()), qAbs(a.alpha() - b.alpha()))));
                } else {
                    const auto a = reinterpret_cast<const QRgb *>(actual.constScanLine(y))[x];
                    const auto b = reinterpret_cast<const QRgb *>(expected.constScanLine(y))[x];
                    maximum =
                        qMax(maximum,
                             qMax(qMax(qAbs(qRed(a) - qRed(b)), qAbs(qGreen(a) - qGreen(b))),
                                  qMax(qAbs(qBlue(a) - qBlue(b)), qAbs(qAlpha(a) - qAlpha(b)))));
                }
            }
        QVERIFY2(maximum <= 1,
                 qPrintable(QString("gradient difference: %1 native units").arg(maximum)));
        QCOMPARE(plane.region({245, 240, 40, 35}), actual.copy(245, 240, 40, 35));
        QCOMPARE(plane.region({690, 520, 11, 9}), actual.copy(690, 520, 11, 9));
    }
    void versionsShareUntouchedTilesAndKeepDiskLeases()
    {
        QTemporaryDir root;
        TiledImage    plane({600, 300}, QImage::Format_RGBA64_Premultiplied, Qt::transparent);
        QVERIFY(plane.paint({0, 0, 512, 256}, [](QPainter &_painter) {
            _painter.fillRect(QRect(0, 0, 512, 256),
                              QColor::fromRgba64(12345, 34567, 56789, 45678));
        }));
        const TiledImage before   = plane;
        const QImage     original = before.region({0, 0, 512, 256});
        QVERIFY(plane.paintTransient({20, 20, 5, 5}, [](QPainter &_painter) {
            _painter.fillRect(QRect(20, 20, 5, 5), Qt::red);
        }));
        QCOMPARE(before.region({0, 0, 512, 256}), original);
        QVERIFY(plane.region({20, 20, 5, 5}) != before.region({20, 20, 5, 5}));
        const QImage modified = plane.region({0, 0, 512, 256});
        {
            ScratchImageCache cache({root.path(), 8 * 1024 * 1024, 0});
            QVERIFY(before.offload(cache));
            QCOMPARE(cache.statistics()["entries"].toInt(), 2);
            QVERIFY(plane.offload(cache));
            // The second tile is shared with the old version, not written twice.
            QCOMPARE(cache.statistics()["entries"].toInt(), 3);
            QCOMPARE(before.heapBytes(), qint64(0));
            QCOMPARE(plane.heapBytes(), qint64(0));
            cache.clear();
            QVERIFY(cache.statistics()["diskBytes"].toLongLong() > 0);
            QCOMPARE(plane.region({0, 0, 512, 256}), modified);
        }
        // These are canonical tile versions, independent of the cache index.
        QCOMPARE(before.region({0, 0, 512, 256}), original);
        QCOMPARE(plane.region({0, 0, 512, 256}), modified);
        QVERIFY(plane.paintTransient({30, 30, 5, 5}, [](QPainter &_painter) {
            _painter.fillRect(QRect(30, 30, 5, 5), Qt::blue);
        }));
        QVERIFY(plane.heapBytes() > 0);
        QCOMPARE(before.region({0, 0, 512, 256}), original);
        ScratchImageCache unavailable({root.path(), 1, 0});
        const QImage      retained = plane.region({0, 0, 512, 256});
        QVERIFY(!plane.offload(unavailable));
        QCOMPARE(plane.region({0, 0, 512, 256}), retained);
    }
    void workerOffloadAndEditingSeparateVersions()
    {
        QTemporaryDir     root;
        ScratchImageCache cache({root.path(), 8 * 1024 * 1024, 0});
        TiledImage        plane({600, 300}, QImage::Format_ARGB32_Premultiplied, Qt::white);
        QVERIFY(plane.paint({0, 0, 512, 256}, [](QPainter &_painter) {
            _painter.fillRect(QRect(0, 0, 512, 256), Qt::green);
        }));
        const TiledImage snapshot = plane;
        auto worker = QtConcurrent::run([snapshot, &cache] { return snapshot.offload(cache); });
        const bool painted = plane.paint({0, 0, 10, 10}, [](QPainter &_painter) {
            _painter.fillRect(QRect(0, 0, 10, 10), Qt::red);
        });
        const bool stored  = worker.result();
        QVERIFY(painted);
        QVERIFY(stored);
        QCOMPARE(snapshot.region({0, 0, 10, 10}).pixelColor(2, 2), QColor(Qt::green));
        QCOMPARE(plane.region({0, 0, 10, 10}).pixelColor(2, 2), QColor(Qt::red));
    }
};

QTEST_GUILESS_MAIN(TiledImageTest)
#include "TiledImageTest.moc"
