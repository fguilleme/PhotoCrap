#include "ImageAlpha.h"
#include <QtMath>

namespace PhotoCrap
{
QImage alphaCoverage(const QImage &_source, qreal _opacity)
{
    if (_source.isNull())
        return {};
    const auto format = _source.format();
    const bool wide =
        format == QImage::Format_RGBA64 || format == QImage::Format_RGBA64_Premultiplied;
    const bool   native = wide || format == QImage::Format_ARGB32 ||
                          format == QImage::Format_ARGB32_Premultiplied ||
                          format == QImage::Format_RGB32;
    const QImage source =
        native ? _source : _source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage coverage(source.size(), QImage::Format_Grayscale8);
    if (coverage.isNull())
        return {};
    const qreal opacity = qBound(0., _opacity, 1.);
    for (int y = 0; y < source.height(); ++y) {
        auto *mask = coverage.scanLine(y);
        if (wide) {
            const auto *pixels = reinterpret_cast<const QRgba64 *>(source.constScanLine(y));
            for (int x = 0; x < source.width(); ++x)
                mask[x] = uchar(qRound(pixels[x].alpha8() * opacity));
        } else {
            const auto *pixels = reinterpret_cast<const QRgb *>(source.constScanLine(y));
            for (int x = 0; x < source.width(); ++x)
                mask[x] = uchar(qRound(qAlpha(pixels[x]) * opacity));
        }
    }
    return coverage;
}
} // namespace PhotoCrap
