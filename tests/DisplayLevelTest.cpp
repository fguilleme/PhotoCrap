#include "render/DisplayLevel.h"
#include <QColorSpace>
#include <QtTest>

class DisplayLevelTest : public QObject
{
    Q_OBJECT
private slots:
    void regionalReduction_data()
    {
        QTest::addColumn<int>("format");
        QTest::addColumn<QSize>("size");
        QTest::addColumn<QSize>("target");
        for (auto format : {QImage::Format_RGB32, QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64_Premultiplied})
            for (const auto &sizes :
                 {qMakePair(QSize(64, 48), QSize(32, 24)), qMakePair(QSize(68, 52), QSize(17, 13)),
                  qMakePair(QSize(65, 49), QSize(32, 24)),
                  qMakePair(QSize(2048, 64), QSize(128, 4)), qMakePair(QSize(7, 3), QSize(1, 1))})
                QTest::newRow(qPrintable(QString("%1-%2x%3")
                                             .arg(int(format))
                                             .arg(sizes.first.width())
                                             .arg(sizes.first.height())))
                    << int(format) << sizes.first << sizes.second;
    }
    void regionalReduction()
    {
        QFETCH(int, format);
        QFETCH(QSize, size);
        QFETCH(QSize, target);
        QImage source(size, QImage::Format(format)), cache;
        source.setColorSpace(QColorSpace::SRgb);
        for (int y = 0; y < size.height(); ++y)
            for (int x = 0; x < size.width(); ++x)
                source.setPixelColor(
                    x, y,
                    QColor::fromRgba64(ushort(x * 1099 + y * 733), ushort(x * 911 + y * 2137),
                                       ushort(x * 5231 + y * 3331), ushort(x * 4831 + y * 2347)));
        auto update = PhotoCrap::Render::updateDisplayLevel(cache, source, target);
        QCOMPARE(update.changed, QRect(QPoint{}, target));
        QCOMPARE(update.sourcePixels, qint64(size.width()) * size.height());
        QVERIFY(!update.incremental);
        QCOMPARE(cache, source.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        const QList<QRect> edits{{0, 0, 1, 1},
                                 {size.width() - 1, size.height() - 1, 1, 1},
                                 {size.width() / 2, size.height() / 2, 3, 2},
                                 {1, 1, 5, 5},
                                 {size.width() / 3, 0, 1, size.height()}};
        for (const auto &edit : edits) {
            const QImage before  = cache;
            const QRect  changed = edit.intersected(source.rect());
            for (int y = changed.top(); y <= changed.bottom(); ++y)
                for (int x = changed.left(); x <= changed.right(); ++x)
                    source.setPixelColor(x, y, QColor::fromRgba64(57431, 19873, 43567, 12345));
            update = PhotoCrap::Render::updateDisplayLevel(cache, source, target, changed);
            const auto expected =
                source.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            QCOMPARE(cache, expected);
            QCOMPARE(cache.colorSpace(), source.colorSpace());
            for (int y = 0; y < target.height(); ++y)
                for (int x = 0; x < target.width(); ++x)
                    if (!update.changed.contains(x, y))
                        QCOMPARE(cache.pixelColor(x, y), before.pixelColor(x, y));
            const bool integral =
                size.width() % target.width() == 0 && size.height() % target.height() == 0;
            QCOMPARE(update.incremental, integral);
            if (!integral)
                QCOMPARE(update.sourcePixels, qint64(size.width()) * size.height());
        }
    }
    void localWorkAndInvalidation()
    {
        QImage source(2048, 1024, QImage::Format_RGBA64_Premultiplied), cache;
        source.fill(QColor::fromRgba64(45678, 12345, 54321, 32768));
        PhotoCrap::Render::updateDisplayLevel(cache, source, {512, 256});
        source.setPixelColor(1023, 511, Qt::white);
        const auto update =
            PhotoCrap::Render::updateDisplayLevel(cache, source, {512, 256}, {1023, 511, 1, 1});
        QVERIFY(update.incremental);
        QVERIFY(update.sourcePixels <= 16 * 16);
        QCOMPARE(cache, source.scaled(512, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        // A zoom change or missing delta must rebuild the whole level.
        auto rebuilt =
            PhotoCrap::Render::updateDisplayLevel(cache, source, {256, 128}, {10, 10, 1, 1});
        QVERIFY(!rebuilt.incremental);
        QCOMPARE(rebuilt.changed, cache.rect());
        rebuilt = PhotoCrap::Render::updateDisplayLevel(cache, source, {256, 128});
        QVERIFY(!rebuilt.incremental);
        QCOMPARE(rebuilt.changed, cache.rect());
        // Source format changes cannot patch an incompatible cached image.
        source  = source.convertToFormat(QImage::Format_RGB32);
        rebuilt = PhotoCrap::Render::updateDisplayLevel(cache, source, {256, 128}, {10, 10, 1, 1});
        QVERIFY(!rebuilt.incremental);
        QCOMPARE(cache.format(), source.format());
        PhotoCrap::Render::updateDisplayLevel(cache, {}, {});
        QVERIFY(cache.isNull());
    }
};
QTEST_GUILESS_MAIN(DisplayLevelTest)
#include "DisplayLevelTest.moc"
