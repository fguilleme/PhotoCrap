#include "SelectiveColorFilter.h"
#include "ImagePixels.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace
{
constexpr std::array<const char *, 9> RangeNames{
    "reds", "yellows", "greens", "cyans", "blues", "magentas", "whites", "neutrals", "blacks"};

struct RangeAdjustment {
    double cyanRed{};
    double magentaGreen{};
    double yellowBlue{};
    double black{};
};

std::array<double, 9> rangeWeights(double _red, double _green, double _blue)
{
    const double maximum   = std::max({_red, _green, _blue});
    const double minimum   = std::min({_red, _green, _blue});
    const double luminance = (.2126 * _red + .7152 * _green + .0722 * _blue) / 255.;
    return {std::max(0., _red - std::max(_green, _blue)) / 255.,
            std::max(0., std::min(_red, _green) - _blue) / 255.,
            std::max(0., _green - std::max(_red, _blue)) / 255.,
            std::max(0., std::min(_green, _blue) - _red) / 255.,
            std::max(0., _blue - std::max(_red, _green)) / 255.,
            std::max(0., std::min(_red, _blue) - _green) / 255.,
            std::max(0., (minimum - 128.) / 127.),
            std::max(0., 1. - std::abs(luminance - .5) * 2.),
            std::max(0., (128. - maximum) / 128.)};
}

double adjustedChannel(double _channel, double _delta, bool _relative)
{
    if (!_relative)
        return _channel + _delta;
    const double room = _delta >= 0. ? 255. - _channel : _channel;
    return _channel + _delta * room / 255.;
}
} // namespace

namespace PhotoCrap::Filters
{
QVariantMap defaultSelectiveColorParameters()
{
    QVariantMap result{{QStringLiteral("relative"), true}};
    for (const char *range : RangeNames)
        for (const char *component : {"CyanRed", "MagentaGreen", "YellowBlue", "Black"})
            result.insert(QLatin1String(range) + QLatin1String(component), 0);
    return result;
}

template <typename Pixel>
QImage applySelectiveColorPixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;
    const bool   relative = _parameters.value("relative", true).toBool();
    const bool   legacy   = !_parameters.contains("redsCyanRed");
    const QImage input    = _source.convertToFormat(P::format);
    QImage       result(input.size(), P::format);
    std::array<RangeAdjustment, RangeNames.size()> adjustments{};
    for (size_t range = 0; range < RangeNames.size(); ++range) {
        const QString prefix = QLatin1String(RangeNames[range]);
        adjustments[range]   = {_parameters.value(prefix + "CyanRed", 0).toDouble() * 2.55,
                                _parameters.value(prefix + "MagentaGreen", 0).toDouble() * 2.55,
                                _parameters.value(prefix + "YellowBlue", 0).toDouble() * 2.55,
                                _parameters.value(prefix + "Black", 0).toDouble() * 2.55};
    }
    const RangeAdjustment legacyAdjustment{_parameters.value("cyanRed", 0).toDouble() * 2.55,
                                           _parameters.value("magentaGreen", 0).toDouble() * 2.55,
                                           _parameters.value("yellowBlue", 0).toDouble() * 2.55,
                                           _parameters.value("black", 0).toDouble() * 2.55};
    const int             jobCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    ImageUtils::blockingMap(jobs, [&](int _job) {
        for (int y = input.height() * _job / jobCount; y < input.height() * (_job + 1) / jobCount;
             ++y) {
            const auto *source = reinterpret_cast<const Pixel *>(input.constScanLine(y));
            auto       *output = reinterpret_cast<Pixel *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x) {
                const Pixel pixel = source[x];
                double      red = P::red(pixel), green = P::green(pixel), blue = P::blue(pixel);
                if (legacy) {
                    const double saturation =
                        (std::max({red, green, blue}) - std::min({red, green, blue})) / 255.;
                    red += legacyAdjustment.cyanRed * saturation;
                    green += legacyAdjustment.magentaGreen * saturation;
                    blue += legacyAdjustment.yellowBlue * saturation;
                    red -= legacyAdjustment.black;
                    green -= legacyAdjustment.black;
                    blue -= legacyAdjustment.black;
                } else {
                    const auto weights  = rangeWeights(red, green, blue);
                    double     redDelta = 0., greenDelta = 0., blueDelta = 0.;
                    for (size_t range = 0; range < RangeNames.size(); ++range) {
                        const double           weight     = weights[range];
                        const RangeAdjustment &adjustment = adjustments[range];
                        redDelta += (adjustment.cyanRed - adjustment.black) * weight;
                        greenDelta += (adjustment.magentaGreen - adjustment.black) * weight;
                        blueDelta += (adjustment.yellowBlue - adjustment.black) * weight;
                    }
                    red   = adjustedChannel(red, redDelta, relative);
                    green = adjustedChannel(green, greenDelta, relative);
                    blue  = adjustedChannel(blue, blueDelta, relative);
                }
                output[x] =
                    P::rgba(qBound(0., P::round(red), 255.), qBound(0., P::round(green), 255.),
                            qBound(0., P::round(blue), 255.), P::alpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
QImage applySelectiveColor(const QImage &_source, const QVariantMap &_parameters)
{
    return ImageUtils::is16Bit(_source) ? applySelectiveColorPixels<QRgba64>(_source, _parameters)
                                        : applySelectiveColorPixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
