#include "ChannelMixerFilter.h"
#include "ImagePixels.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <array>
#include <numeric>

namespace PhotoCrap::Filters
{
template <typename Pixel>
QImage applyChannelMixerPixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
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

    const QImage input = _source.convertToFormat(P::format);
    QImage       result(input.size(), P::format);
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
                const Pixel                 pixel = source[x];
                const std::array<double, 3> channels{double(P::red(pixel)), double(P::green(pixel)),
                                                     double(P::blue(pixel))};
                if (monochrome) {
                    const double gray =
                        qBound(0.,
                               P::round(channels[0] * mono[0] + channels[1] * mono[1] +
                                        channels[2] * mono[2] + monoConstant),
                               255.);
                    output[x] = P::rgba(gray, gray, gray, P::alpha(pixel));
                    continue;
                }
                std::array<double, 3> mixed{};
                for (size_t channel = 0; channel < mixed.size(); ++channel)
                    mixed[channel] =
                        qBound(0.,
                               P::round(channels[0] * matrix[channel][0] +
                                        channels[1] * matrix[channel][1] +
                                        channels[2] * matrix[channel][2] + constants[channel]),
                               255.);
                output[x] = P::rgba(mixed[0], mixed[1], mixed[2], P::alpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
QImage applyChannelMixer(const QImage &_source, const QVariantMap &_parameters)
{
    return ImageUtils::is16Bit(_source) ? applyChannelMixerPixels<QRgba64>(_source, _parameters)
                                        : applyChannelMixerPixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
