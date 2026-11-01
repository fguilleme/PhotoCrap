#include "ImageHistogram.h"
#include "Document.h"
#include <QColorSpace>
#include <QElapsedTimer>
#include <QtTest>
#include <array>
#include <cmath>

namespace
{
// Independent reference: convert the complete original image, then walk the old grid.
QVariantMap reference(const QImage &_source, bool _color)
{
    const QImage image  = _source.convertToFormat(QImage::Format_ARGB32);
    const qint64 pixels = qint64(image.width()) * image.height();
    const int    step   = qMax(1, int(std::sqrt(double(pixels) / (_color ? 65536. : 262144.))));
    std::array<std::array<quint64, 256>, 4> bins{};
    quint64                                 maximum = 0;
    for (int y = 0; y < image.height(); y += step) {
        const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); x += step) {
            const QRgb p = row[x];
            const int  l =
                qBound(0, qRound(.2126 * qRed(p) + .7152 * qGreen(p) + .0722 * qBlue(p)), 255);
            bins[3][l] += quint64(qAlpha(p));
            maximum = qMax(maximum, bins[3][l]);
            if (_color) {
                bins[0][qRed(p)] += quint64(qAlpha(p));
                bins[1][qGreen(p)] += quint64(qAlpha(p));
                bins[2][qBlue(p)] += quint64(qAlpha(p));
                maximum = qMax(maximum,
                               qMax(bins[0][qRed(p)], qMax(bins[1][qGreen(p)], bins[2][qBlue(p)])));
            }
        }
    }
    const auto normalize = [&](const auto &_bins) {
        QVariantList result;
        for (auto count : _bins)
            result.append(maximum ? double(count) / double(maximum) : 0.);
        return result;
    };
    if (!_color)
        return {{"luminance", normalize(bins[3])}};
    return {{"red", normalize(bins[0])},
            {"green", normalize(bins[1])},
            {"blue", normalize(bins[2])},
            {"luminance", normalize(bins[3])}};
}
QImage precise(QSize _size)
{
    QImage        image(_size, QImage::Format_RGBA64);
    const quint16 alphas[] = {0, 1, 127, 128, 256, 32767, 32768, 65534, 65535};
    for (int y = 0; y < image.height(); ++y) {
        auto *row = reinterpret_cast<QRgba64 *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const quint32 n = quint32(x) * 2654435761u + quint32(y) * 2246822519u;
            row[x]          = QRgba64::fromRgba64(quint16(n), quint16(n >> 11), quint16(n >> 7),
                                                  alphas[(x + 7 * y) % 9]);
        }
    }
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}
} // namespace

