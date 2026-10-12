#include "ColorFilter.h"
#include "ImagePixels.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <cmath>
#include <numeric>

namespace PhotoCrap::Filters
{
template <typename Pixel>
QImage applyColorPixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;
    const double temperature = qBound(-1., _parameters.value("temperature").toDouble() / 100., 1.);
    const double tint        = qBound(-1., _parameters.value("tint").toDouble() / 100., 1.);
    const double vibrance    = qBound(-1., _parameters.value("vibrance").toDouble() / 100., 1.);
    const double saturation  = qBound(-1., _parameters.value("saturation").toDouble() / 100., 1.);
    const QImage input       = _source.convertToFormat(P::format);
    QImage       result(input.size(), P::format);
    const int    jobsCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobsCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    ImageUtils::blockingMap(jobs, [&](int _job) {
        for (int y = input.height() * _job / jobsCount; y < input.height() * (_job + 1) / jobsCount;
             ++y) {
            const auto *source = reinterpret_cast<const Pixel *>(input.constScanLine(y));
            auto       *output = reinterpret_cast<Pixel *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x) {
                const Pixel pixel = source[x];
                double      red = P::red(pixel) / 255., green = P::green(pixel) / 255.,
                            blue       = P::blue(pixel) / 255.;
                red                    = qBound(0., red + .18 * temperature + .06 * tint, 1.);
                green                  = qBound(0., green - .12 * tint, 1.);
                blue                   = qBound(0., blue - .18 * temperature + .06 * tint, 1.);
                const double maximum   = qMax(red, qMax(green, blue));
                const double minimum   = qMin(red, qMin(green, blue));
                const double chroma    = maximum - minimum;
                const double luminance = .2126 * red + .7152 * green + .0722 * blue;
                const double factor    = qMax(0., 1. + saturation + vibrance * (1. - chroma));
                red                    = qBound(0., luminance + (red - luminance) * factor, 1.);
                green                  = qBound(0., luminance + (green - luminance) * factor, 1.);
                blue                   = qBound(0., luminance + (blue - luminance) * factor, 1.);
                output[x]              = P::rgba(P::round(red * 255.), P::round(green * 255.),
                                                 P::round(blue * 255.), P::alpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
QImage applyColor(const QImage &_source, const QVariantMap &_parameters)
{
    return ImageUtils::is16Bit(_source) ? applyColorPixels<QRgba64>(_source, _parameters)
                                        : applyColorPixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
