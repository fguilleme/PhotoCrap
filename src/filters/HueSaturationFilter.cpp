#include "HueSaturationFilter.h"
#include "ImagePixels.h"
#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <cmath>
#include <numeric>

namespace PhotoCrap::Filters
{
namespace
{
template <typename Pixel>
QImage applyHueSaturationPixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;
    const double hue        = _parameters.value("hue", 0).toDouble() / 360.;
    const double saturation = 1 + _parameters.value("saturation", 0).toDouble() / 100.;
    const double lightness  = _parameters.value("lightness", 0).toDouble() / 100.;
    const bool   photoshop  = _parameters.value("photoshopAdjustment").toBool();
    const bool   colorize   = _parameters.value("colorize").toBool();
    const QImage input      = _source.convertToFormat(P::format);
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
                QColor color = P::color(source[x]);
                float  h, s, l, a;
                color.getHslF(&h, &s, &l, &a);
                // QColor reports -1 for achromatic pixels; rotating that sentinel
                // can produce an invalid HSL color when the requested hue is negative.
                h = std::fmod((colorize ? 0.f : qMax(0.f, h)) + hue + 1., 1.);
                s = qBound(0.,
                           colorize
                               ? saturation - 1.
                               : (photoshop && saturation > 1. ? s / qMax(.001, 2. - saturation)
                                                               : s * saturation),
                           1.);
                if (!photoshop)
                    l = qBound(0., l + lightness, 1.);
                color.setHslF(h, s, l, a);
                const auto channel = [&](double v) {
                    if (photoshop)
                        v = lightness < 0. ? v * (1. + lightness) : v + (1. - v) * lightness;
                    return P::round(v * 255.);
                };
                output[x] = P::rgba(channel(color.redF()), channel(color.greenF()),
                                    channel(color.blueF()), P::alpha(source[x]));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
} // namespace
QImage applyHueSaturation(const QImage &_source, const QVariantMap &_parameters)
{
    return ImageUtils::is16Bit(_source) ? applyHueSaturationPixels<QRgba64>(_source, _parameters)
                                        : applyHueSaturationPixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
