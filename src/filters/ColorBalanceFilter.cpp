#include "ColorBalanceFilter.h"
#include "ImagePixels.h"
#include "PhotoshopColor.h"
#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <algorithm>
#include <array>
#include <numeric>

namespace PhotoCrap::Filters
{
namespace
{
template <typename Pixel>
QImage applyColorBalancePixels(const QImage &_source, const QVariantMap &_parameters)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;
    const QImage input = _source.convertToFormat(P::format);
    QImage       result(input.size(), P::format);
    const double redShift   = P::round(_parameters.value("cyanRed").toDouble() * 2.55);
    const double greenShift = P::round(_parameters.value("magentaGreen").toDouble() * 2.55);
    const double blueShift  = P::round(_parameters.value("yellowBlue").toDouble() * 2.55);
    const bool   tonal      = _parameters.contains("shadowCyanRed");
    const bool   preserve   = _parameters.value("preserveLuminosity").toBool();
    std::array<std::array<double, 3>, 3> shifts{};
    if (tonal) {
        constexpr std::array keys{
            std::array{"shadowCyanRed", "shadowMagentaGreen", "shadowYellowBlue"},
            std::array{"cyanRed", "magentaGreen", "yellowBlue"},
            std::array{"highlightCyanRed", "highlightMagentaGreen", "highlightYellowBlue"}};
        for (size_t t = 0; t < keys.size(); ++t)
            for (size_t c = 0; c < keys[t].size(); ++c)
                shifts[t][c] = _parameters.value(keys[t][c]).toDouble() / 100.;
    }
    const int jobCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    ImageUtils::blockingMap(jobs, [&](int _job) {
        for (int y = input.height() * _job / jobCount; y < input.height() * (_job + 1) / jobCount;
             ++y) {
            const auto *source = reinterpret_cast<const Pixel *>(input.constScanLine(y));
            auto       *output = reinterpret_cast<Pixel *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x) {
                const Pixel pixel = source[x];
                if (tonal) {
                    const double r = P::red(pixel) / 255., g = P::green(pixel) / 255.,
                                 b      = P::blue(pixel) / 255.;
                    const double l      = (std::min({r, g, b}) + std::max({r, g, b})) / 2.;
                    const double dark   = qBound(0., (.5 - l) * 4. + .25, 1.);
                    const double bright = qBound(0., (l - .5) * 4. + .25, 1.);
                    const auto   adjust = [&](double value, int c) {
                        return qBound(0.,
                                        value + .7 * (dark * shifts[0][c] +
                                                    (1. - dark - bright) * shifts[1][c] +
                                                    bright * shifts[2][c]),
                                        1.);
                    };
                    QColor color = QColor::fromRgbF(adjust(r, 0), adjust(g, 1), adjust(b, 2));
                    if (preserve)
                        color.setHslF(color.hslHueF(), color.hslSaturationF(), l);
                    output[x] = P::rgba(color.redF() * 255., color.greenF() * 255.,
                                        color.blueF() * 255., P::alpha(pixel));
                    continue;
                }
                output[x] = P::rgba(qBound(0., P::red(pixel) + redShift, 255.),
                                    qBound(0., P::green(pixel) + greenShift, 255.),
                                    qBound(0., P::blue(pixel) + blueShift, 255.), P::alpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
} // namespace
QImage applyColorBalance(const QImage &_source, const QVariantMap &_parameters)
{
    if (_parameters.value("photoshopAdjustment").toBool())
        return photoshopColorBalance(_source, _parameters);
    return ImageUtils::is16Bit(_source) ? applyColorBalancePixels<QRgba64>(_source, _parameters)
                                        : applyColorBalancePixels<QRgb>(_source, _parameters);
}
} // namespace PhotoCrap::Filters
