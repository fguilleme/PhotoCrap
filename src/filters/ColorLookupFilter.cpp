#include "ColorLookupFilter.h"
#include "ImagePixels.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
double filmCurve(double _value)
{
    const double normalized = _value / 255.;
    return (normalized * normalized * (3. - 2. * normalized)) * 255.;
}

void applyStyle(int _preset, double &_red, double &_green, double &_blue)
{
    switch (_preset) {
    case 0: // Ambre doux
        _red   = filmCurve(_red) * 1.04 + 7.;
        _green = filmCurve(_green) * 1.01 + 2.;
        _blue  = filmCurve(_blue) * .91;
        break;
    case 1: // Cinéma chaud
        _red   = filmCurve(_red) * 1.06 + 4.;
        _green = filmCurve(_green) * .99 + 3.;
        _blue  = filmCurve(_blue) * .93 + 8.;
        break;
    case 2: // Bleu arctique
        _red   = filmCurve(_red) * .9;
        _green = filmCurve(_green) * 1.01 + 3.;
        _blue  = filmCurve(_blue) * 1.1 + 7.;
        break;
    case 3: // Mat
        _red   = 24. + _red * .8;
        _green = 25. + _green * .79;
        _blue  = 28. + _blue * .77;
        break;
    case 4: { // Sarcelle et orange
        const double luminance = .2126 * _red + .7152 * _green + .0722 * _blue;
        const double shadow    = 1. - luminance / 255.;
        const double light     = luminance / 255.;
        _red += 22. * light - 12. * shadow;
        _green += 11. * shadow;
        _blue += 24. * shadow - 16. * light;
        break;
    }
    default: // Pellicule passée
        _red   = 18. + filmCurve(_red) * .86 + _green * .04;
        _green = 15. + filmCurve(_green) * .84 + _blue * .04;
        _blue  = 20. + filmCurve(_blue) * .79 + _red * .03;
        break;
    }
}
} // namespace

namespace PhotoCrap::Filters
{
template <typename Pixel>
QImage applyColorLookupPixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;
    const int    preset   = qBound(0, _parameters.value("preset", 0).toInt(), 5);
    const double strength = qBound(0., _parameters.value("strength", 100.).toDouble() / 100., 1.);
    const bool   preserveLuminosity = _parameters.value("preserveLuminosity", false).toBool();
    if (strength <= 0.)
        return _source;

    const QImage input = _source.convertToFormat(P::format);
    QImage       result(input.size(), P::format);
    const int    jobCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    ImageUtils::blockingMap(jobs, [&](int _job) {
        for (int y = input.height() * _job / jobCount; y < input.height() * (_job + 1) / jobCount;
             ++y) {
            const auto *source = reinterpret_cast<const Pixel *>(input.constScanLine(y));
            auto       *output = reinterpret_cast<Pixel *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x) {
                const Pixel  pixel     = source[x];
                const double sourceRed = P::red(pixel), sourceGreen = P::green(pixel),
                             sourceBlue = P::blue(pixel);
                double       red = sourceRed, green = sourceGreen, blue = sourceBlue;
                applyStyle(preset, red, green, blue);
                if (preserveLuminosity) {
                    const double sourceLum =
                        .2126 * sourceRed + .7152 * sourceGreen + .0722 * sourceBlue;
                    const double styledLum = .2126 * red + .7152 * green + .0722 * blue;
                    red += sourceLum - styledLum;
                    green += sourceLum - styledLum;
                    blue += sourceLum - styledLum;
                }
                red   = sourceRed + (red - sourceRed) * strength;
                green = sourceGreen + (green - sourceGreen) * strength;
                blue  = sourceBlue + (blue - sourceBlue) * strength;
                output[x] =
                    P::rgba(qBound(0., P::round(red), 255.), qBound(0., P::round(green), 255.),
                            qBound(0., P::round(blue), 255.), P::alpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
QImage applyColorLookup(const QImage &_source, const QVariantMap &_parameters)
{
    return ImageUtils::is16Bit(_source) ? applyColorLookupPixels<QRgba64>(_source, _parameters)
                                        : applyColorLookupPixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
