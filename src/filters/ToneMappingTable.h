#pragma once
#include <QImage>
#include <QVariantMap>
#include <QVector4D>
#include <array>

namespace PhotoCrap::Filters
{
// Values are shared with the shader's mode uniform.
enum class ToneMappingMode : qint32 {
    Channels    = 0,
    Luminance   = 1,
    Color       = 2,
    GradientMap = 3,
    BlackWhite  = 4,
    Matrix      = 5,
    Threshold   = 6
};
struct ToneMappingSettings {
    ToneMappingMode          mode = ToneMappingMode::Channels;
    QVector4D                color{};
    QVector4D                families0{}, families1{};
    std::array<QVector4D, 3> matrix{};
    bool                     operator==(const ToneMappingSettings &) const = default;
};
ToneMappingSettings toneMappingSettings(const QString &_type, const QVariantMap &_parameters);
// CPU reference lookup: RGB channels for Curves/Levels/Balance, luminance for Light,
// palettes for BlackWhite/GradientMap, and an identity table for analytic shaders.
QImage toneMappingTable(const QString &_type, const QVariantMap &_parameters);
} // namespace PhotoCrap::Filters
