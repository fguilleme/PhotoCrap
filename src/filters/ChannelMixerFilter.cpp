#include "ChannelMixerFilter.h"
#include "ImageUtils.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <array>
#include <numeric>

namespace PhotoCrap::Filters
{
QImage applyChannelMixer(const QImage &_source, const QVariantMap &_parameters)
{
    if (_source.isNull())
        return _source;
    const bool monochrome = _parameters.value("monochrome", false).toBool();
    const auto value      = [&](const char *_key, double _fallback) {
        return _parameters.value(QLatin1String(_key), _fallback).toDouble() / 100.;
    };
    const std::array<double, 3>                mono{value("monoRed", 40), value("monoGreen", 40),
                                                    value("monoBlue", 20)};
    const double                               monoConstant = value("monoConstant", 0) * 255.;
    const std::array<std::array<double, 3>, 3> matrix{
        std::array<double, 3>{value("redRed", 100), value("redGreen", 0), value("redBlue", 0)},
        std::array<double, 3>{value("greenRed", 0), value("greenGreen", 100),
                              value("greenBlue", 0)},
        std::array<double, 3>{value("blueRed", 0), value("blueGreen", 0), value("blueBlue", 100)}};
    const std::array<double, 3> constants{value("redConstant", 0) * 255.,
                                          value("greenConstant", 0) * 255.,
                                          value("blueConstant", 0) * 255.};

    const QImage input = _source.convertToFormat(QImage::Format_ARGB32);
    QImage       result(input.size(), QImage::Format_ARGB32);
    const int    jobCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    ImageUtils::blockingMap(jobs, [&](int _job) {
        for (int y = input.height() * _job / jobCount; y < input.height() * (_job + 1) / jobCount;
             ++y) {
            const auto *source = reinterpret_cast<const QRgb *>(input.constScanLine(y));
            auto       *output = reinterpret_cast<QRgb *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x) {
                const QRgb                  pixel = source[x];
                const std::array<double, 3> channels{double(qRed(pixel)), double(qGreen(pixel)),
                                                     double(qBlue(pixel))};
                if (monochrome) {
                    const int gray = qBound(0,
                                            qRound(channels[0] * mono[0] + channels[1] * mono[1] +
                                                   channels[2] * mono[2] + monoConstant),
                                            255);
                    output[x]      = qRgba(gray, gray, gray, qAlpha(pixel));
                    continue;
                }
                std::array<int, 3> mixed{};
                for (size_t channel = 0; channel < mixed.size(); ++channel)
                    mixed[channel] = qBound(
                        0,
                        qRound(channels[0] * matrix[channel][0] + channels[1] * matrix[channel][1] +
                               channels[2] * matrix[channel][2] + constants[channel]),
                        255);
                output[x] = qRgba(mixed[0], mixed[1], mixed[2], qAlpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
} // namespace PhotoCrap::Filters
