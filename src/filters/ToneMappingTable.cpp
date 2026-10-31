#include "ToneMappingTable.h"
#include "BlackWhiteFilter.h"
#include "BlendMath.h"
#include "BrightnessContrastFilter.h"
#include "ColorBalanceFilter.h"
#include "CurvesFilter.h"
#include "ExposureFilter.h"
#include "GradientMapFilter.h"
#include "InvertFilter.h"
#include "LevelsFilter.h"
#include "LightFilter.h"
#include "PhotoFilterFilter.h"
#include "PosterizeFilter.h"
#include "SelectiveColorFilter.h"
#include "ThresholdFilter.h"
#include <QColorSpace>
#include <cstring>

namespace PhotoCrap::Filters
{
const QList<int> &gpuToneBlendModes()
{
    // Universally supported RGB modes; HSL eligibility is checked separately.
    static const QList<int> modes{0, 2, 3, 5, 7, 8, 10, 12, 14, 16, 17, 19, 20, 21};
    return modes;
}
const QList<int> &gpuToneHslBlendModes()
{
    static const QList<int> modes{23, 24, 25, 26};
    return modes;
}
const QList<int> &gpuToneRatioBlendModes()
{
    static const QList<int> modes{4, 9, 22};
    return modes;
}
const QList<int> &gpuToneContrastBlendModes()
{
    static const QList<int> modes{13, 15};
    return modes;
}
const QList<int> &gpuToneColorSelectionBlendModes()
{
    static const QList<int> modes{6, 11};
    return modes;
}
const QList<int> &gpuToneHardMixBlendModes()
{
    static const QList<int> modes{18};
    return modes;
}
const QList<int> &gpuToneConditionalBlendModes()
{
    static const QList<int> modes = [] {
        auto result = gpuToneHslBlendModes();
        result.append(gpuToneRatioBlendModes());
        result.append(gpuToneContrastBlendModes());
        result.append(gpuToneColorSelectionBlendModes());
        result.append(gpuToneHardMixBlendModes());
        return result;
    }();
    return modes;
}
bool gpuToneBlendSupported(const QString &_type, int _blend, bool _fullCoverage)
{
    if (_blend == 1)
        return _fullCoverage;
    if (gpuToneBlendModes().contains(_blend))
        return true;
    // Hue, near-zero divisions and whole-color decisions amplify one-byte errors.
    // Require exact inputs, without visible mask premultiplication.
    static const QStringList exactTypes{QStringLiteral("curves"),
                                        QStringLiteral("levels"),
                                        QStringLiteral("brightnessContrast"),
                                        QStringLiteral("exposure"),
                                        QStringLiteral("colorBalance"),
                                        QStringLiteral("posterize"),
                                        QStringLiteral("light"),
                                        QStringLiteral("gradientMap"),
                                        QStringLiteral("threshold"),
                                        QStringLiteral("invert")};
    return _fullCoverage && gpuToneConditionalBlendModes().contains(_blend) &&
           exactTypes.contains(_type);
}
const QStringList &gpuTonePreviewTypes()
{
    static const QStringList types{QStringLiteral("levels"),
                                   QStringLiteral("light"),
                                   QStringLiteral("color"),
                                   QStringLiteral("colorBalance"),
                                   QStringLiteral("blackWhite"),
                                   QStringLiteral("gradientMap"),
                                   QStringLiteral("photoFilter"),
                                   QStringLiteral("channelMixer"),
                                   QStringLiteral("posterize"),
                                   QStringLiteral("threshold"),
                                   QStringLiteral("colorLookup"),
                                   QStringLiteral("selectiveColor"),
                                   QStringLiteral("brightnessContrast"),
                                   QStringLiteral("exposure"),
                                   QStringLiteral("vibrance"),
                                   QStringLiteral("hueSaturation"),
                                   QStringLiteral("invert"),
                                   QStringLiteral("curves")};
    return types;
}
ToneMappingSettings toneMappingSettings(const QString &_type, const QVariantMap &_parameters)
{
    if (_type == QStringLiteral("hueSaturation"))
        return {ToneMappingMode::HueSaturation,
                {float(_parameters.value("hue", 0).toDouble() / 360.),
                 float(1 + _parameters.value("saturation", 0).toDouble() / 100.),
                 float(_parameters.value("lightness", 0).toDouble() / 100.), 0.f}};
    if (_type == QStringLiteral("vibrance"))
        return {ToneMappingMode::Vibrance,
                {float(_parameters.value("vibrance", 0).toDouble() / 100.),
                 float(_parameters.value("saturation", 0).toDouble() / 100.), 0.f, 0.f}};
    if (_type == QStringLiteral("colorLookup"))
        return {ToneMappingMode::ColorLookup,
                {float(qBound(0, _parameters.value("preset", 0).toInt(), 5)),
                 float(qBound(0., _parameters.value("strength", 100.).toDouble() / 100., 1.)),
                 _parameters.value("preserveLuminosity", false).toBool() ? 1.f : 0.f, 0.f}};
    if (_type == QStringLiteral("selectiveColor")) {
        ToneMappingSettings result;
        result.mode = _parameters.value("relative", true).toBool()
                          ? ToneMappingMode::SelectiveColorRelative
                          : ToneMappingMode::SelectiveColorAbsolute;
        for (size_t range = 0; range < SelectiveColorRangeNames.size(); ++range) {
            const QString prefix    = QLatin1String(SelectiveColorRangeNames[range]);
            const double  black     = _parameters.value(prefix + "Black", 0).toDouble() * 2.55;
            result.selective[range] = {
                float(_parameters.value(prefix + "CyanRed", 0).toDouble() * 2.55 - black),
                float(_parameters.value(prefix + "MagentaGreen", 0).toDouble() * 2.55 - black),
                float(_parameters.value(prefix + "YellowBlue", 0).toDouble() * 2.55 - black), 0.f};
        }
        return result;
    }
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
namespace
{
QImage withHardMixBoundary(const QImage &_row)
{
    if (_row.isNull())
        return {};
    static const QImage boundary = [] {
        QImage row(256, 1, QImage::Format_ARGB32);
        for (int source = 0; source < 256; ++source) {
            const int high = blendChannel((255 - source) / 255., source / 255., 18) >= .5 ? 255 : 0;
            row.setPixel(source, 0, qRgb(high, high, high));
        }
        return row;
    }();
    const QImage color = _row.convertToFormat(QImage::Format_ARGB32);
    QImage       table(256, 2, QImage::Format_ARGB32);
    std::memcpy(table.scanLine(0), color.constScanLine(0), 256 * 4);
    std::memcpy(table.scanLine(1), boundary.constScanLine(0), 256 * 4);
    table.setColorSpace(color.colorSpace());
    return table;
}
} // namespace
QImage toneMappingTable(const QString &_type, const QVariantMap &_parameters)
{
    static const QImage ramp = [] {
        QImage image(256, 1, QImage::Format_ARGB32);
        for (int value = 0; value < 256; ++value)
            image.setPixel(value, 0, qRgb(value, value, value));
        return image;
    }();
    if (_type == QStringLiteral("invert")) {
        static const QImage inverted = withHardMixBoundary(applyInvert(ramp, {}));
        return inverted;
    }
    // Analytic shaders share an identity texture that is never re-uploaded during edits.
    if (_type == QStringLiteral("color") || _type == QStringLiteral("photoFilter") ||
        _type == QStringLiteral("channelMixer") || _type == QStringLiteral("colorLookup") ||
        _type == QStringLiteral("selectiveColor") || _type == QStringLiteral("vibrance") ||
        _type == QStringLiteral("hueSaturation")) {
        static const QImage identity = withHardMixBoundary(ramp);
        return identity;
    }
    if (_type == QStringLiteral("curves"))
        return withHardMixBoundary(applyCurves(ramp, _parameters));
    if (_type == QStringLiteral("levels"))
        return withHardMixBoundary(applyLevels(ramp, _parameters));
    if (_type == QStringLiteral("light"))
        return withHardMixBoundary(applyLight(ramp, _parameters));
    if (_type == QStringLiteral("colorBalance"))
        return withHardMixBoundary(applyColorBalance(ramp, _parameters));
    if (_type == QStringLiteral("gradientMap"))
        return withHardMixBoundary(applyGradientMap(ramp, _parameters));
    if (_type == QStringLiteral("blackWhite"))
        return withHardMixBoundary(applyBlackWhite(ramp, _parameters));
    if (_type == QStringLiteral("posterize"))
        return withHardMixBoundary(applyPosterize(ramp, _parameters));
    if (_type == QStringLiteral("threshold"))
        return withHardMixBoundary(applyThreshold(ramp, _parameters));
    if (_type == QStringLiteral("brightnessContrast"))
        return withHardMixBoundary(applyBrightnessContrast(ramp, _parameters));
    if (_type == QStringLiteral("exposure"))
        return withHardMixBoundary(applyExposure(ramp, _parameters));
    return {};
}
} // namespace PhotoCrap::Filters
