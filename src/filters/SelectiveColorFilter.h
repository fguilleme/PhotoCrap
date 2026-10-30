#pragma once

#include <QImage>
#include <QVariantMap>
#include <array>

namespace PhotoCrap::Filters
{
inline constexpr std::array<const char *, 9> SelectiveColorRangeNames{
    "reds", "yellows", "greens", "cyans", "blues", "magentas", "whites", "neutrals", "blacks"};
QVariantMap defaultSelectiveColorParameters();
QImage      applySelectiveColor(const QImage &_source, const QVariantMap &_parameters);
} // namespace PhotoCrap::Filters
