#include "storage/TiledImage.h"
#include "storage/ScratchImageCache.h"

#include <QColorSpace>
#include <QTemporaryDir>
#include <QtConcurrent>
#include <QtTest>

class TiledImageTest : public QObject
{
    Q_OBJECT
private slots:
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
