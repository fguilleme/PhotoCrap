#include "LightFilter.h"
#include "ImageUtils.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <array>
#include <cmath>
#include <numeric>

namespace
{
double smoothStep(double _minimum, double _maximum, double _value)
{
    const double position = qBound(0., (_value - _minimum) / (_maximum - _minimum), 1.);
    return position * position * (3. - 2. * position);
}

double shiftTone(double _value, double _amount, double _weight)
{
    return _amount >= 0. ? _value + (1. - _value) * _amount * _weight
                         : _value + _value * _amount * _weight;
}
} // namespace

namespace PhotoCrap::Filters
{
QImage applyLight(const QImage &_source, const QVariantMap &_parameters)
{
    if (_source.isNull())
        return _source;

    const double exposure = qBound(-5., _parameters.value("exposure", 0.).toDouble(), 5.);
    const double contrast = qBound(-1., _parameters.value("contrast", 0.).toDouble() / 100., 1.);
    const double highlights =
        qBound(-1., _parameters.value("highlights", 0.).toDouble() / 100., 1.);
    const double shadows = qBound(-1., _parameters.value("shadows", 0.).toDouble() / 100., 1.);
    const double whites  = qBound(-1., _parameters.value("whites", 0.).toDouble() / 100., 1.);
    const double blacks  = qBound(-1., _parameters.value("blacks", 0.).toDouble() / 100., 1.);

    std::array<uchar, 256> toneCurve{};
    for (int level = 0; level < int(toneCurve.size()); ++level) {
        double       value          = qBound(0., level / 255. * std::exp2(exposure), 1.);
        const double contrastFactor = std::exp2(contrast);
        value                       = qBound(0., .5 + (value - .5) * contrastFactor, 1.);
        value                       = shiftTone(value, shadows, 1. - smoothStep(0., .65, value));
        value                       = shiftTone(value, highlights, smoothStep(.35, 1., value));
        value                       = shiftTone(value, blacks, 1. - smoothStep(0., .35, value));
        value                       = shiftTone(value, whites, smoothStep(.65, 1., value));
        toneCurve[size_t(level)]    = uchar(qBound(0, qRound(value * 255.), 255));
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
                    (54 * qRed(pixel) + 183 * qGreen(pixel) + 19 * qBlue(pixel) + 128) >> 8;
                const int mapped     = toneCurve[size_t(luminance)];
                auto      mapChannel = [&](int _channel) {
                    if (mapped >= luminance) {
                        if (luminance == 255)
                            return mapped;
                        return _channel +
                               (255 - _channel) * (mapped - luminance) / (255 - luminance);
                    }
                    if (luminance == 0)
                        return mapped;
                    return _channel * mapped / luminance;
                };
                output[x] = qRgba(mapChannel(qRed(pixel)), mapChannel(qGreen(pixel)),
                                  mapChannel(qBlue(pixel)), qAlpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
} // namespace PhotoCrap::Filters
