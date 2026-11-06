#pragma once
#include <QRect>
#include <array>
#include <cmath>

namespace PhotoCrap::Render
{
// A reduced pixel is valid only when its entire sampling area lies in the tile.
inline std::array<QRect, 2> pyramidRegions(const QRect &native, const QSize &document,
                                           const QSize &reduced)
{
    const auto left = int(std::ceil(double(native.x()) * reduced.width() / document.width()));
    const auto right =
        int(std::floor(double(native.x() + native.width()) * reduced.width() / document.width()));
    const auto top    = int(std::ceil(double(native.y()) * reduced.height() / document.height()));
    const auto bottom = int(
        std::floor(double(native.y() + native.height()) * reduced.height() / document.height()));
    return {QRect(left, native.y(), right - left, native.height()),
            QRect(left, top, right - left, bottom - top)};
}
} // namespace PhotoCrap::Render
