#include "ToneMappingTable.h"
#include "ColorBalanceFilter.h"
#include "CurvesFilter.h"
#include "LevelsFilter.h"
#include "LightFilter.h"

namespace PhotoCrap::Filters
{
ToneMappingSettings toneMappingSettings(const QString &_type, const QVariantMap &_parameters)
{
    if (_type == QStringLiteral("light"))
        return {ToneMappingMode::Luminance, {}};
    if (_type == QStringLiteral("color")) {
        const auto value = [&](const char *_key) {
            return float(qBound(-1., _parameters.value(QLatin1String(_key)).toDouble() / 100., 1.));
        };
        return {ToneMappingMode::Color,
                {value("temperature"), value("tint"), value("vibrance"), value("saturation")}};
    }
    return {};
}
QImage toneMappingTable(const QString &_type, const QVariantMap &_parameters)
{
    static const QImage ramp = [] {
        QImage image(256, 1, QImage::Format_ARGB32);
        for (int value = 0; value < 256; ++value)
            image.setPixel(value, 0, qRgb(value, value, value));
        return image;
    }();
    // Color uses analytic uniforms; keep one identity table without re-uploading it.
    if (_type == QStringLiteral("color"))
        return ramp;
    if (_type == QStringLiteral("curves"))
        return applyCurves(ramp, _parameters);
    if (_type == QStringLiteral("levels"))
        return applyLevels(ramp, _parameters);
    if (_type == QStringLiteral("light"))
        return applyLight(ramp, _parameters);
    if (_type == QStringLiteral("colorBalance"))
        return applyColorBalance(ramp, _parameters);
    return {};
}
} // namespace PhotoCrap::Filters
