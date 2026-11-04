#include "storage/ProjectWriteDevice.h"
#include "storage/StreamingImageWriter.h"
#include <QBuffer>
#include <QColorSpace>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

namespace
{
class ShortSink final : public QIODevice
{
public:
    ShortSink()
    {
        open(QIODevice::WriteOnly | QIODevice::Unbuffered);
    }
    QByteArray bytes;
    qint64     chunk = 7, failAfter = std::numeric_limits<qint64>::max(), largestRequest = 0;
    bool       zeroOnFailure = false;
    bool       isSequential() const override
    {
        return true;
    }

protected:
    qint64 readData(char *, qint64) override
    {
        return -1;
    }
    qint64 writeData(const char *_data, qint64 _size) override
    {
        largestRequest = qMax(largestRequest, _size);
        if (bytes.size() >= failAfter) {
            setErrorString(QStringLiteral("Simulated write failure"));
            return zeroOnFailure ? 0 : -1;
        }
        const auto count = qMin(_size, qMin(chunk, failAfter - bytes.size()));
        bytes.append(_data, count);
        return count;
    }
};
QByteArray input(int _size)
{
    QByteArray data(_size, Qt::Uninitialized);
    for (int i = 0; i < _size; ++i)
        data[i] = char((i * 37 + i / 7) & 255);
    return data;
}
} // namespace

class ProjectWriteTest : public QObject
{
    Q_OBJECT
private slots:
    void streamingImagesKeepNativePixelsAndBoundTheirReads()
    {
        for (int bits : {8, 16})
            for (bool png : {false, true}) {
                QImage source(257, 301,
                              bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                         : QImage::Format_ARGB32_Premultiplied);
                for (int y = 0; y < source.height(); ++y)
                    for (int x = 0; x < source.width(); ++x)
                        source.setPixelColor(
                            x, y,
                            QColor::fromRgba64(ushort(x * 211 + y * 3), ushort(y * 173 + x * 7),
                                               ushort(x * 71 + y * 31),
                                               ushort(17000 + (x * 113 + y * 37) % 48000)));
                ShortSink sink;
                QString   error;
                int       reads   = 0;
                bool      bounded = true;
                QVERIFY(PhotoCrap::Storage::writeImageBands(
                    sink, source.size(), bits, png,
                    [&](const QRect &_region) {
                        ++reads;
                        bounded &= _region.height() <= 256;
                        return source.copy(_region);
                    },
                    {}, error));
                QVERIFY(reads >= 2 && bounded);
                const QImage decoded = QImage::fromData(sink.bytes);
                QVERIFY(!decoded.isNull());
                QImage expected;
                if (png)
                    expected = source.convertToFormat(bits == 16 ? QImage::Format_RGBA64
                                                                 : QImage::Format_RGBA8888);
                else {
                    expected = QImage(source.size(), QImage::Format_RGB32);
                    expected.fill(Qt::white);
                    QPainter painter(&expected);
                    painter.drawImage(0, 0, source);
                    painter.end();
                }
                const auto actual = decoded.convertToFormat(expected.format());
                for (int y = 0; y < expected.height(); ++y)
                    for (int x = 0; x < expected.width(); ++x) {
                        if (actual.pixelColor(x, y).rgba64() != expected.pixelColor(x, y).rgba64())
                            qWarning() << bits << png << x << y << actual.pixelColor(x, y)
                                       << expected.pixelColor(x, y);
                        QCOMPARE(actual.pixelColor(x, y).rgba64(),
                                 expected.pixelColor(x, y).rgba64());
                    }
                QVERIFY(sink.largestRequest <= 64 * 1024);
                if (png) {
                    QCOMPARE(uchar(sink.bytes.at(24)), uchar(bits));
                    QCOMPARE(decoded.colorSpace(), QColorSpace(QColorSpace::SRgb));
                }
                ShortSink interrupted;
                interrupted.failAfter = 100;
                QVERIFY(!PhotoCrap::Storage::writeImageBands(
                    interrupted, source.size(), bits, png,
                    [&](const QRect &_region) { return source.copy(_region); }, {}, error));
                int       rows = 0;
                ShortSink cancelled;
                QVERIFY(!PhotoCrap::Storage::writeImageBands(
                    cancelled, source.size(), bits, png,
                    [&](const QRect &_region) { return source.copy(_region); },
                    [&](int, int) { return ++rows < 10; }, error));
            }
    }

