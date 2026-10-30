#include "ColorBalanceFilter.h"
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
QImage applyColorBalancePixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;
    const QImage input = _source.convertToFormat(P::format);
    QImage       result(input.size(), P::format);
    const double redShift   = P::round(_parameters.value("cyanRed").toDouble() * 2.55);
    const double greenShift = P::round(_parameters.value("magentaGreen").toDouble() * 2.55);
    const double blueShift  = P::round(_parameters.value("yellowBlue").toDouble() * 2.55);
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
                const Pixel pixel = source[x];
                output[x] = P::rgba(qBound(0., P::red(pixel) + redShift, 255.),
                                    qBound(0., P::green(pixel) + greenShift, 255.),
                                    qBound(0., P::blue(pixel) + blueShift, 255.), P::alpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
} // namespace
QImage applyColorBalance(const QImage &_source, const QVariantMap &_parameters)
{
    return ImageUtils::is16Bit(_source) ? applyColorBalancePixels<QRgba64>(_source, _parameters)
                                        : applyColorBalancePixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
