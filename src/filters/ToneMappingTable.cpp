#include "ToneMappingTable.h"
#include "BlackWhiteFilter.h"
#include "ColorBalanceFilter.h"
#include "CurvesFilter.h"
#include "GradientMapFilter.h"
#include "LevelsFilter.h"
#include "LightFilter.h"
#include "PhotoFilterFilter.h"
#include "PosterizeFilter.h"
#include "ThresholdFilter.h"

namespace PhotoCrap::Filters
{
ToneMappingSettings toneMappingSettings(const QString &_type, const QVariantMap &_parameters)
{
    if (_type == QStringLiteral("threshold"))
        return {ToneMappingMode::Threshold, {}};
    if (_type == QStringLiteral("photoFilter")) {
        ToneMappingSettings result;
        result.mode           = ToneMappingMode::Matrix;
        const QColor filter   = photoFilterColor(_parameters);
        const double density  = qBound(0., _parameters.value("density", 25.).toDouble() / 100., 1.);
        const bool   preserve = _parameters.value("preserveLuminosity", true).toBool();
        const std::array<double, 3> weights{.2126, .7152, .0722};
        const std::array<double, 3> color{double(filter.red()), double(filter.green()),
                                          double(filter.blue())};
        const double                filterLuminosity =
            color[0] * weights[0] + color[1] * weights[1] + color[2] * weights[2];
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column)
                result.matrix[row][column] = float((row == column ? 1. - density : 0.) +
                                                   (preserve ? density * weights[column] : 0.));
            result.matrix[row][3] =
                float(density * (color[row] - (preserve ? filterLuminosity : 0.)));
        }
        return result;
    }
    if (_type == QStringLiteral("channelMixer")) {
        ToneMappingSettings result;
        result.mode      = ToneMappingMode::Matrix;
        const auto value = [&](const char *_key, double _fallback = 0.) {
            return _parameters.value(QLatin1String(_key), _fallback).toDouble() / 100.;
        };
        if (_parameters.value("monochrome", false).toBool()) {
            result.matrix.fill({float(value("monoRed", 40)), float(value("monoGreen", 40)),
                                float(value("monoBlue", 20)), float(value("monoConstant") * 255.)});
        } else {
            result.matrix = {
                QVector4D{float(value("redRed", 100)), float(value("redGreen")),
                          float(value("redBlue")), float(value("redConstant") * 255.)},
                QVector4D{float(value("greenRed")), float(value("greenGreen", 100)),
                          float(value("greenBlue")), float(value("greenConstant") * 255.)},
                QVector4D{float(value("blueRed")), float(value("blueGreen")),
                          float(value("blueBlue", 100)), float(value("blueConstant") * 255.)}};
        }
        return result;
    }
    if (_type == QStringLiteral("light"))
        return {ToneMappingMode::Luminance, {}};
    if (_type == QStringLiteral("gradientMap"))
        return {ToneMappingMode::GradientMap, {}};
    if (_type == QStringLiteral("blackWhite")) {
        const auto adjustment = [&](const char *_key, double _default) {
            return float((_parameters.value(QLatin1String(_key), _default).toDouble() - _default) /
                         100.);
        };
        return {ToneMappingMode::BlackWhite,
                {},
                {adjustment("reds", 40), adjustment("yellows", 60), adjustment("greens", 40),
                 adjustment("cyans", 60)},
                {adjustment("blues", 20), adjustment("magentas", 80), 0, 0}};
    }
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
    // Analytic shaders share an identity texture that is never re-uploaded during edits.
    if (_type == QStringLiteral("color") || _type == QStringLiteral("photoFilter") ||
        _type == QStringLiteral("channelMixer"))
        return ramp;
    if (_type == QStringLiteral("curves"))
        return applyCurves(ramp, _parameters);
    if (_type == QStringLiteral("levels"))
        return applyLevels(ramp, _parameters);
    if (_type == QStringLiteral("light"))
        return applyLight(ramp, _parameters);
    if (_type == QStringLiteral("colorBalance"))
        return applyColorBalance(ramp, _parameters);
    if (_type == QStringLiteral("gradientMap"))
        return applyGradientMap(ramp, _parameters);
    if (_type == QStringLiteral("blackWhite"))
        return applyBlackWhite(ramp, _parameters);
    if (_type == QStringLiteral("posterize"))
        return applyPosterize(ramp, _parameters);
    if (_type == QStringLiteral("threshold"))
        return applyThreshold(ramp, _parameters);
    return {};
}
} // namespace PhotoCrap::Filters
