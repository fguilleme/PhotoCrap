#include "ExposureFilter.h"
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
QImage applyExposurePixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;
    const double exposure = std::pow(2., _parameters.value("exposure", 0).toDouble());
    const double offset   = _parameters.value("offset", 0).toDouble() * 255.;
    const double gamma    = qMax(.1, _parameters.value("gamma", 1).toDouble());
    const QImage input    = _source.convertToFormat(P::format);
    QImage       result(input.size(), P::format);
    const int    jobCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    const auto expose = [&](double _value) {
        const double mapped =
            std::pow(qMax(0., _value * exposure + offset) / 255., 1 / gamma) * 255.;
        return qBound(0., P::round(mapped), 255.);
    };
    ImageUtils::blockingMap(jobs, [&](int _job) {
        for (int y = input.height() * _job / jobCount; y < input.height() * (_job + 1) / jobCount;
             ++y) {
            const auto *source = reinterpret_cast<const Pixel *>(input.constScanLine(y));
            auto       *output = reinterpret_cast<Pixel *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x)
                output[x] = P::rgba(expose(P::red(source[x])), expose(P::green(source[x])),
                                    expose(P::blue(source[x])), P::alpha(source[x]));
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
} // namespace
QImage applyExposure(const QImage &_source, const QVariantMap &_parameters)
{
    return ImageUtils::is16Bit(_source) ? applyExposurePixels<QRgba64>(_source, _parameters)
                                        : applyExposurePixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
