#pragma once

#include <QImage>
#include <QVector>

namespace PhotoCrap::ImageSampling
{
// Obtain the actual Qt nearest-neighbor phase without scaling the source
// image. A second axis reproduces Qt's two-axis path when both dimensions vary.
inline QVector<int> nearestAxis(int _source, int _destination, bool _vertical, bool _otherChanges,
                                bool _wide, bool _roundTrip = false)
{
    const int   other = _otherChanges ? 2 : 1;
    const QSize original(_vertical ? QSize(other, _source) : QSize(_source, other));
    QImage strip(original,
                 _wide ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied);
    if (strip.isNull())
        return {};
    for (int i = 0; i < _source; ++i)
        for (int j = 0; j < other; ++j) {
            auto     *row = strip.scanLine(_vertical ? i : j);
            const int x   = _vertical ? j : i;
            if (_wide)
                reinterpret_cast<QRgba64 *>(row)[x] = QRgba64::fromRgba64(0, 0, ushort(i), 65535);
            else
                reinterpret_cast<QRgb *>(row)[x] = 0xff000000u | quint32(i);
        }
    strip = strip.scaled(_vertical ? QSize(1, _destination) : QSize(_destination, 1),
                         Qt::IgnoreAspectRatio, Qt::FastTransformation);
    if (_roundTrip && !strip.isNull())
        strip = strip.scaled(original, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    if (strip.isNull())
        return {};
    QVector<int> values(_roundTrip ? _source : _destination);
    for (int i = 0; i < values.size(); ++i) {
        const auto *row = strip.constScanLine(_vertical ? i : 0);
        const int   x   = _vertical ? 0 : i;
        values[i]       = _wide ? int(reinterpret_cast<const QRgba64 *>(row)[x].blue())
                                : int(reinterpret_cast<const QRgb *>(row)[x] & 0x00ffffffu);
    }
    return values;
}
} // namespace PhotoCrap::ImageSampling
