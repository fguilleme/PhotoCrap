#include "ThresholdFilter.h"
#include "ImagePixels.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <numeric>

namespace PhotoCrap::Filters
{
QImage applyThreshold(const QImage &_source, const QVariantMap &_parameters)
{
    if (_source.isNull())
        return _source;

    const int threshold = qBound(0, _parameters.value("level", 128).toInt(), 255);
    if (ImageUtils::is16Bit(_source)) {
        return ImageUtils::map16(_source, [&](QRgba64 _pixel) {
            const double luminance =
                .2126 * _pixel.red() + .7152 * _pixel.green() + .0722 * _pixel.blue();
            const int value = luminance >= threshold * 257. ? 65535 : 0;
            return QRgba64::fromRgba64(value, value, value, _pixel.alpha());
        });
    }
    const QImage input = _source.convertToFormat(QImage::Format_ARGB32);
    QImage       result(input.size(), QImage::Format_ARGB32);
    const int    jobCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    ImageUtils::blockingMap(jobs, [&](int _job) {
        const int firstRow = input.height() * _job / jobCount;
        const int lastRow  = input.height() * (_job + 1) / jobCount;
        for (int y = firstRow; y < lastRow; ++y) {
            const auto *source = reinterpret_cast<const QRgb *>(input.constScanLine(y));
            auto       *output = reinterpret_cast<QRgb *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x) {
                const QRgb pixel = source[x];
                const int  luminance =
                    (2126 * qRed(pixel) + 7152 * qGreen(pixel) + 722 * qBlue(pixel) + 5000) / 10000;
                const int value = luminance >= threshold ? 255 : 0;
                output[x]       = qRgba(value, value, value, qAlpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
} // namespace PhotoCrap::Filters
