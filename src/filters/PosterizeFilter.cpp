#include "PosterizeFilter.h"
#include "ImageUtils.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <array>
#include <numeric>

namespace PhotoCrap::Filters
{
QImage applyPosterize(const QImage &_source, const QVariantMap &_parameters)
{
    if (_source.isNull())
        return _source;

    const int              levels = qBound(2, _parameters.value("levels", 4).toInt(), 32);
    const double           step   = 255. / (levels - 1);
    std::array<uchar, 256> table{};
    for (int value = 0; value < int(table.size()); ++value)
        table[size_t(value)] = uchar(qBound(0, qRound(qRound(value / step) * step), 255));

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
                output[x]        = qRgba(table[size_t(qRed(pixel))], table[size_t(qGreen(pixel))],
                                         table[size_t(qBlue(pixel))], qAlpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
} // namespace PhotoCrap::Filters
