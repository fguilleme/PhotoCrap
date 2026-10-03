#include "LevelsFilter.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <QtConcurrent>
#include <array>
#include <cmath>
#include <numeric>

namespace PhotoCrap::Filters
{
QImage applyLevels(const QImage &_source, const QVariantMap &_parameters)
{
    if (_source.isNull())
        return _source;
    const double inputBlack = qBound(0., _parameters.value("inputBlack", 0).toDouble(), 254.);
    const double inputWhite =
        qBound(inputBlack + 1., _parameters.value("inputWhite", 255).toDouble(), 255.);
    const double gamma       = qBound(.1, _parameters.value("gamma", 1.).toDouble(), 3.);
    const double outputBlack = qBound(0., _parameters.value("outputBlack", 0).toDouble(), 255.);
    const double outputWhite =
        qBound(outputBlack, _parameters.value("outputWhite", 255).toDouble(), 255.);
    std::array<int, 256> table{};
    for (int value = 0; value < 256; ++value) {
        const double normalized = qBound(0., (value - inputBlack) / (inputWhite - inputBlack), 1.);
        table[value]            = qBound(
            0, qRound(outputBlack + std::pow(normalized, 1 / gamma) * (outputWhite - outputBlack)),
            255);
    }
    const QImage input = _source.convertToFormat(QImage::Format_ARGB32);
    QImage       result(input.size(), QImage::Format_ARGB32);
    const int    jobCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    QtConcurrent::blockingMap(jobs, [&](int _job) {
        const int firstRow = input.height() * _job / jobCount;
        const int lastRow  = input.height() * (_job + 1) / jobCount;
        for (int y = firstRow; y < lastRow; ++y) {
            const auto source = reinterpret_cast<const QRgb *>(input.constScanLine(y));
            auto       output = reinterpret_cast<QRgb *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x)
                output[x] = qRgba(table[qRed(source[x])], table[qGreen(source[x])],
                                  table[qBlue(source[x])], qAlpha(source[x]));
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
} // namespace PhotoCrap::Filters
