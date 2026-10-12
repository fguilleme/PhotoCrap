#include "GradientMapFilter.h"
#include "ImagePixels.h"

#include <QColor>
#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <array>
#include <numeric>

namespace PhotoCrap::Filters
{
QImage applyGradientMap(const QImage &_source, const QVariantMap &_parameters)
{
    if (_source.isNull())
        return _source;

    const QColor shadow(_parameters.value("shadowColor", QStringLiteral("#101828")).toString());
    const QColor highlight(
        _parameters.value("highlightColor", QStringLiteral("#f5d9a8")).toString());
    if (ImageUtils::is16Bit(_source)) {
        return ImageUtils::map16(_source, [&](QRgba64 _pixel) {
            const double luminance =
                (54. * _pixel.red() + 183. * _pixel.green() + 19. * _pixel.blue()) /
                (256. * 65535.);
            auto channel = [&](double _shadow, double _highlight) {
                return qRound((_shadow + (_highlight - _shadow) * luminance) * 65535.);
            };
            return QRgba64::fromRgba64(channel(shadow.redF(), highlight.redF()),
                                       channel(shadow.greenF(), highlight.greenF()),
                                       channel(shadow.blueF(), highlight.blueF()), _pixel.alpha());
        });
    }
    std::array<QRgb, 256> palette{};
    for (int luminance = 0; luminance < int(palette.size()); ++luminance) {
        const int inverse = 255 - luminance;
        palette[size_t(luminance)] =
            qRgb((shadow.red() * inverse + highlight.red() * luminance + 127) / 255,
                 (shadow.green() * inverse + highlight.green() * luminance + 127) / 255,
                 (shadow.blue() * inverse + highlight.blue() * luminance + 127) / 255);
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
                output[x] =
                    (palette[size_t(luminance)] & 0x00ffffffU) | (uint(qAlpha(pixel)) << 24);
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
} // namespace PhotoCrap::Filters
