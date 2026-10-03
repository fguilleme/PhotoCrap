#include "CurvesFilter.h"
#include "ImageUtils.h"

#include <QColorSpace>
#include <QPointF>
#include <QThreadPool>
#include <QVariantList>
#include <QVector>
#include <QtConcurrent>
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <utility>

namespace PhotoCrap::Filters
{
namespace
{
std::array<int, 256> curveTable(const QVariantMap &_parameters)
{
    QVector<QPointF> points;
    for (const QVariant &entry : _parameters.value("points").toList()) {
        const QVariantMap point = entry.toMap();
        if (point.contains("x") && point.contains("y"))
            points.append({qBound(0., point.value("x").toDouble(), 255.),
                           qBound(0., point.value("y").toDouble(), 255.)});
    }
    std::sort(points.begin(), points.end(),
              [](const QPointF &_a, const QPointF &_b) { return _a.x() < _b.x(); });
    QVector<QPointF> unique;
    for (const QPointF &point : std::as_const(points)) {
        if (!unique.isEmpty() && qAbs(unique.last().x() - point.x()) < .5)
            unique.last() = point;
        else
            unique.append(point);
    }

    std::array<int, 256> table{};
    if (unique.size() < 2) {
        const double shadows    = _parameters.value("shadows", 0).toDouble();
        const double midtones   = _parameters.value("midtones", 0).toDouble();
        const double highlights = _parameters.value("highlights", 0).toDouble();
        for (int x = 0; x < 256; ++x) {
            const double t = x / 255.;
            const double delta =
                shadows * std::pow(1 - t, 2) + midtones * 4 * t * (1 - t) + highlights * t * t;
            table[x] = qBound(0, qRound(x + delta), 255);
        }
        return table;
    }

    QVector<double> slopes(unique.size()), delta(unique.size() - 1);
    for (int i = 0; i + 1 < unique.size(); ++i)
        delta[i] =
            (unique[i + 1].y() - unique[i].y()) / qMax(.001, unique[i + 1].x() - unique[i].x());
    slopes[0]     = delta[0];
    slopes.last() = delta.last();
    for (int i = 1; i + 1 < unique.size(); ++i) {
        if (delta[i - 1] * delta[i] <= 0)
            slopes[i] = 0;
        else {
            const double left  = unique[i].x() - unique[i - 1].x();
            const double right = unique[i + 1].x() - unique[i].x();
            slopes[i]          = (left + right) / ((left + 2 * right) / (3 * delta[i - 1]) +
                                                   (2 * left + right) / (3 * delta[i]));
        }
    }
    int segment = 0;
    for (int x = 0; x < 256; ++x) {
        while (segment + 1 < unique.size() - 1 && x > unique[segment + 1].x())
            ++segment;
        if (x <= unique.first().x()) {
            table[x] = qRound(unique.first().y());
            continue;
        }
        if (x >= unique.last().x()) {
            table[x] = qRound(unique.last().y());
            continue;
        }
        const double width = unique[segment + 1].x() - unique[segment].x();
        const double t = (x - unique[segment].x()) / width, t2 = t * t, t3 = t2 * t;
        const double y = (2 * t3 - 3 * t2 + 1) * unique[segment].y() +
                         (t3 - 2 * t2 + t) * width * slopes[segment] +
                         (-2 * t3 + 3 * t2) * unique[segment + 1].y() +
                         (t3 - t2) * width * slopes[segment + 1];
        table[x]       = qBound(0, qRound(y), 255);
    }
    return table;
}
} // namespace

QImage applyCurves(const QImage &_source, const QVariantMap &_parameters)
{
    if (_source.isNull())
        return _source;
    const auto   table = curveTable(_parameters);
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
