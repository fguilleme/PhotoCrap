#include "BlackWhiteFilter.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <QtConcurrent>
#include <numeric>

namespace PhotoCrap::Filters
{
QImage applyBlackWhite(const QImage &_source, const QVariantMap &_parameters)
{
    if (_source.isNull())
        return _source;
    const int    redWeight   = _parameters.value("reds", 40).toInt();
    const int    greenWeight = _parameters.value("greens", 40).toInt();
    const int    blueWeight  = _parameters.value("blues", 20).toInt();
    const int    total       = qMax(1, redWeight + greenWeight + blueWeight);
    const QImage input       = _source.convertToFormat(QImage::Format_ARGB32);
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
                const QRgb pixel = source[x];
                const int  gray  = qBound(0,
                                          (qRed(pixel) * redWeight + qGreen(pixel) * greenWeight +
                                           qBlue(pixel) * blueWeight) /
                                              total,
                                          255);
                output[x]        = qRgba(gray, gray, gray, qAlpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
} // namespace PhotoCrap::Filters
