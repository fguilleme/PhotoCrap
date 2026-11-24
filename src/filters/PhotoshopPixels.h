#pragma once
#include "ImagePixels.h"
#include <QVector3D>
#include <algorithm>
#include <array>
#include <numeric>

namespace PhotoCrap::Filters::Photoshop
{
using RGB = QVector3D;
template <class Function>
QImage mapColors(const QImage &source, Function transform)
{
    auto render = [&]<class Pixel>() {
        using P     = ImageUtils::PixelAccess<Pixel>;
        QImage out  = source.convertToFormat(P::format);
        auto  *data = out.bits();
        if (!data)
            return QImage();
        const auto   stride = out.bytesPerLine();
        const int    width  = out.width();
        QVector<int> rows(out.height());
        std::iota(rows.begin(), rows.end(), 0);
        ImageUtils::blockingMap(rows, [&](int y) {
            auto *row = reinterpret_cast<Pixel *>(data + y * stride);
            for (int x = 0; x < width; ++x) {
                const Pixel p      = row[x];
                const auto  result = transform(RGB(P::red(p), P::green(p), P::blue(p)));
                row[x]             = P::rgba(result.x(), result.y(), result.z(), P::alpha(p));
            }
        });
        return out.convertToFormat(P::premultiplied);
    };
    return ImageUtils::is16Bit(source) ? render.template operator()<QRgba64>()
                                       : render.template operator()<QRgb>();
}

inline float luminosity(RGB rgb)
{
    return .30f * rgb.x() + .59f * rgb.y() + .11f * rgb.z();
}

template <class T, size_t N>
T interpolate(const std::array<T, N> &table, double value)
{
    const double position = std::clamp(value, 0., 255.) * (N - 1) / 255.;
    const int    i        = std::min(int(position), int(N) - 2);
    return table[i] + float(position - i) * (table[i + 1] - table[i]);
}

} // namespace PhotoCrap::Filters::Photoshop
