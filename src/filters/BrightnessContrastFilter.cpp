#include "BrightnessContrastFilter.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <QtConcurrent>
#include <cmath>
#include <numeric>

namespace PhotoCrap::Filters
{
QImage applyBrightnessContrast(const QImage &_source, const QVariantMap &_parameters)
{
    if (_source.isNull())
        return _source;
    const double brightness = _parameters.value("brightness", 0).toDouble() * 2.55;
    const double contrast   = _parameters.value("contrast", 0).toDouble();
    const double factor     = std::pow((100 + contrast) / 100, 2);
    const QImage input      = _source.convertToFormat(QImage::Format_ARGB32);
    QImage       result(input.size(), QImage::Format_ARGB32);

    const int jobCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    QtConcurrent::blockingMap(jobs, [&](int _job) {
        const int firstRow = input.height() * _job / jobCount;
        const int lastRow  = input.height() * (_job + 1) / jobCount;
        for (int y = firstRow; y < lastRow; ++y) {
            const auto *source = reinterpret_cast<const QRgb *>(input.constScanLine(y));
            auto       *output = reinterpret_cast<QRgb *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x) {
                const QRgb pixel  = source[x];
                const auto adjust = [&](int _value) {
                    return qBound(0, qRound((_value - 128) * factor + 128 + brightness), 255);
                };
                output[x] = qRgba(adjust(qRed(pixel)), adjust(qGreen(pixel)), adjust(qBlue(pixel)),
                                  qAlpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
} // namespace PhotoCrap::Filters
