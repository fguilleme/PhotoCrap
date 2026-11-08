#include "storage/BinaryProject.h"
#include "Document.h"
#include "ProjectFixture.h"
#include "storage/ProjectPreview.h"
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace PhotoCrap::Storage;
class BinaryProjectTest : public QObject
{
    Q_OBJECT
private slots:
    void nativeSamplesAreExactAndRepeatedPlanesShareData()
    {
        for (int bits : {8, 16}) {
            const auto format = bits == 8 ? QImage::Format_ARGB32_Premultiplied
                                          : QImage::Format_RGBA64_Premultiplied;
            QImage     source(529, 273, format);
            source.setColorSpace(QColorSpace::SRgb);
            for (int y = 0; y < source.height(); ++y)
                for (int x = 0; x < source.width(); ++x) {
                    const quint16 a = quint16((x * 137 + y * 61) & 65535);
                    if (bits == 16)
                        reinterpret_cast<QRgba64 *>(source.scanLine(y))[x] =
                            QRgba64::fromRgba64(a / 3, a / 2, a / 5, a);
                    else
                        reinterpret_cast<QRgb *>(source.scanLine(y))[x] =
                            qRgba((a >> 8) / 3, (a >> 8) / 2, (a >> 8) / 5, a >> 8);
                }
            PixelPlane    plane(source);
            QTemporaryDir dir;
            QFile         file(dir.filePath("native.pcr"));
            QVERIFY(file.open(QIODevice::ReadWrite));
            BinaryProjectWriter writer(file, {}, {});
            const int           id = writer.plane(plane);
            QVERIFY(id >= 0);
            QCOMPARE(writer.plane(plane), id);
            QVERIFY(writer.finish({{"test", 1}}));
            QVERIFY(file.size() < source.sizeInBytes());
            BinaryProjectReader reader;
            QVERIFY(reader.read(file));
            const auto restored = reader.plane(id, source.size(), format);
            QCOMPARE(QImage(restored), source);
            QVERIFY(reader.plane(id, {1, 1}, format).isNull());
            QVERIFY(reader.plane(id, source.size(), QImage::Format_Grayscale8).isNull());
            // Corrupt the first tile; no damaged native pixels may be published.
            QVERIFY(file.seek(64));
            char byte;
            QCOMPARE(file.read(&byte, 1), qint64(1));
            byte ^= 0x40;
            QVERIFY(file.seek(64));
            QCOMPARE(file.write(&byte, 1), qint64(1));
            file.flush();
            BinaryProjectReader broken;
            QVERIFY(broken.read(file));
            QVERIFY(broken.plane(id, source.size(), format).isNull());
        }
    }
    void uniformPlanesAndCancellationAreBounded()
    {
        QImage pixel(1, 1, QImage::Format_RGBA64_Premultiplied);
        pixel.fill(Qt::red);
        pixel.setColorSpace(QColorSpace::SRgb);
        pixel.setDevicePixelRatio(1.25);
        pixel.setDotsPerMeterX(11811);
        pixel.setDotsPerMeterY(11811);
        const auto plane = PixelPlane::uniform({6000, 4000}, pixel);
        QByteArray bytes;
        QBuffer    file(&bytes);
        QVERIFY(file.open(QIODevice::ReadWrite));
        BinaryProjectWriter writer(file, {}, {});
        for (int i = 0; i < 512; ++i)
            QCOMPARE(writer.plane(plane), 0);
        QVERIFY(writer.finish({}));
        QVERIFY(bytes.size() < 1024);
        BinaryProjectReader reader;
        QVERIFY(reader.read(file));
        const auto restored = reader.plane(0, {6000, 4000}, QImage::Format_RGBA64_Premultiplied);
        QVERIFY(restored.isUniform());
        QCOMPARE(restored.uniformPixel(), pixel);
        QVERIFY(!reader.read(file, [] { return false; }));
        bytes.clear();
        file.seek(0);
        BinaryProjectWriter cancelled(file, {}, [] { return false; });
        QCOMPARE(cancelled.plane(plane), 0);
        QVERIFY(!cancelled.finish({}));
    }
    void binaryContainerRoundTripAndLegacyMigration()
    {
        QTemporaryDir dir;
        Document      doc;
        QVERIFY(doc.newDocument(512, 384, true, 16));
        doc.setColor(QColor::fromRgba64(12000, 34000, 50000, 12345));
        doc.setBrushSize(37);
        doc.beginStroke(200, 180);
        doc.endStroke();
        doc.addLayerMask();
        doc.setEditingMask(true);
        doc.setColor(Qt::black);
        doc.beginStroke(210, 180);
        doc.endStroke();
        doc.setEditingMask(false);
        const auto before = doc.layerImage(0), mask = doc.maskImage(0);
        const auto path = dir.filePath("binary.pcr");
        QVERIFY(doc.saveProject(QUrl::fromLocalFile(path)));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.peek(8), QByteArray(BinaryHeader::Magic, 8));
        QVERIFY(!PhotoCrap::Preview::read(path, {100, 100}).isNull());
        Document restored;
        QVERIFY(restored.openFile(QUrl::fromLocalFile(path)));
        QCOMPARE(restored.layerImage(0), before);
        QCOMPARE(restored.maskImage(0), mask);
        auto legacy = legacyProjectFixture(file);
        QVERIFY(!legacy.isEmpty());
        const auto old = dir.filePath("legacy.pcr");
        QFile      out(old);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(QJsonDocument(legacy).toJson(QJsonDocument::Compact));
        out.close();
        Document migrated;
        QVERIFY(migrated.openFile(QUrl::fromLocalFile(old)));
        QVERIFY(migrated.saveProject(QUrl::fromLocalFile(old)));
        QFile check(old);
        QVERIFY(check.open(QIODevice::ReadOnly));
        QCOMPARE(check.peek(8), QByteArray(BinaryHeader::Magic, 8));
        const auto native = restored.composite();
        // Header and index corruption must preserve the currently open document.
        QFile bad(dir.filePath("truncated.pcr"));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        file.seek(0);
        auto truncated = file.readAll();
        truncated.chop(1);
        bad.write(truncated);
        bad.close();
        QVERIFY(!restored.openFile(QUrl::fromLocalFile(bad.fileName())));
        QCOMPARE(restored.composite(), native);
    }
    void sparseMasksKeepTheirBackgroundAndTileFootprint()
    {
        QImage pixel(1, 1, QImage::Format_Grayscale8);
        pixel.fill(255);
        TiledImage tiles({6000, 4000}, pixel);
        QImage     patch(17, 23, QImage::Format_Grayscale8);
        patch.fill(12);
        QVERIFY(tiles.writeRegion({259, 258}, patch));
        const auto plane = PixelPlane::fromTiles(tiles);
        QByteArray bytes;
        QBuffer    file(&bytes);
        QVERIFY(file.open(QIODevice::ReadWrite));
        BinaryProjectWriter writer(file, {}, {});
        QCOMPARE(writer.plane(plane), 0);
        QVERIFY(writer.finish({}));
        QVERIFY(bytes.size() < 8192);
        BinaryProjectReader reader;
        QVERIFY(reader.read(file));
        const auto restored = reader.plane(0, {6000, 4000}, QImage::Format_Grayscale8);
        QCOMPARE(restored.tiles().tileCount(), tiles.tileCount());
        QCOMPARE(restored.sizeInBytes(), plane.sizeInBytes());
        QCOMPARE(restored.pixelColor(259, 258).red(), 12);
        QCOMPARE(restored.pixelColor(3000, 3000).red(), 255);
    }
    void invalidHeadersAndIndexChecksumsAreRejected()
    {
        QByteArray bytes;
        QBuffer    output(&bytes);
        QVERIFY(output.open(QIODevice::ReadWrite));
        QImage pixel(1, 1, QImage::Format_ARGB32_Premultiplied);
        pixel.fill(Qt::red);
        BinaryProjectWriter writer(output, {}, {});
        QCOMPARE(writer.plane(PixelPlane(pixel)), 0);
        QVERIFY(writer.finish({}));
        BinaryProjectReader reader;
        QVERIFY(reader.read(output));
        for (int field : {8, 24, 32, 40, 48, 56}) {
            auto broken = bytes;
            BinaryHeader::number(broken.data() + field,
                                 field == 40 ? 33ULL * 1024 * 1024 : 0xffffffffffffffffULL);
            QBuffer file(&broken);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QVERIFY(!reader.read(file));
            QVERIFY(reader.plane(0, {1, 1}, QImage::Format_ARGB32_Premultiplied).isNull());
        }
        auto corrupt = bytes;
        corrupt[corrupt.size() - 1] ^= 1;
        QBuffer file(&corrupt);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!reader.read(file));
    }
    void realProjectProbe()
    {
        const auto input = qEnvironmentVariable("PHOTOCRAP_BINARY_PROBE");
        if (input.isEmpty())
            QSKIP("Set PHOTOCRAP_BINARY_PROBE to a legacy project for a read-only comparison.");
        QTemporaryDir dir;
        Document      doc;
        QElapsedTimer timer;
        timer.start();
        QVERIFY(doc.openFile(QUrl::fromLocalFile(input)));
        qInfo() << "Probe dimensions, channel bits, layers:" << doc.width() << doc.height()
                << doc.channelBits() << doc.layers().size();
        const auto loadOld = timer.elapsed();
        const auto path    = dir.filePath("binary.pcr");
        timer.restart();
        QVERIFY(doc.saveProject(QUrl::fromLocalFile(path)));
        const auto save = timer.elapsed();
        Document   reopened;
        timer.restart();
        QVERIFY(reopened.openFile(QUrl::fromLocalFile(path)));
        const auto load = timer.elapsed();
        QCOMPARE(reopened.layers().size(), doc.layers().size());
        QCOMPARE(reopened.channelBits(), doc.channelBits());
        qInfo() << "Legacy bytes, binary bytes, old load ms, binary save ms, binary load ms:"
                << QFileInfo(input).size() << QFileInfo(path).size() << loadOld << save << load;
        for (int i = 0; i < doc.layers().size(); ++i) {
            QCOMPARE(reopened.layerImage(i), doc.layerImage(i));
            QCOMPARE(reopened.maskImage(i), doc.maskImage(i));
        }
        QCOMPARE(reopened.composite(), doc.composite());
    }
    void finderFixture()
    {
        const auto path = qEnvironmentVariable("PHOTOCRAP_BINARY_FIXTURE");
        if (path.isEmpty())
            QSKIP("Set PHOTOCRAP_BINARY_FIXTURE for a native Finder fixture.");
        Document document;
        QVERIFY(document.newDocument(160, 120, false, 16));
        document.setColor(QColor(40, 100, 180));
        document.setBrushSize(65);
        document.beginStroke(80, 60);
        document.endStroke();
        QVERIFY(document.saveProject(QUrl::fromLocalFile(path)));
    }
};
QTEST_MAIN(BinaryProjectTest)
#include "BinaryProjectTest.moc"
