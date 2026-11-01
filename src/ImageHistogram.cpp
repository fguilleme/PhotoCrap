#include "ImageHistogram.h"
#include <QColorSpace>
#include <QtMath>
#include <array>
#include <cmath>
#include <cstring>

namespace PhotoCrap
{
QImage histogramSamples(const QImage &_source, int _stride)
{
    if (_source.isNull() || _stride < 1)
        return {};
    if (_stride == 1)
        return _source.convertToFormat(QImage::Format_ARGB32);
    const int width  = 1 + (_source.width() - 1) / _stride;
    const int height = 1 + (_source.height() - 1) / _stride;
    QImage    selected(width, height, _source.format());
    if (selected.isNull())
        return {};
    selected.setColorTable(_source.colorTable());
    selected.setColorSpace(_source.colorSpace());
    const int bytes = _source.depth() / 8;
    for (int y = 0; y < height; ++y) {
        if (_source.depth() == 1) {
            for (int x = 0; x < width; ++x)
                selected.setPixel(x, y, _source.pixelIndex(x * _stride, y * _stride));
        } else {
            const auto *source = _source.constScanLine(y * _stride);
            auto       *target = selected.scanLine(y);
            for (int x = 0; x < width; ++x)
                std::memcpy(target + qsizetype(x) * bytes, source + qsizetype(x) * _stride * bytes,
                            size_t(bytes));
        }
    }
    return selected.convertToFormat(QImage::Format_ARGB32);
}

namespace
{
using Bins = std::array<quint64, 256>;
int stride(const QImage &_source, qreal _target)
{
    const qint64 pixels = qint64(_source.width()) * _source.height();
    return qMax(1, int(std::sqrt(double(pixels) / _target)));
}
int pixelLuminance(QRgb _pixel)
{
    return qBound(0, qRound(.2126 * qRed(_pixel) + .7152 * qGreen(_pixel) + .0722 * qBlue(_pixel)),
                  255);
}
QVariantList normalized(const Bins &_bins, quint64 _maximum)
{
    QVariantList result;
    result.reserve(256);
    for (const quint64 count : _bins)
        result.append(_maximum == 0 ? 0. : double(count) / double(_maximum));
    return result;
}
} // namespace

ImageHistogramCache::Key ImageHistogramCache::key(const QImage &_source)
{
    return {_source.cacheKey(), _source.size(), _source.format()};
}
QVariantList ImageHistogramCache::luminance(const QImage &_source)
{
    const auto sourceKey = key(_source);
    if (m_hasLuminance && m_luminanceKey == sourceKey)
        return m_luminance;
    const QImage image = histogramSamples(_source, stride(_source, 262144.));
    Bins         bins{};
    quint64      maximum = 0;
    for (int y = 0; y < image.height(); ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = row[x];
            const auto value = pixelLuminance(pixel);
            bins[value] += quint64(qAlpha(pixel));
            maximum = qMax(maximum, bins[value]);
        }
    }
    const auto result = normalized(bins, maximum);
    // Do not pin an allocation failure as a valid result for a live image.
    if (_source.isNull() || !image.isNull()) {
        m_luminanceKey = sourceKey;
        m_luminance    = result;
        m_hasLuminance = true;
    }
    return result;
}
QVariantMap ImageHistogramCache::color(const QImage &_source)
{
    const auto sourceKey = key(_source);
    if (m_hasColor && m_colorKey == sourceKey)
        return m_color;
    const QImage        image = histogramSamples(_source, stride(_source, 65536.));
    std::array<Bins, 4> bins{};
    quint64             maximum = 0;
    for (int y = 0; y < image.height(); ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = row[x];
            const auto alpha = quint64(qAlpha(pixel));
            const int  r = qRed(pixel), g = qGreen(pixel), b = qBlue(pixel);
            const int  l = pixelLuminance(pixel);
            bins[0][r] += alpha;
            bins[1][g] += alpha;
            bins[2][b] += alpha;
            bins[3][l] += alpha;
            maximum =
                qMax(maximum, qMax(bins[3][l], qMax(bins[0][r], qMax(bins[1][g], bins[2][b]))));
        }
    }
    const QVariantMap result{{QStringLiteral("red"), normalized(bins[0], maximum)},
                             {QStringLiteral("green"), normalized(bins[1], maximum)},
                             {QStringLiteral("blue"), normalized(bins[2], maximum)},
                             {QStringLiteral("luminance"), normalized(bins[3], maximum)}};
    if (_source.isNull() || !image.isNull()) {
        m_colorKey = sourceKey;
        m_color    = result;
        m_hasColor = true;
    }
    return result;
}
} // namespace PhotoCrap
