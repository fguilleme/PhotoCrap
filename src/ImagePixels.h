#pragma once
#include "ImageUtils.h"
#include <cmath>
#include <type_traits>

namespace ImageUtils
{
// Algorithms use the familiar 0..255 coordinate system, but never round a
// 16-bit channel to an 8-bit integer. Quantization happens only at output.
template <typename Pixel>
struct PixelAccess {
    static constexpr bool           wide   = std::is_same_v<Pixel, QRgba64>;
    static constexpr QImage::Format format = wide ? QImage::Format_RGBA64 : QImage::Format_ARGB32;
    static constexpr QImage::Format premultiplied =
        wide ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied;
    static double red(Pixel _p)
    {
        if constexpr (wide)
            return _p.red() / 257.;
        else
            return qRed(_p);
    }
    static double green(Pixel _p)
    {
        if constexpr (wide)
            return _p.green() / 257.;
        else
            return qGreen(_p);
    }
    static double blue(Pixel _p)
    {
        if constexpr (wide)
            return _p.blue() / 257.;
        else
            return qBlue(_p);
    }
    static double alpha(Pixel _p)
    {
        if constexpr (wide)
            return _p.alpha() / 257.;
        else
            return qAlpha(_p);
    }
    static double round(double _v)
    {
        return wide ? _v : qRound(_v);
    }
    static Pixel rgba(double _r, double _g, double _b, double _a)
    {
        auto channel = [](double _v) { return qRound(qBound(0., _v, 255.) * (wide ? 257. : 1.)); };
        if constexpr (wide)
            return QRgba64::fromRgba64(channel(_r), channel(_g), channel(_b), channel(_a));
        else
            return qRgba(channel(_r), channel(_g), channel(_b), channel(_a));
    }
    static QColor color(Pixel _p)
    {
        if constexpr (wide)
            return QColor::fromRgba64(_p);
        else
            return QColor::fromRgba(_p);
    }
};
} // namespace ImageUtils

namespace ImageUtils
{
template <typename Function>
QImage map16(const QImage &_source, Function _function)
{
    const QImage input = _source.convertToFormat(QImage::Format_RGBA64);
    QImage       result(input.size(), QImage::Format_RGBA64);
    QVector<int> rows(input.height());
    for (int y = 0; y < input.height(); ++y)
        rows[y] = y;
    blockingMap(rows, [&](int _y) {
        const auto *src = reinterpret_cast<const QRgba64 *>(input.constScanLine(_y));
        auto       *dst = reinterpret_cast<QRgba64 *>(result.scanLine(_y));
        for (int x = 0; x < input.width(); ++x)
            dst[x] = _function(src[x]);
    });
    result.setColorSpace(_source.colorSpace());
    return result.convertToFormat(QImage::Format_RGBA64_Premultiplied);
}
} // namespace ImageUtils
