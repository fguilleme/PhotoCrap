#pragma once
#include "ImagePixels.h"
#include <array>

// Repair searches use 0..255 coordinates, retaining fractional native channels.
// Coverage stays 8-bit; colour samples and output never pass through QRgb in 16-bit work.
namespace HealingPixels
{
using Color = std::array<qreal, 4>;
template <typename Pixel>
struct Access : ImageUtils::PixelAccess<Pixel> {
    static Pixel straight(Pixel _pixel)
    {
        if constexpr (ImageUtils::PixelAccess<Pixel>::wide)
            return _pixel.unpremultiplied();
        else
            return qUnpremultiply(_pixel);
    }
    static Pixel premultiplied(Pixel _pixel)
    {
        if constexpr (ImageUtils::PixelAccess<Pixel>::wide)
            return _pixel.premultiplied();
        else
            return qPremultiply(_pixel);
    }
    static Pixel over(Pixel _source, Pixel _before)
    {
        if constexpr (ImageUtils::PixelAccess<Pixel>::wide) {
            const quint64 inverse = 65535 - _source.alpha();
            const auto    channel = [inverse](quint64 _a, quint64 _b) {
                return _a + (_b * inverse + 32767) / 65535;
            };
            return QRgba64::fromRgba64(
                channel(_source.red(), _before.red()), channel(_source.green(), _before.green()),
                channel(_source.blue(), _before.blue()), channel(_source.alpha(), _before.alpha()));
        } else {
            const int inverse = 255 - qAlpha(_source);
            return qRgba(qRed(_source) + (qRed(_before) * inverse + 127) / 255,
                         qGreen(_source) + (qGreen(_before) * inverse + 127) / 255,
                         qBlue(_source) + (qBlue(_before) * inverse + 127) / 255,
                         qAlpha(_source) + (qAlpha(_before) * inverse + 127) / 255);
        }
    }
};
inline Color color(const QImage &_image, const QPoint &_point)
{
    if (ImageUtils::is16Bit(_image)) {
        using P          = Access<QRgba64>;
        const auto pixel = P::straight(
            reinterpret_cast<const QRgba64 *>(_image.constScanLine(_point.y()))[_point.x()]);
        return {P::red(pixel), P::green(pixel), P::blue(pixel), P::alpha(pixel)};
    }
    const auto pixel = qUnpremultiply(_image.pixel(_point));
    return {qreal(qRed(pixel)), qreal(qGreen(pixel)), qreal(qBlue(pixel)), qreal(qAlpha(pixel))};
}
} // namespace HealingPixels