    void recordingForwardsEveryByteAcrossItsCaptureLimit_data()
    {
        QTest::addColumn<int>("limit");
        QTest::addColumn<int>("chunk");
        for (int limit : {0, 1, 31, 4095, 4096, 4097})
            for (int chunk : {1, 7, 4096})
                QTest::newRow(qPrintable(QString("%1-%2").arg(limit).arg(chunk))) << limit << chunk;
    }
    void recordingForwardsEveryByteAcrossItsCaptureLimit()
    {
        QFETCH(int, limit);
        QFETCH(int, chunk);
        const auto           data = input(4096);
        ShortSink            sink;
        RecordingWriteDevice recording(sink, limit);
        for (int offset = 0; offset < data.size(); offset += chunk) {
            const auto size = qMin(qsizetype(chunk), data.size() - offset);
            QCOMPARE(recording.write(data.constData() + offset, size), qint64(size));
        }
        QCOMPARE(sink.bytes, data);
        QCOMPARE(recording.recordingComplete(), limit >= data.size());
        QCOMPARE(recording.recording(), limit >= data.size() ? data : QByteArray{});
    }
    void recordingDiscardsIncompletePayloadAfterTargetFailure()
    {
        for (bool zero : {false, true}) {
            ShortSink sink;
            sink.failAfter     = 5;
            sink.zeroOnFailure = zero;
            RecordingWriteDevice recording(sink, 100);
            QCOMPARE(recording.write("abc", 3), qint64(3));
            QCOMPARE(recording.recording(), QByteArray("abc"));
            QCOMPARE(recording.write("defghijkl", 9), qint64(-1));
            QVERIFY(!recording.recordingComplete());
            QVERIFY(recording.recording().isEmpty());
            QCOMPARE(sink.bytes, QByteArray("abcde"));
            QCOMPARE(recording.errorString(), sink.errorString());
            QCOMPARE(recording.write("z", 1), qint64(-1));
            QCOMPARE(sink.bytes, QByteArray("abcde"));
        }
    }

