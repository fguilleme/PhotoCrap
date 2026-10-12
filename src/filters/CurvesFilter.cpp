#include "CurvesFilter.h"
#include "ImagePixels.h"

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
QVector<int> curveTable(const QVariantMap &_parameters, int _maximum = 255)
{
    const double     scale = _maximum / 255.;
    QVector<QPointF> points;
    for (const QVariant &entry : _parameters.value("points").toList()) {
        const QVariantMap point = entry.toMap();
        if (point.contains("x") && point.contains("y"))
            points.append({qBound(0., point.value("x").toDouble(), 255.),
                           qBound(0., point.value("y").toDouble(), 255.)});
    }
    for (auto &point : points)
        point *= scale;
    std::sort(points.begin(), points.end(),
              [](const QPointF &_a, const QPointF &_b) { return _a.x() < _b.x(); });
    QVector<QPointF> unique;
    for (const QPointF &point : std::as_const(points)) {
        if (!unique.isEmpty() && qAbs(unique.last().x() - point.x()) < .5 * scale)
            unique.last() = point;
        else
            unique.append(point);
    }

    QVector<int> table(_maximum + 1);
    if (unique.size() < 2) {
        const double shadows    = _parameters.value("shadows", 0).toDouble();
        const double midtones   = _parameters.value("midtones", 0).toDouble();
        const double highlights = _parameters.value("highlights", 0).toDouble();
        for (int x = 0; x <= _maximum; ++x) {
            const double t = double(x) / _maximum;
            const double delta =
                shadows * std::pow(1 - t, 2) + midtones * 4 * t * (1 - t) + highlights * t * t;
            table[x] = qBound(0, qRound(x + delta * scale), _maximum);
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
    for (int x = 0; x <= _maximum; ++x) {
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
        table[x]       = qBound(0, qRound(y), _maximum);
    }
    return table;
}

QVariantList channelPoints(const QVariantMap &_parameters, int _channel)
{
    QVariantList result{QVariantMap{{"x", 0}, {"y", 0}}, QVariantMap{{"x", 255}, {"y", 255}}};
    const auto   appendSample = [&](const QString &_key, int _output) {
        const QVariantList sample = _parameters.value(_key).toList();
        if (sample.size() == 3)
            result.append(QVariantMap{{"x", sample[_channel].toInt()}, {"y", _output}});
    };
    appendSample(QStringLiteral("blackSample"), 0);
    const QVariantList gray = _parameters.value(QStringLiteral("graySample")).toList();
    if (gray.size() == 3) {
        const int neutral =
            qRound(.2126 * gray[0].toInt() + .7152 * gray[1].toInt() + .0722 * gray[2].toInt());
        result.append(QVariantMap{{"x", gray[_channel].toInt()}, {"y", neutral}});
    }
    appendSample(QStringLiteral("whiteSample"), 255);
    return result;
}
} // namespace

QImage applyCurves(const QImage &_source, const QVariantMap &_parameters)
{
    if (_source.isNull())
        return _source;
    if (ImageUtils::is16Bit(_source)) {
        const auto table = curveTable(_parameters, 65535);
        const auto red   = curveTable({{"points", channelPoints(_parameters, 0)}}, 65535);
        const auto green = curveTable({{"points", channelPoints(_parameters, 1)}}, 65535);
        const auto blue  = curveTable({{"points", channelPoints(_parameters, 2)}}, 65535);
        return ImageUtils::map16(_source, [&](QRgba64 _pixel) {
            return QRgba64::fromRgba64(red[table[_pixel.red()]], green[table[_pixel.green()]],
                                       blue[table[_pixel.blue()]], _pixel.alpha());
        });
    }
    const auto   table      = curveTable(_parameters);
    const auto   redTable   = curveTable({{"points", channelPoints(_parameters, 0)}});
    const auto   greenTable = curveTable({{"points", channelPoints(_parameters, 1)}});
    const auto   blueTable  = curveTable({{"points", channelPoints(_parameters, 2)}});
    const QImage input      = _source.convertToFormat(QImage::Format_ARGB32);
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
            for (int x = 0; x < input.width(); ++x) {
                const int red = table[qRed(source[x])], green = table[qGreen(source[x])],
                          blue = table[qBlue(source[x])];
                output[x] =
                    qRgba(redTable[red], greenTable[green], blueTable[blue], qAlpha(source[x]));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
} // namespace PhotoCrap::Filters
