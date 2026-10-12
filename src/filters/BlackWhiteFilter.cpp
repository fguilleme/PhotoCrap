#include "BlackWhiteFilter.h"
#include "ImagePixels.h"

#include <QColor>
#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <QtConcurrent>
#include <array>
#include <cmath>
#include <numeric>

namespace PhotoCrap::Filters
{
QImage applyBlackWhite(const QImage &_source, const QVariantMap &_parameters)
{
    if (_source.isNull())
        return _source;
    const std::array<double, 6> values{
        _parameters.value("reds", 40).toDouble(),   _parameters.value("yellows", 60).toDouble(),
        _parameters.value("greens", 40).toDouble(), _parameters.value("cyans", 60).toDouble(),
        _parameters.value("blues", 20).toDouble(),  _parameters.value("magentas", 80).toDouble()};
    const std::array<double, 6> defaults{40., 60., 40., 60., 20., 80.};
    const bool                  tintEnabled = _parameters.value("tintEnabled", false).toBool();
    QColor tint(_parameters.value("tintColor", QStringLiteral("#d6aa78")).toString());
    if (!tint.isValid())
        tint = QColor(QStringLiteral("#d6aa78"));
    if (ImageUtils::is16Bit(_source)) {
        float hue = 0., saturation = 0., lightness = 0., alpha = 0.;
        tint.getHslF(&hue, &saturation, &lightness, &alpha);
        return ImageUtils::map16(_source, [&](QRgba64 _pixel) {
            const double red = _pixel.red(), green = _pixel.green(), blue = _pixel.blue();
            const double maximum = std::max({red, green, blue}),
                         minimum = std::min({red, green, blue}), chroma = maximum - minimum;
            double       family = 0.;
            if (chroma > 0) {
                if (maximum == red)
                    family = std::fmod((green - blue) / chroma + 6., 6.);
                else if (maximum == green)
                    family = (blue - red) / chroma + 2.;
                else
                    family = (red - green) / chroma + 4.;
            }
            const int    first = int(std::floor(family)) % 6, second = (first + 1) % 6;
            const double fraction   = family - std::floor(family);
            const double adjustment = ((values[first] - defaults[first]) * (1. - fraction) +
                                       (values[second] - defaults[second]) * fraction) /
                                      100.;
            const int    gray       = qBound(
                0, qRound(.2126 * red + .7152 * green + .0722 * blue + adjustment * chroma), 65535);
            if (!tintEnabled)
                return QRgba64::fromRgba64(gray, gray, gray, _pixel.alpha());
            QColor mapped;
            mapped.setHslF(hue, saturation, gray / 65535., _pixel.alpha() / 65535.);
            return mapped.rgba64();
        });
    }
    std::array<QRgb, 256> tintLookup{};
    if (tintEnabled) {
        float hue = 0., saturation = 0., lightness = 0., alpha = 0.;
        tint.getHslF(&hue, &saturation, &lightness, &alpha);
        for (int level = 0; level < 256; ++level) {
            QColor mapped;
            mapped.setHslF(hue, saturation, level / 255., 1.);
            tintLookup[size_t(level)] = mapped.rgba();
        }
    }
    const QImage input = _source.convertToFormat(QImage::Format_ARGB32);
    QImage       result(input.size(), QImage::Format_ARGB32);

    const int jobCount =
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
                const int  red = qRed(pixel), green = qGreen(pixel), blue = qBlue(pixel);
                const int  maximum = qMax(red, qMax(green, blue));
                const int  minimum = qMin(red, qMin(green, blue));
                const int  chroma  = maximum - minimum;
                double     hue     = 0.;
                if (chroma > 0) {
                    if (maximum == red)
                        hue = std::fmod((green - blue) / double(chroma) + 6., 6.);
                    else if (maximum == green)
                        hue = (blue - red) / double(chroma) + 2.;
                    else
                        hue = (red - green) / double(chroma) + 4.;
                }
                const int    first    = int(std::floor(hue)) % 6;
                const int    second   = (first + 1) % 6;
                const double fraction = hue - std::floor(hue);
                const double adjustment =
                    ((values[size_t(first)] - defaults[size_t(first)]) * (1. - fraction) +
                     (values[size_t(second)] - defaults[size_t(second)]) * fraction) /
                    100.;
                const double luminance = .2126 * red + .7152 * green + .0722 * blue;
                const int    gray      = qBound(0, qRound(luminance + adjustment * chroma), 255);
                if (tintEnabled) {
                    const QRgb mapped = tintLookup[size_t(gray)];
                    output[x] = qRgba(qRed(mapped), qGreen(mapped), qBlue(mapped), qAlpha(pixel));
                } else
                    output[x] = qRgba(gray, gray, gray, qAlpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
} // namespace PhotoCrap::Filters
