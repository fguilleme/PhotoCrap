#include "ExposureFilter.h"
#include "ImagePixels.h"
#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <cmath>
#include <numeric>

namespace PhotoCrap::Filters
{
namespace
{
template <typename Pixel>
QImage applyExposurePixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;
    const bool   photoshop = _parameters.value("photoshopExposure").toBool();
    const double gain      = std::exp2(_parameters.value("exposure", 0).toDouble());
    const double offset    = _parameters.value("offset", 0).toDouble();
    const double gamma     = qMax(photoshop ? .01 : .1, _parameters.value("gamma", 1).toDouble());
    constexpr int maximum  = P::wide ? 65535 : 255;

    // Photoshop Exposure decodes RGB before applying gain and offset. Its measured
    // RGB response follows gamma 2.2 as represented by an ICC u8Fixed8 value.
    const double rgbGamma = photoshop ? 563. / 256. : 1.;
    const double exponent = 1. / (rgbGamma * gamma);
    QVector<quint16> table(maximum + 1);
    for (int i = 0; i <= maximum; ++i) {
        const double encoded = double(i) / maximum;
        const double input   = photoshop ? std::pow(encoded, rgbGamma) : encoded;
        const double exposed = qBound(0., input * gain + offset, 1.);
        table[i]             = quint16(qRound(maximum * std::pow(exposed, exponent)));
    }
    const auto *lut = table.constData();
    const QImage input    = _source.convertToFormat(P::format);
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
                const auto pixel = source[x];
                if constexpr (P::wide)
                    output[x] = QRgba64::fromRgba64(lut[pixel.red()], lut[pixel.green()],
                                                    lut[pixel.blue()], pixel.alpha());
                else
                    output[x] = qRgba(lut[qRed(pixel)], lut[qGreen(pixel)], lut[qBlue(pixel)],
                                      qAlpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
} // namespace
QImage applyExposure(const QImage &_source, const QVariantMap &_parameters)
{
    return ImageUtils::is16Bit(_source) ? applyExposurePixels<QRgba64>(_source, _parameters)
                                        : applyExposurePixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
