#include "BrightnessContrastFilter.h"
#include "ImagePixels.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <QtConcurrent>
#include <cmath>
#include <numeric>

namespace PhotoCrap::Filters
{
template <typename Pixel>
QImage applyBrightnessContrastPixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;
    const double brightness = _parameters.value("brightness", 0).toDouble() * 2.55;
    const double contrast   = _parameters.value("contrast", 0).toDouble();
    const double factor     = std::pow((100 + contrast) / 100, 2);
    const QImage input      = _source.convertToFormat(P::format);
    QImage       result(input.size(), P::format);

    const int jobCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    ImageUtils::blockingMap(jobs, [&](int _job) {
        const int firstRow = input.height() * _job / jobCount;
        const int lastRow  = input.height() * (_job + 1) / jobCount;
        for (int y = firstRow; y < lastRow; ++y) {
            const auto *source = reinterpret_cast<const Pixel *>(input.constScanLine(y));
            auto       *output = reinterpret_cast<Pixel *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x) {
                const Pixel pixel  = source[x];
                const auto  adjust = [&](double _value) {
                    return qBound(0., P::round((_value - 128) * factor + 128 + brightness), 255.);
                };
                output[x] = P::rgba(adjust(P::red(pixel)), adjust(P::green(pixel)),
                                    adjust(P::blue(pixel)), P::alpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
QImage applyBrightnessContrast(const QImage &_source, const QVariantMap &_parameters)
{
    return ImageUtils::is16Bit(_source)
               ? applyBrightnessContrastPixels<QRgba64>(_source, _parameters)
               : applyBrightnessContrastPixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