class ImageHistogramTest : public QObject
{
    Q_OBJECT
private slots:
    void sampledFormats_data()
    {
        QTest::addColumn<int>("format");
        QTest::addColumn<int>("step");
        for (int f = QImage::Format_Mono; f < QImage::NImageFormats; ++f)
            for (int step : {1, 3, 7, 999})
                QTest::newRow(qPrintable(QString("%1-step-%2").arg(f).arg(step))) << f << step;
    }
    void sampledFormats()
    {
        QFETCH(int, format);
        QFETCH(int, step);
        const QImage source = precise({134, 87}).convertToFormat(QImage::Format(format));
        QVERIFY2(!source.isNull(), qPrintable(QString("create format %1").arg(format)));
        const QImage full     = source.convertToFormat(QImage::Format_ARGB32);
        const QImage selected = PhotoCrap::histogramSamples(source, step);
        QCOMPARE(selected.format(), QImage::Format_ARGB32);
        QCOMPARE(selected.size(),
                 QSize(1 + (source.width() - 1) / step, 1 + (source.height() - 1) / step));
        for (int y = 0; y < selected.height(); ++y)
            for (int x = 0; x < selected.width(); ++x)
                QCOMPARE(selected.pixel(x, y), full.pixel(x * step, y * step));
    }
    void invalidAndTransparent()
    {
        QVERIFY(PhotoCrap::histogramSamples({}, 3).isNull());
        QVERIFY(PhotoCrap::histogramSamples(precise({3, 4}), 0).isNull());
        PhotoCrap::ImageHistogramCache cache;
        QCOMPARE(cache.luminance({}), reference({}, false)["luminance"].toList());
        QCOMPARE(cache.color({}), reference({}, true));
        QImage transparent(19, 13, QImage::Format_RGBA64_Premultiplied);
        transparent.fill(Qt::transparent);
        const auto histogram = cache.color(transparent);
        for (const auto &value : cache.luminance(transparent))
            QCOMPARE(value.toDouble(), 0.);
        for (const auto &channel : histogram)
            for (const auto &value : channel.toList())
                QCOMPARE(value.toDouble(), 0.);
    }
    void exactHistograms_data()
    {
        QTest::addColumn<int>("format");
        for (auto format : {QImage::Format_ARGB32, QImage::Format_ARGB32_Premultiplied,
                            QImage::Format_RGBA64, QImage::Format_RGBA64_Premultiplied})
            QTest::newRow(qPrintable(QString::number(format))) << int(format);
    }
    void exactHistograms()
    {
        QFETCH(int, format);
        const QImage source = precise({2051, 1027}).convertToFormat(QImage::Format(format));
        PhotoCrap::ImageHistogramCache cache;
        QCOMPARE(cache.luminance(source), reference(source, false)["luminance"].toList());
        QCOMPARE(cache.color(source), reference(source, true));
    }
    void reusesBinsWithoutRetainingPixels()
    {
        QImage source(37, 19, QImage::Format_ARGB32);
        source.fill(Qt::white);
        PhotoCrap::ImageHistogramCache cache;
        const auto                     first      = cache.luminance(source);
        const auto                     firstColor = cache.color(source);
        const auto                     again      = cache.luminance(source);
        const auto                     againColor = cache.color(source);
        QCOMPARE(first.constData(), again.constData());
        const auto red = firstColor["red"].toList(), againRed = againColor["red"].toList();
        QCOMPARE(red.constData(), againRed.constData());
        QVERIFY(source.isDetached());
        source.setPixelColor(0, 0, Qt::black);
        QVERIFY(cache.luminance(source) != first);
        QVERIFY(cache.color(source) != firstColor);
        QCOMPARE(cache.luminance(source), reference(source, false)["luminance"].toList());
        QCOMPARE(cache.color(source), reference(source, true));
        // Alternating sources cannot return a histogram for the previous image.
        QImage other = precise({17, 11});
        QCOMPARE(cache.color(other), reference(other, true));
        QCOMPARE(cache.color(source), reference(source, true));
        QCOMPARE(cache.luminance(other), reference(other, false)["luminance"].toList());
        QVERIFY(source.isDetached());
        QVERIFY(other.isDetached());
    }
    void paletteChangesInvalidate()
    {
        QImage source(21, 17, QImage::Format_Indexed8);
        source.setColorTable({qRgba(255, 0, 0, 255), qRgba(0, 255, 0, 128)});
        source.fill(0);
        PhotoCrap::ImageHistogramCache cache;
        const auto                     old = cache.color(source);
        source.setColor(0, qRgba(0, 0, 255, 100));
        QVERIFY(cache.color(source) != old);
        QCOMPARE(cache.color(source), reference(source, true));
        QCOMPARE(cache.luminance(source), reference(source, false)["luminance"].toList());
    }
    void large16BitGrid()
    {
        QImage source(6000, 4000, QImage::Format_RGBA64_Premultiplied);
        source.fill(QColor::fromRgba64(51234, 19283, 42300, 43567));
        for (int y = 0; y < 4000; y += 41)
            for (int x = 0; x < 6000; x += 31)
                source.setPixelColor(x, y,
                                     QColor::fromRgba64(quint16(x * 11), quint16(y * 16), 12345,
                                                        quint16((x + y) * 6)));
        const QImage luminance = PhotoCrap::histogramSamples(source, 9);
        const QImage color     = PhotoCrap::histogramSamples(source, 19);
        QCOMPARE(luminance.size(), QSize(667, 445));
        QCOMPARE(color.size(), QSize(316, 211));
        PhotoCrap::ImageHistogramCache cache;
        QElapsedTimer                  timer;
        timer.start();
        const auto old      = reference(source, false);
        const auto oldColor = reference(source, true);
        const auto oldTime  = timer.nsecsElapsed();
        timer.restart();
        QCOMPARE(cache.luminance(source), old["luminance"].toList());
        QCOMPARE(cache.color(source), oldColor);
        const auto newTime = timer.nsecsElapsed();
        timer.restart();
        for (int i = 0; i < 1000; ++i) {
            cache.luminance(source);
            cache.color(source);
        }
        qInfo() << "24 Mpx 16-bit histograms: full conversion" << oldTime / 1000000.
                << "ms, sampled" << newTime / 1000000. << "ms, 1000 cached pairs"
                << timer.nsecsElapsed() / 1000000. << "ms";
        QVERIFY(source.isDetached());
    }
    void documentInvalidation_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8-bit") << 8;
        QTest::newRow("16-bit") << 16;
    }
    void documentInvalidation()
    {
        QFETCH(int, bits);
        Document d;
        QVERIFY(d.newDocument(37, 29, false, bits));
        const auto before      = d.histogram();
        const auto beforeColor = d.colorHistogram();
        const auto check       = [&] {
            const QImage displayed = d.displayComposite();
            return d.histogram() == reference(displayed, false)["luminance"].toList() &&
                   d.colorHistogram() == reference(displayed, true);
        };
        QVERIFY(check());
        d.setColor(QColor(20, 100, 220));
        d.fillWithColor(true);
        const auto painted      = d.histogram();
        const auto paintedColor = d.colorHistogram();
        QVERIFY(painted != before);
        QVERIFY(paintedColor != beforeColor);
        QVERIFY(check());
        d.setDisplayChannel(1);
        QVERIFY(check());
        QVERIFY(d.colorHistogram() != paintedColor);
        d.setDisplayChannel(0);
        QCOMPARE(d.histogram(), painted);
        d.undo();
        QCOMPARE(d.histogram(), before);
        QCOMPARE(d.colorHistogram(), beforeColor);
        QVERIFY(check());
        d.redo();
        QCOMPARE(d.histogram(), painted);
        QCOMPARE(d.colorHistogram(), paintedColor);
        QVERIFY(check());
        QVERIFY(d.newDocument(41, 15, true, bits));
        QVERIFY(check());
        for (const auto &value : d.histogram())
            QCOMPARE(value.toDouble(), 0.);
    }
};
QTEST_MAIN(ImageHistogramTest)
#include "ImageHistogramTest.moc"
