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
#include <cstring>
#include <limits>

class TiledImageTest : public QObject
{
    Q_OBJECT
private slots:
    void logicalUniformPlanesKeepNativeBitsAndIndependentVersions()
    {
        QSet<qint64> versions;
        for (auto format : {QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8}) {
            QImage pixel(1, 1, format);
            if (format == QImage::Format_RGBA64_Premultiplied)
                *reinterpret_cast<QRgba64 *>(pixel.bits()) =
                    QRgba64::fromRgba64(123, 456, 789, 12345);
            else if (format == QImage::Format_ARGB32_Premultiplied)
                *reinterpret_cast<QRgb *>(pixel.bits()) = qRgba(17, 31, 49, 103);
            else
                pixel.bits()[0] = 71;
            pixel.setColorSpace(QColorSpace::DisplayP3);
            pixel.setDevicePixelRatio(1.5);
            pixel.setDotsPerMeterX(4200);
            pixel.setDotsPerMeterY(4300);
            auto plane = PixelPlane::uniform({6000, 4000}, pixel);
            QVERIFY(!plane.isNull());
            QVERIFY(plane.isUniform());
            QCOMPARE(plane.size(), QSize(6000, 4000));
            QCOMPARE(plane.format(), format);
            QCOMPARE(plane.sizeInBytes(), qint64(0));
            QVERIFY(plane.heapBlocks().isEmpty());
            QCOMPARE(plane.tiles().tileCount(), 0);
            const auto region = plane.region({5997, 3998, 3, 2});
            QCOMPARE(region.colorSpace(), pixel.colorSpace());
            QCOMPARE(region.devicePixelRatio(), pixel.devicePixelRatio());
            QCOMPARE(region.dotsPerMeterX(), 4200);
            QCOMPARE(region.dotsPerMeterY(), 4300);
            for (int y = 0; y < region.height(); ++y)
                for (int x = 0; x < region.width(); ++x)
                    QVERIFY(!std::memcmp(region.constScanLine(y) + x * pixel.depth() / 8,
                                         pixel.constBits(), size_t(pixel.depth() / 8)));
            const auto key = plane.cacheKey();
            QVERIFY(key != 0 && key != pixel.cacheKey());
            QVERIFY(!versions.contains(key));
            versions.insert(key);
            QVERIFY(plane.writeRegion({258, 257}, pixel, plane.size()));
            QCOMPARE(plane.cacheKey(), key);
            QCOMPARE(plane.tiles().tileCount(), 0);
            const auto before  = plane;
            QImage     changed = pixel.copy();
            changed.fill(Qt::black);
            QVERIFY(plane.writeRegion({258, 257}, changed, plane.size()));
            QVERIFY(plane.cacheKey() != key);
            QVERIFY(!versions.contains(plane.cacheKey()));
            versions.insert(plane.cacheKey());
            QCOMPARE(plane.tiles().tileCount(), 1);
            QCOMPARE(before.region({258, 257, 1, 1}), pixel);
            QCOMPARE(plane.region({258, 257, 1, 1}), changed);
            for (const QSize size : {QSize(1, 1), QSize(601, 503), QSize(6000, 4000)}) {
                const auto other = PixelPlane::uniform(size, pixel);
                QVERIFY(!other.isNull());
                QVERIFY(!versions.contains(other.cacheKey()));
                versions.insert(other.cacheKey());
                QCOMPARE(other.size(), size);
                QCOMPARE(other.tileImage(0, 0), pixel);
            }
        }
    }
    void logicalUniformPlanesRejectInvalidInputs()
    {
        QImage pixel(1, 1, QImage::Format_ARGB32_Premultiplied);
        pixel.fill(Qt::white);
        QVERIFY(PixelPlane::uniform({}, pixel).isNull());
        QVERIFY(PixelPlane::uniform({16385, 1}, pixel).isNull());
        QVERIFY(PixelPlane::uniform({16384, 16384}, pixel).isNull());
        QVERIFY(PixelPlane::uniform({32, 32}, {}).isNull());
        QVERIFY(PixelPlane::uniform({32, 32}, QImage(2, 2, pixel.format())).isNull());
        QVERIFY(PixelPlane::uniform({32, 32}, QImage(1, 1, QImage::Format_RGB888)).isNull());
    }