    void projectEncodingMemoryProbe()
    {
        const auto mode = qEnvironmentVariable("PHOTOCRAP_PROJECT_ENCODING_PROBE");
        if (mode != "legacy" && mode != "stream")
            QSKIP("Opt in with PHOTOCRAP_PROJECT_ENCODING_PROBE=legacy or stream.");
        const int bits = qEnvironmentVariableIntValue("PHOTOCRAP_PROJECT_ENCODING_BITS");
        QVERIFY(bits == 8 || bits == 16);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVector<QImage> planes;
        quint32         random = 0x6a09e667;
        const auto      next   = [&random] {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            return random;
        };
        for (int layer = 0; layer < 12; ++layer) {
            QImage plane(1024, 1024,
                         bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                    : QImage::Format_ARGB32_Premultiplied);
            QVERIFY(!plane.isNull());
            for (int y = 0; y < plane.height(); ++y)
                for (int x = 0; x < plane.width(); ++x) {
                    const auto r = next(), g = next(), b = next();
                    if (bits == 16)
                        reinterpret_cast<QRgba64 *>(plane.scanLine(y))[x] =
                            QRgba64::fromRgba64(ushort(r), ushort(g), ushort(b), 65535);
                    else
                        reinterpret_cast<QRgb *>(plane.scanLine(y))[x] =
                            qRgb(r & 255, g & 255, b & 255);
                }
            planes.append(std::move(plane));
        }
        const auto path = directory.filePath("probe.pcr");
        QSaveFile  file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QElapsedTimer timer;
        timer.start();
        if (mode == "legacy") {
            QJsonArray layers;
            for (const auto &plane : planes) {
                QByteArray bytes;
                QBuffer    buffer(&bytes);
                QVERIFY(buffer.open(QIODevice::WriteOnly));
                QVERIFY(plane.save(&buffer, "PNG"));
                layers.append(QJsonObject{{"png", QString::fromLatin1(bytes.toBase64())}});
            }
            const auto bytes =
                QJsonDocument(QJsonObject{{"layers", layers}}).toJson(QJsonDocument::Compact);
            QCOMPARE(file.write(bytes), bytes.size());
        } else {
            LimitedWriteDevice limited(file, 256LL * 1024 * 1024);
            const auto         write = [&](const QByteArray &_data) {
                return limited.write(_data) == _data.size();
            };
            QVERIFY(write("{\"layers\":["));
            bool first = true;
            for (const auto &plane : planes) {
                QVERIFY(write(first ? "{\"png\":\"" : ",{\"png\":\""));
                first = false;
                Base64WriteDevice encoder(limited);
                QVERIFY(plane.save(&encoder, "PNG"));
                QVERIFY(encoder.finish());
                QVERIFY(write("\"}"));
            }
            QVERIFY(write("]}"));
        }
        QVERIFY(file.commit());
        const auto elapsed = timer.elapsed();
        QFile      saved(path);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QCryptographicHash hash(QCryptographicHash::Sha256);
        QVERIFY(hash.addData(&saved));
        qInfo() << "Project memory fixture" << mode << bits << "bits; bytes" << saved.size()
                << "; encoding ms" << elapsed << "; SHA256" << hash.result().toHex();
    }
    void arbitraryChunksMatchQtBase64()
    {
        for (int size : {0, 1, 2, 3, 4, 17, 12287, 12288, 12289, 131075})
            for (int chunk : {1, 2, 3, 5, 12287, 12288, 12289, 65536}) {
                const auto         data = input(size), expected = data.toBase64();
                ShortSink          sink;
                LimitedWriteDevice limited(sink, expected.size());
                Base64WriteDevice  encoder(limited);
                for (int at = 0; at < size; at += chunk) {
                    const auto count = qMin(chunk, size - at);
                    QCOMPARE(encoder.write(data.constData() + at, count), qint64(count));
                }
                QVERIFY(encoder.finish());
                QVERIFY(encoder.finish());
                QCOMPARE(sink.bytes, expected);
                QCOMPARE(limited.writtenBytes(), expected.size());
                QVERIFY(sink.largestRequest <= 16 * 1024);
            }
    }
    void sizeLimitIncludesPaddingAndRejectsFurtherWrites()
    {
        const auto data = input(17), expected = data.toBase64();
        for (int limit = 0; limit <= expected.size() + 1; ++limit) {
            ShortSink          sink;
            LimitedWriteDevice limited(sink, limit);
            Base64WriteDevice  encoder(limited);
            const bool         success = encoder.write(data) == data.size() && encoder.finish();
            QCOMPARE(success, limit >= expected.size());
            QVERIFY(sink.bytes.size() <= limit);
            QCOMPARE(limited.writtenBytes(), sink.bytes.size());
            if (success)
                QCOMPARE(sink.bytes, expected);
            else {
                QVERIFY(limited.limitExceeded());
                QCOMPARE(limited.write("more"), qint64(-1));
                QVERIFY(!encoder.finish());
            }
        }
    }
    void downstreamFailuresAreSticky()
    {
        for (bool zero : {false, true}) {
            ShortSink sink;
            sink.failAfter     = 13;
            sink.zeroOnFailure = zero;
            LimitedWriteDevice limited(sink, 100);
            Base64WriteDevice  encoder(limited);
            QCOMPARE(encoder.write(input(30)), qint64(-1));
            QVERIFY(!encoder.finish());
            QVERIFY(!limited.limitExceeded());
            QCOMPARE(limited.writtenBytes(), qint64(13));
            QCOMPARE(limited.write("more"), qint64(-1));
            QCOMPARE(sink.bytes.size(), 13);
        }
    }
    void failedStreamDoesNotReplaceAnExistingFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto       path = directory.filePath("project.pcr");
        const QByteArray previous("Previous valid project");
        QFile            original(path);
        QVERIFY(original.open(QIODevice::WriteOnly));
        QCOMPARE(original.write(previous), previous.size());
        original.close();
        for (int limit : {0, 7, 20}) {
            {
                QSaveFile file(path);
                QVERIFY(file.open(QIODevice::WriteOnly));
                LimitedWriteDevice limited(file, limit);
                Base64WriteDevice  encoder(limited);
                const bool         success = encoder.write(input(64)) == 64 && encoder.finish();
                QVERIFY(!success);
                QVERIFY(limited.limitExceeded());
            }
            QVERIFY(original.open(QIODevice::ReadOnly));
            QCOMPARE(original.readAll(), previous);
            original.close();
        }
    }
    void pngWriterPropagatesAnEncodedSizeLimit()
    {
        QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::red);
        ShortSink          sink;
        LimitedWriteDevice limited(sink, 32);
        Base64WriteDevice  encoder(limited);
        const bool         success = image.save(&encoder, "PNG") && encoder.finish();
        QVERIFY(!success);
        QVERIFY(limited.limitExceeded());
        QVERIFY(!encoder.finish());
        QVERIFY(sink.bytes.size() <= 32);
    }
    void finishedEncoderRejectsFurtherData()
    {
        ShortSink         sink;
        Base64WriteDevice encoder(sink);
        QCOMPARE(encoder.write("ab"), qint64(2));
        QVERIFY(encoder.finish());
        const auto saved = sink.bytes;
        QCOMPARE(encoder.write("c"), qint64(-1));
        QVERIFY(!encoder.finish());
        QCOMPARE(sink.bytes, saved);
    }
    void nativePngPayloadMatchesThePreviousBufferEncoder()
    {
        for (const auto format : {QImage::Format_ARGB32_Premultiplied,
                                  QImage::Format_RGBA64_Premultiplied, QImage::Format_Grayscale8}) {
            QImage image(259, 257, format);
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    image.setPixelColor(x, y,
                                        QColor::fromRgba64(ushort(x * 227), ushort(y * 239),
                                                           ushort((x + y) * 113),
                                                           ushort(30000 + (x * y) % 35000)));
            QByteArray previous;
            QBuffer    buffer(&previous);
            QVERIFY(buffer.open(QIODevice::WriteOnly));
            QVERIFY(image.save(&buffer, "PNG"));
            ShortSink            sink;
            LimitedWriteDevice   limited(sink, 4 * 1024 * 1024);
            RecordingWriteDevice recording(limited, 4 * 1024 * 1024);
            Base64WriteDevice    encoder(recording);
            QVERIFY(image.save(&encoder, "PNG"));
            QVERIFY(encoder.finish());
            QCOMPARE(sink.bytes, previous.toBase64());
            QVERIFY(recording.recordingComplete());
            QCOMPARE(recording.recording(), previous.toBase64());
            QVERIFY(sink.largestRequest <= 16 * 1024);
        }
    }
};
QTEST_GUILESS_MAIN(ProjectWriteTest)
#include "ProjectWriteTest.moc"
