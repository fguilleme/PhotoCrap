#include "VibranceFilter.h"
#include "ImagePixels.h"
#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <numeric>

namespace PhotoCrap::Filters
{
namespace
{
template <typename Pixel>
QImage applyVibrancePixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;
    const double vibrance   = _parameters.value("vibrance", 0).toDouble() / 100.;
    const double saturation = _parameters.value("saturation", 0).toDouble() / 100.;
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
                const double red = P::red(source[x]), green = P::green(source[x]),
                             blue      = P::blue(source[x]);
                const double maximum   = qMax(red, qMax(green, blue)),
                             minimum   = qMin(red, qMin(green, blue));
                const double luminance = .2126 * red + .7152 * green + .0722 * blue;
                // Legacy Vibrance deliberately allows a negative factor, unlike Color.
                const double factor  = 1 + saturation + vibrance * (1 - (maximum - minimum) / 255.);
                const auto   channel = [&](double _value) {
                    return qBound(0., P::round(luminance + (_value - luminance) * factor), 255.);
                };
                output[x] =
                    P::rgba(channel(red), channel(green), channel(blue), P::alpha(source[x]));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
} // namespace
QImage applyVibrance(const QImage &_source, const QVariantMap &_parameters)
{
    return ImageUtils::is16Bit(_source) ? applyVibrancePixels<QRgba64>(_source, _parameters)
                                        : applyVibrancePixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