    void opacityMetadataSurvivesNativeEditsAndDiskMappings()
    {
        for (auto format :
             {QImage::Format_ARGB32_Premultiplied, QImage::Format_RGBA64_Premultiplied}) {
            QImage image(517, 523, format);
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    image.setPixelColor(x, y, QColor(x % 256, y % 256, (x + y) % 256));
            TiledImage plane(image.size(), format, Qt::transparent);
            QVERIFY(!plane.knownOpaque());
            QVERIFY(plane.replace(image));
            QVERIFY(plane.knownOpaque());
            const auto old = plane;
            QImage     patch(1, 1, format);
            patch.fill(Qt::transparent);
            QVERIFY(plane.writeRegion({256, 256}, patch));
            QVERIFY(!plane.knownOpaque());
            QVERIFY(old.knownOpaque());
            patch.fill(Qt::red);
            QVERIFY(plane.writeRegion({256, 256}, patch));
            QVERIFY(plane.knownOpaque());
            QTemporaryDir directory;
            QVERIFY(directory.isValid());
            ScratchImageCache cache({directory.path(), 16 * 1024 * 1024, 0});
            QVERIFY(plane.offload(cache));
            QVERIFY(plane.knownOpaque());
            QCOMPARE(plane.heapBytes(), qint64(0));
            QVERIFY(plane.paintTransient({255, 255, 1, 1}, [](QPainter &p) {
                p.setCompositionMode(QPainter::CompositionMode_Clear);
                p.fillRect(255, 255, 1, 1, Qt::transparent);
            }));
            QVERIFY(!plane.knownOpaque());
            QVERIFY(old.knownOpaque());

            // A tile that returns to the opaque background stops contributing
            // to the transparency count, even after conversion and a snapshot.
            TiledImage uniform(QSize(257, 257), format, Qt::white);
            patch.fill(Qt::transparent);
            QVERIFY(uniform.writeRegion({256, 256}, patch));
            const auto snapshot = uniform;
            QVERIFY(!uniform.knownOpaque());
            const auto other     = format == QImage::Format_ARGB32_Premultiplied
                                       ? QImage::Format_RGBA64_Premultiplied
                                       : QImage::Format_ARGB32_Premultiplied;
            const auto converted = uniform.convertedTo(other);
            QVERIFY(!converted.isNull());
            QVERIFY(!converted.knownOpaque());
            patch.fill(Qt::white);
            QVERIFY(uniform.writeRegion({256, 256}, patch));
            QCOMPARE(uniform.tileCount(), 0);
            QVERIFY(uniform.knownOpaque());
            QVERIFY(!snapshot.knownOpaque());
        }
    }
    void smallNativeTileEditsStoreBoundedDeltas()
    {
        for (const auto format : {QImage::Format_ARGB32_Premultiplied,
                                  QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8}) {
            QImage expected(256, 256, format);
            for (int y = 0; y < 256; ++y)
                for (int x = 0; x < 256; ++x)
                    expected.setPixelColor(
                        x, y, QColor((x * 13 + y) % 256, (y * 17 + x) % 256, (x + y * 11) % 256));
            TiledImage plane({256, 256}, format, Qt::white);
            QVERIFY(plane.replace(expected));
            QVector<TiledImage> versions{plane};
            QVector<QImage>     references{expected};
            const auto          untouched = plane.regionKey({0, 0, 5, 5});
            for (int i = 0; i < 40; ++i) {
                const QRect bounds(12 + (i % 10) * 20, 12 + (i / 10) * 20, 8, 8);
                QImage      patch(bounds.size(), format);
                patch.fill(QColor((i * 43) % 256, (i * 19) % 256, (i * 73) % 256));
                for (int y = 0; y < bounds.height(); ++y)
                    std::memcpy(
                        expected.scanLine(bounds.y() + y) + bounds.x() * expected.depth() / 8,
                        patch.constScanLine(y), size_t(bounds.width()) * expected.depth() / 8);
                if (i % 3 == 0)
                    QVERIFY(plane.writeRegion(bounds.topLeft(), patch));
                else if (i % 3 == 1)
                    QVERIFY(plane.paint(bounds, [&](QPainter &p) {
                        p.setCompositionMode(QPainter::CompositionMode_Source);
                        p.drawImage(bounds.topLeft(), patch);
                    }));
                else
                    QVERIFY(plane.replace(expected));
                QCOMPARE(plane.region(expected.rect()), expected);
                QCOMPARE(plane.tileImage(0, 0), expected);
                for (const auto &crop : {bounds, bounds.adjusted(-3, -2, 4, 6), QRect(3, 5, 2, 3)})
                    QCOMPARE(plane.region(crop), expected.copy(crop));
                if (i == 7)
                    QCOMPARE(
                        plane.scaledFast({127, 131}),
                        expected.scaled(127, 131, Qt::IgnoreAspectRatio, Qt::FastTransformation));
                QCOMPARE(plane.regionKey({0, 0, 5, 5}), untouched);
                QVERIFY(plane.heapBytes() <= expected.sizeInBytes() + 8 * patch.sizeInBytes());
                versions.append(plane);
                references.append(expected);
            }
            QHash<qint64, qint64> blocks;
            for (const auto &version : versions) {
                const auto own = version.heapBlocks();
                for (auto it = own.cbegin(); it != own.cend(); ++it)
                    blocks.insert(it.key(), it.value());
            }
            qint64 total = 0;
            for (auto bytes : blocks)
                total += bytes;
            QVERIFY(total < 7 * expected.sizeInBytes()); // Forty full versions need 41 tiles.
            QTemporaryDir directory;
            QVERIFY(directory.isValid());
            ScratchImageCache cache({directory.path(), 64 * 1024 * 1024, 0});
            for (int i = 0; i < versions.size(); ++i) {
                QVERIFY(versions[i].offload(cache));
                QCOMPARE(versions[i].heapBytes(), qint64(0));
                QCOMPARE(versions[i].region(expected.rect()), references[i]);
            }
            cache.clear();
            for (int i = 0; i < versions.size(); ++i)
                QCOMPARE(versions[i].region(expected.rect()), references[i]);
        }
    }
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
    void partialRegionKeysIgnorePixelsOutsideTheirIntersection()
    {
        for (const auto format : {QImage::Format_ARGB32_Premultiplied,
                                  QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8}) {
            TiledImage  plane({768, 512}, format, Qt::transparent);
            const QRect border(240, 30, 32, 40);
            const auto  emptyKey   = plane.regionKey(border);
            const auto  fullKey    = plane.regionKey({0, 0, 256, 256});
            const auto  background = plane.region(border);
            QVERIFY(plane.paint({10, 10, 20, 20},
                                [](QPainter &p) { p.fillRect(10, 10, 20, 20, Qt::red); }));
            QCOMPARE(plane.region(border), background);
            QVERIFY(plane.regionKey({0, 0, 256, 256}) != fullKey);
            QCOMPARE(plane.regionKey(border), emptyKey);
            const auto original = plane;
            QVERIFY(plane.paint({250, 40, 2, 2},
                                [](QPainter &p) { p.fillRect(250, 40, 2, 2, Qt::white); }));
            const auto changedKey = plane.regionKey(border);
            QVERIFY(changedKey != emptyKey);
            QCOMPARE(original.regionKey(border), emptyKey);
            QTemporaryDir directory;
            QVERIFY(directory.isValid());
            ScratchImageCache cache({directory.path(), 8 * 1024 * 1024, 0});
            QVERIFY(plane.offload(cache));
            cache.clear();
            QCOMPARE(plane.regionKey(border), changedKey);
            QImage restore(2, 2, format);
            restore.fill(Qt::transparent);
            QVERIFY(plane.writeRegion({250, 40}, restore));
            QCOMPARE(plane.regionKey(border), emptyKey);
            if (format == QImage::Format_RGBA64_Premultiplied) {
                QImage tiny(1, 1, format);
                tiny.fill(QColor::fromRgba64(0, 0, 0, 1));
                QVERIFY(plane.writeRegion({250, 40}, tiny));
                QVERIFY(plane.regionKey(border) != emptyKey);
            }
            TiledImage           empty({768, 512}, format, Qt::transparent);
            QList<QFuture<bool>> reads;
            for (int worker = 0; worker < 8; ++worker)
                reads.append(QtConcurrent::run([original, empty] {
                    for (int i = 0; i < 64; ++i) {
                        const QRect region(200 + i % 16, 35 + i / 16, 32, 40);
                        if (original.regionKey(region) != empty.regionKey(region))
                            return false;
                    }
                    return true;
                }));
            for (auto &read : reads)
                QVERIFY(read.result());
            TiledImage transient({768, 512}, format, Qt::transparent);
            QVERIFY(transient.paintTransient(
                {250, 40, 2, 2}, [](QPainter &p) { p.fillRect(250, 40, 2, 2, Qt::white); }));
            const auto firstKey = transient.regionKey(border);
            QVERIFY(transient.paintTransient(
                {250, 40, 2, 2}, [](QPainter &p) { p.fillRect(250, 40, 2, 2, Qt::black); }));
            QVERIFY(transient.regionKey(border) != firstKey);
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
            QCOMPARE(plane.heapBytes(),
                     qint64((patch.depth() / 8 + 3) & ~3)); // One pixel, native row alignment.
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
                QCOMPARE(plane.devicePixelRatio(), image.devicePixelRatio());
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
    void initialImportCompactsDominantUniformTilesExactly()
    {
        for (auto format : {QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8}) {
            QImage source(800, 520, format);
            source.fill(Qt::white);
            source.setColorSpace(QColorSpace::DisplayP3);
            source.setDevicePixelRatio(2);
            source.setDotsPerMeterX(4200);
            source.setDotsPerMeterY(4300);
            // The first pixel is deliberately not representative of the background.
            source.setPixelColor(0, 0, Qt::black);
            for (int y = 0; y < 256; ++y)
                for (int x = 256; x < 512; ++x)
                    source.setPixelColor(x, y, Qt::black);
            source.setPixelColor(780, 510, Qt::black);
            TiledImage plane;
            QVERIFY(plane.replace(source));
            QCOMPARE(plane.tileCount(), 3);
            const auto output = plane.region(source.rect());
            QCOMPARE(output, source);
            QCOMPARE(output.colorSpace(), source.colorSpace());
            QCOMPARE(output.devicePixelRatio(), 2);
            QCOMPARE(output.dotsPerMeterX(), 4200);
            QCOMPARE(output.dotsPerMeterY(), 4300);
            QCOMPARE(plane.backgroundPixel().pixelColor(0, 0), QColor(Qt::white));
            const auto        old = plane;
            const auto        key = plane.regionKey({256, 0, 256, 256});
            QTemporaryDir     dir;
            ScratchImageCache cache({dir.path(), 8 * 1024 * 1024, 0});
            QVERIFY(plane.offload(cache));
            source.setPixelColor(10, 10, Qt::black);
            QVERIFY(plane.replace(source));
            QCOMPARE(plane.region(source.rect()), source);
            QCOMPARE(plane.regionKey({256, 0, 256, 256}), key);
            QCOMPARE(plane.backgroundPixel(), old.backgroundPixel());
            cache.clear();
            QCOMPARE(old.region({10, 10, 1, 1}).pixelColor(0, 0), QColor(Qt::white));
        }
    }
    void importedUniformBackgroundPreservesRawPremultipliedBits()
    {
        for (auto format :
             {QImage::Format_ARGB32_Premultiplied, QImage::Format_RGBA64_Premultiplied}) {
            QImage    source(513, 300, format);
            const int bytes = source.depth() / 8;
            // Raw native values, including noncanonical premultiplied channels.
            const QByteArray pixel =
                QByteArray::fromHex(bytes == 4 ? "05040302" : "0500040003000200");
            for (int y = 0; y < source.height(); ++y)
                for (int x = 0; x < source.width(); ++x)
                    std::memcpy(source.scanLine(y) + x * bytes, pixel.constData(), size_t(bytes));
            TiledImage plane;
            QVERIFY(plane.replace(source));
            QCOMPARE(plane.tileCount(), 0);
            QCOMPARE(plane.region(source.rect()), source);
        }
    }
    void dominantBackgroundWeightsNativeEdgeAllocations()
    {
        QImage source(257, 512, QImage::Format_Grayscale8);
        source.fill(Qt::black);
        for (int y = 0; y < 256; ++y)
            std::memset(source.scanLine(y), 255, 256);
        // Two narrow black tiles weigh less than the large white one, but together
        // with the large black tile they make black the optimal background.
        TiledImage plane;
        QVERIFY(plane.replace(source));
        QCOMPARE(plane.backgroundPixel().constBits()[0], uchar(0));
        QCOMPARE(plane.tileCount(), 1);
        QCOMPARE(plane.allocatedBytes(), qint64(256 * 256));
        QCOMPARE(plane.region(source.rect()), source);
    }
    void tileOffloadCanStopBetweenWritesWithoutChangingPixels()
    {
        for (auto format : {QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8}) {
            QTemporaryDir     directory;
            ScratchImageCache cache({directory.path(), 8 * 1024 * 1024, 0});
            TiledImage        plane({768, 512}, format, Qt::white);
            QVERIFY(plane.paint({0, 0, 768, 512},
                                [](QPainter &p) { p.fillRect(0, 0, 768, 512, Qt::red); }));
            QCOMPARE(plane.tileCount(), 6);
            const auto expected = plane.region({0, 0, 768, 512});
            const auto key      = plane.regionKey({0, 0, 768, 512});
            const auto heap     = plane.heapBytes();
            QVERIFY(!plane.offload(cache, [] { return false; }));
            QCOMPARE(plane.heapBytes(), heap);
            QCOMPARE(cache.statistics()["entries"].toInt(), 0);
            int calls = 0;
            QVERIFY(!plane.offload(cache, [&] { return ++calls < 2; }));
            QCOMPARE(calls, 2);
            QCOMPARE(plane.heapBytes(), heap - 256LL * 256 * expected.depth() / 8);
            QCOMPARE(cache.statistics()["entries"].toInt(), 1);
            QVERIFY(cache.statistics()["error"].toString().isEmpty());
            cache.clear();
            QCOMPARE(plane.region({0, 0, 768, 512}), expected);
            QCOMPARE(plane.regionKey({0, 0, 768, 512}), key);
            QVERIFY(plane.offload(cache));
            QCOMPARE(plane.heapBytes(), qint64(0));
            QCOMPARE(plane.region({0, 0, 768, 512}), expected);
            QImage background(1, 1, format);
            background.fill(Qt::white);
            PixelPlane pixels(background);
            QImage     patch(12, 12, format);
            patch.fill(Qt::red);
            QVERIFY(pixels.writeRegion({100, 100}, patch, {768, 512}));
            const auto oldKey  = pixels.cacheKey();
            const auto oldHeap = pixels.tiles().heapBytes();
            QVERIFY(!pixels.offload(cache, [] { return false; }));
            QCOMPARE(pixels.tiles().heapBytes(), oldHeap);
            QCOMPARE(pixels.cacheKey(), oldKey);
            QVERIFY(pixels.offload(cache));
            QCOMPARE(pixels.tiles().heapBytes(), qint64(0));
            QCOMPARE(pixels.region({100, 100, 12, 12}), patch);
        }
    }
    void tiledNativeConversionsMatchQtAndKeepSnapshots()
    {
        QTemporaryDir     directory;
        ScratchImageCache cache({directory.path(), 32 * 1024 * 1024, 0});
        for (auto sourceFormat : {QImage::Format_ARGB32_Premultiplied,
                                  QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8})
            for (auto targetFormat :
                 {QImage::Format_ARGB32_Premultiplied, QImage::Format_RGBA64_Premultiplied,
                  QImage::Format_Grayscale8}) {
                QImage source(517, 301, sourceFormat);
                source.setColorSpace(QColorSpace::DisplayP3);
                source.setDevicePixelRatio(2);
                source.setDotsPerMeterX(4200);
                source.setDotsPerMeterY(4300);
                for (int y = 0; y < source.height(); ++y)
                    for (int x = 0; x < source.width(); ++x)
                        source.setPixelColor(x, y,
                                             QColor::fromRgba64(ushort(x * 173), ushort(y * 199),
                                                                ushort(x * 37 + y * 67),
                                                                ushort(20000 + (x + y) % 40000)));
                TiledImage plane;
                QVERIFY(plane.replace(source));
                QVERIFY(plane.offload(cache));
                const auto key       = plane.regionKey(source.rect());
                const auto blocks    = plane.storageBlocks();
                const auto converted = plane.convertedTo(targetFormat);
                QVERIFY(!converted.isNull());
                const auto output   = converted.region(source.rect());
                const auto expected = source.convertToFormat(targetFormat);
                QCOMPARE(output, expected);
                QCOMPARE(output.colorSpace(), expected.colorSpace());
                QCOMPARE(output.devicePixelRatio(), 2.);
                QCOMPARE(output.dotsPerMeterX(), 4200);
                QCOMPARE(output.dotsPerMeterY(), 4300);
                QCOMPARE(plane.regionKey(source.rect()), key);
                QCOMPARE(plane.storageBlocks(), blocks);
                QCOMPARE(plane.heapBytes(), qint64(0));
                if (sourceFormat == targetFormat)
                    QCOMPARE(converted.storageBlocks(), blocks);
                else {
                    int calls = 0;
                    QVERIFY(plane.convertedTo(targetFormat, [&] { return ++calls < 3; }).isNull());
                    QCOMPARE(calls, 3);
                    QCOMPARE(plane.region(source.rect()), source);
                }
                QVERIFY(plane.convertedTo(QImage::Format_Mono).isNull());
                PixelPlane pixels(source);
                QVERIFY(pixels.freeze());
                QVERIFY(pixels.offload(cache));
                const auto before   = pixels;
                const auto pixelKey = pixels.cacheKey();
                if (sourceFormat != targetFormat) {
                    int        calls     = 0;
                    const auto oldBlocks = pixels.storageBlocks();
                    QVERIFY(!pixels.convertTo(targetFormat, [&] { return ++calls < 3; }));
                    QCOMPARE(calls, 3);
                    QCOMPARE(pixels.cacheKey(), pixelKey);
                    QCOMPARE(pixels.storageBlocks(), oldBlocks);
                    QCOMPARE(pixels.format(), sourceFormat);
                }
                QVERIFY(pixels.convertTo(targetFormat));
                QCOMPARE(QImage(pixels), expected);
                QCOMPARE(QImage(before), source);
                if (sourceFormat == targetFormat)
                    QCOMPARE(pixels.cacheKey(), pixelKey);
                else
                    QVERIFY(pixels.cacheKey() != pixelKey);
                const auto convertedKey = pixels.cacheKey();
                QVERIFY(pixels.convertTo(targetFormat));
                QCOMPARE(pixels.cacheKey(), convertedKey);
                cache.clear();
                QCOMPARE(plane.region(source.rect()), source);
                QCOMPARE(QImage(pixels), expected);
            }
    }
    void sparseDepthConversionAllocatesOnlyPresentTiles()
    {
        TiledImage plane({6000, 4000}, QImage::Format_ARGB32_Premultiplied, Qt::white);
        QVERIFY(plane.paint({100, 100, 12, 12},
                            [](QPainter &p) { p.fillRect(100, 100, 12, 12, Qt::red); }));
        QCOMPARE(plane.tileCount(), 1);
        const auto converted = plane.convertedTo(QImage::Format_RGBA64_Premultiplied);
        QCOMPARE(converted.tileCount(), 1);
        QCOMPARE(converted.allocatedBytes(), 256LL * 256 * 8);
        QCOMPARE(
            converted.region({90, 90, 40, 40}),
            plane.region({90, 90, 40, 40}).convertToFormat(QImage::Format_RGBA64_Premultiplied));
        QCOMPARE(converted.region({4000, 3000, 1, 1}).pixelColor(0, 0), QColor(Qt::white));
        QImage pixel(1, 1, QImage::Format_ARGB32_Premultiplied);
        pixel.fill(Qt::white);
        PixelPlane pixels(pixel);
        QImage     patch(12, 12, pixel.format());
        patch.fill(Qt::red);
        QVERIFY(pixels.writeRegion({100, 100}, patch, {6000, 4000}));
        const auto before = pixels;
        const auto key    = pixels.cacheKey();
        QVERIFY(!pixels.convertTo(QImage::Format_RGBA64_Premultiplied, [] { return false; }));
        QCOMPARE(pixels.cacheKey(), key);
        QVERIFY(pixels.convertTo(QImage::Format_RGBA64_Premultiplied));
        QCOMPARE(pixels.size(), QSize(6000, 4000));
        QCOMPARE(pixels.sizeInBytes(), 256LL * 256 * 8);
        QCOMPARE(before.sizeInBytes(), 256LL * 256 * 4);
        QCOMPARE(pixels.region({100, 100, 12, 12}), patch.convertToFormat(pixels.format()));
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
        image.setColorSpace(QColorSpace::SRgb);
        TiledImage plane(image.size(), image.format(), Qt::transparent);
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
