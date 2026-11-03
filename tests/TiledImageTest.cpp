#include "storage/TiledImage.h"
#include "storage/PixelPlane.h"
#include "storage/ScratchImageCache.h"

#include <QColorSpace>
#include <QTemporaryDir>
#include <QtConcurrent>
#include <QtTest>

class TiledImageTest : public QObject
{
    Q_OBJECT
private slots:
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
