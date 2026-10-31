#include "render/TileSnapshot.h"
#include <QColorSpace>
#include <QtTest>

class TileSnapshotTest : public QObject
{
    Q_OBJECT
private slots:
    void retainsOnlyIdenticalPixels_data()
    {
        QTest::addColumn<int>("format");
        for (auto format :
             {QImage::Format_Grayscale8, QImage::Format_RGB32, QImage::Format_ARGB32_Premultiplied,
              QImage::Format_RGBA64_Premultiplied})
            QTest::newRow(qPrintable(QString::number(int(format)))) << int(format);
    }
    void retainsOnlyIdenticalPixels()
    {
        QFETCH(int, format);
        QImage source(31, 17, QImage::Format(format));
        source.setColorSpace(QColorSpace::SRgb);
        for (int y = 0; y < source.height(); ++y)
            for (int x = 0; x < source.width(); ++x)
                source.setPixelColor(
                    x, y,
                    QColor::fromRgba64(ushort(x * 1751 + y * 349), ushort(x * 933 + y * 819),
                                       ushort(x * 259 + y * 2639), ushort(x * 1847 + y * 2397)));
        const QRect region(7, 3, 9, 8);
        const auto  tile = PhotoCrap::Render::tileSnapshot(source, region);
        QCOMPARE(tile, source.copy(region));
        QVERIFY(source.isDetached());
        auto next = PhotoCrap::Render::tileSnapshot(source.copy(), region, tile);
        QCOMPARE(next.cacheKey(), tile.cacheKey());
        source.setPixelColor(0, 0, Qt::white);
        next = PhotoCrap::Render::tileSnapshot(source, region, tile);
        QCOMPARE(next.cacheKey(), tile.cacheKey());
        source.setPixelColor(7, 3, Qt::white);
        next = PhotoCrap::Render::tileSnapshot(source, region, tile);
        QVERIFY(next.cacheKey() != tile.cacheKey());
        QCOMPARE(next, source.copy(region));
        // The last pixel in a row and the last row must both be compared.
        auto before = next;
        source.setPixelColor(region.right(), region.bottom(), Qt::black);
        next = PhotoCrap::Render::tileSnapshot(source, region, before);
        QVERIFY(next.cacheKey() != before.cacheKey());
        QCOMPARE(next, source.copy(region));
        QVERIFY(source.isDetached());
        before = next;
        source.setColorSpace(QColorSpace::DisplayP3);
        next = PhotoCrap::Render::tileSnapshot(source, region, before);
        QVERIFY(next.cacheKey() != before.cacheKey());
        QCOMPARE(next.colorSpace(), source.colorSpace());
        before = next;
        source.setDevicePixelRatio(2.);
        next = PhotoCrap::Render::tileSnapshot(source, region, before);
        QVERIFY(next.cacheKey() != before.cacheKey());
        QCOMPARE(next.devicePixelRatio(), 2.);
    }
    void nullAndBorderRegions()
    {
        QImage source(7, 5, QImage::Format_Grayscale8);
        source.fill(53);
        QVERIFY(PhotoCrap::Render::tileSnapshot({}, source.rect()).isNull());
        QVERIFY(PhotoCrap::Render::tileSnapshot(source, {}).isNull());
        const QRect beyond(-1, -1, 5, 4);
        QCOMPARE(PhotoCrap::Render::tileSnapshot(source, beyond), source.copy(beyond));
        const auto previous = source.copy(QRect(0, 0, 3, 2));
        QCOMPARE(PhotoCrap::Render::tileSnapshot(source, QRect(0, 0, 4, 2), previous),
                 source.copy(QRect(0, 0, 4, 2)));
        const auto color     = source.convertToFormat(QImage::Format_RGB32);
        const auto newFormat = PhotoCrap::Render::tileSnapshot(color, QRect(0, 0, 3, 2), previous);
        QCOMPARE(newFormat.format(), QImage::Format_RGB32);
        QVERIFY(newFormat.cacheKey() != previous.cacheKey());
    }
};
QTEST_GUILESS_MAIN(TileSnapshotTest)
#include "TileSnapshotTest.moc"
