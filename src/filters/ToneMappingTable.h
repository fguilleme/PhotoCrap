#pragma once
#include <QImage>
#include <QVariantMap>
#include <QVector4D>

namespace PhotoCrap::Filters
{
// Values are shared with the shader's mode uniform.
enum class ToneMappingMode : qint32 { Channels = 0, Luminance = 1, Color = 2 };
struct ToneMappingSettings {
    ToneMappingMode mode = ToneMappingMode::Channels;
    QVector4D       color;
    bool            operator==(const ToneMappingSettings &) const = default;
};
ToneMappingSettings toneMappingSettings(const QString &_type, const QVariantMap &_parameters);
// CPU reference lookup: RGB channels for Curves/Levels/Balance, luminance for Light,
// and a reused identity table for the analytic Color shader.
QImage toneMappingTable(const QString &_type, const QVariantMap &_parameters);
} // namespace PhotoCrap::Filters
