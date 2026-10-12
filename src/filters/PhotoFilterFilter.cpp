#include "PhotoFilterFilter.h"
#include "ImagePixels.h"

#include <QColor>
#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <cmath>
#include <numeric>

namespace PhotoCrap::Filters
{
template <typename Pixel>
QImage applyPhotoFilterPixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;

    QColor filter;
    if (_parameters.contains("color"))
        filter = QColor(_parameters.value("color").toString());
    if (!filter.isValid())
        filter.setHslF(std::fmod(_parameters.value("hue", 35.).toDouble() / 360. + 1., 1.), .8,
                       .55);
    const double density = qBound(0., _parameters.value("density", 25.).toDouble() / 100., 1.);
    const bool   preserveLuminosity = _parameters.value("preserveLuminosity", true).toBool();
    const double filterRed = filter.red(), filterGreen = filter.green(), filterBlue = filter.blue();

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
                double       red        = sourceRed * (1. - density) + filterRed * density;
                double       green      = sourceGreen * (1. - density) + filterGreen * density;
                double       blue       = sourceBlue * (1. - density) + filterBlue * density;
                if (preserveLuminosity) {
                    const double sourceLuminosity =
                        .2126 * sourceRed + .7152 * sourceGreen + .0722 * sourceBlue;
                    const double filteredLuminosity = .2126 * red + .7152 * green + .0722 * blue;
                    const double correction         = sourceLuminosity - filteredLuminosity;
                    red += correction;
                    green += correction;
                    blue += correction;
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
QImage applyPhotoFilter(const QImage &_source, const QVariantMap &_parameters)
{
    return ImageUtils::is16Bit(_source) ? applyPhotoFilterPixels<QRgba64>(_source, _parameters)
                                        : applyPhotoFilterPixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
