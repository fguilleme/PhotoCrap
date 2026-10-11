#include "RegionHealing.h"

#include <QPainter>
#include <QtMath>
#include <array>

namespace
{
using Tone = std::array<qreal, 3>;

struct PatchTone {
    QSize         size;
    QRect         bounds;
    QVector<Tone> values;

    Tone at(int _x, int _y) const
    {
        const qreal gx = qBound(0., (_x - bounds.x() + .5) * size.width() / bounds.width() - .5,
                                qreal(size.width() - 1));
        const qreal gy = qBound(0., (_y - bounds.y() + .5) * size.height() / bounds.height() - .5,
                                qreal(size.height() - 1));
        const int   x = int(gx), y = int(gy);
        const int   nx = qMin(x + 1, size.width() - 1), ny = qMin(y + 1, size.height() - 1);
        Tone        result{};
        for (int c = 0; c < 3; ++c) {
            const qreal top    = values[y * size.width() + x][c] * (1 - gx + x) +
                                 values[y * size.width() + nx][c] * (gx - x);
            const qreal bottom = values[ny * size.width() + x][c] * (1 - gx + x) +
                                 values[ny * size.width() + nx][c] * (gx - x);
            result[c]          = top * (1 - gy + y) + bottom * (gy - y);
        }
        return result;
    }
};

// Extend colour differences from the healthy pixels outside the selected area.
// A bounded harmonic field preserves the donor's full-resolution texture while
// avoiding both a full-image filter and a blur that would reintroduce the defect.
PatchTone patchTone(const QImage &_source, const QImage &_reference, const QRegion &_region,
                    const QPoint &_offset, int _diffusion)
{
    const QRect bounds =
        _region.boundingRect().adjusted(-4, -4, 4, 4).intersected(_reference.rect());
    const int     side = 48 - 5 * (_diffusion - 1);
    const QSize   size(qBound(2, bounds.width(), side), qBound(2, bounds.height(), side));
    PatchTone     field{size, bounds, QVector<Tone>(size.width() * size.height())};
    QVector<bool> fixed(field.values.size(), false);
    Tone          mean{};
    int           count = 0;
    for (int gy = 0; gy < size.height(); ++gy)
        for (int gx = 0; gx < size.width(); ++gx) {
            const int   index = gy * size.width() + gx;
            const QRect cell(bounds.x() + gx * bounds.width() / size.width(),
                             bounds.y() + gy * bounds.height() / size.height(),
                             qMax(1, (gx + 1) * bounds.width() / size.width() -
                                         gx * bounds.width() / size.width()),
                             qMax(1, (gy + 1) * bounds.height() / size.height() -
                                         gy * bounds.height() / size.height()));
            // Nine probes also catch the healthy rim in cells spanning a border.
            Tone tone{};
            int  samples = 0;
            for (int py = 0; py < 3; ++py)
                for (int px = 0; px < 3; ++px) {
                    const QPoint p(cell.x() + px * (cell.width() - 1) / 2,
                                   cell.y() + py * (cell.height() - 1) / 2);
                    const QPoint donor = p + _offset;
                    if (_region.contains(p) || !_source.rect().contains(donor))
                        continue;
                    const QRgb a = qUnpremultiply(_reference.pixel(p));
                    const QRgb b = qUnpremultiply(_source.pixel(donor));
                    if (qAlpha(a) == 0 || qAlpha(b) == 0)
                        continue;
                    tone[0] += qRed(a) - qRed(b);
                    tone[1] += qGreen(a) - qGreen(b);
                    tone[2] += qBlue(a) - qBlue(b);
                    ++samples;
                }
            if (samples) {
                fixed[index] = true;
                for (int c = 0; c < 3; ++c) {
                    field.values[index][c] = tone[c] / samples;
                    mean[c] += field.values[index][c];
                }
                ++count;
            }
        }
    for (auto &v : mean)
        v /= qMax(1, count);
    for (qsizetype i = 0; i < field.values.size(); ++i)
        if (!fixed[i])
            field.values[i] = mean;
    for (int iteration = 0; iteration < 180; ++iteration)
        for (int y = 0; y < size.height(); ++y)
            for (int x = 0; x < size.width(); ++x) {
                const int index = y * size.width() + x;
                if (fixed[index])
                    continue;
                const int left   = y * size.width() + qMax(0, x - 1);
                const int right  = y * size.width() + qMin(size.width() - 1, x + 1);
                const int top    = qMax(0, y - 1) * size.width() + x;
                const int bottom = qMin(size.height() - 1, y + 1) * size.width() + x;
                for (int c = 0; c < 3; ++c) {
                    const qreal average = (field.values[left][c] + field.values[right][c] +
                                           field.values[top][c] + field.values[bottom][c]) /
                                          4;
                    field.values[index][c] += 1.4 * (average - field.values[index][c]);
                }
            }
    return field;
}
} // namespace

QImage RegionHealing::blend(const QImage &_source, const QImage &_reference, const QRegion &_region,
                            const QPoint &_offset, int _diffusion, qreal _colorBlend,
                            bool _composite, int _feather,
                            const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (_region.isEmpty())
        return {};
    const QRect     bounds = _region.boundingRect();
    const PatchTone tone   = _colorBlend > 0
                                 ? patchTone(_source, _reference, _region, _offset, _diffusion)
                                 : PatchTone{};
    QImage output = _composite ? _reference.copy(bounds)
                               : QImage(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    if (!_composite)
        output.fill(Qt::transparent);
    for (const QRect &span : _region)
        for (int y = span.top(); y <= span.bottom(); ++y) {
            if (_cancel && _cancel->load())
                return {};
            auto     *line = reinterpret_cast<QRgb *>(output.scanLine(y - bounds.y()));
            const int sy   = y + _offset.y();
            if (sy < 0 || sy >= _source.height())
                continue;
            const auto *source = reinterpret_cast<const QRgb *>(_source.constScanLine(sy));
            for (int x = span.left(); x <= span.right(); ++x) {
                const int sx = x + _offset.x();
                if (sx < 0 || sx >= _source.width())
                    continue;
                const QRgb donor = qUnpremultiply(source[sx]);
                if (qAlpha(donor) == 0)
                    continue;
                const Tone correction = _colorBlend > 0 ? tone.at(x, y) : Tone{};
                qreal      coverage   = 1.;
                for (int distance = 1; distance <= _feather; ++distance)
                    if (!_region.contains(QPoint(x - distance, y)) ||
                        !_region.contains(QPoint(x + distance, y)) ||
                        !_region.contains(QPoint(x, y - distance)) ||
                        !_region.contains(QPoint(x, y + distance))) {
                        coverage = qreal(distance) / (_feather + 1);
                        break;
                    }
                const QRgb corrected = qPremultiply(
                    qRgba(qBound(0, qRound(qRed(donor) + correction[0] * _colorBlend), 255),
                          qBound(0, qRound(qGreen(donor) + correction[1] * _colorBlend), 255),
                          qBound(0, qRound(qBlue(donor) + correction[2] * _colorBlend), 255),
                          qRound(qAlpha(donor) * coverage)));
                const int index = x - bounds.x();
                if (!_composite) {
                    line[index] = corrected;
                    continue;
                }
                const QRgb before  = line[index];
                const int  inverse = 255 - qAlpha(corrected);
                line[index] = qRgba(qRed(corrected) + (qRed(before) * inverse + 127) / 255,
                                    qGreen(corrected) + (qGreen(before) * inverse + 127) / 255,
                                    qBlue(corrected) + (qBlue(before) * inverse + 127) / 255,
                                    qAlpha(corrected) + (qAlpha(before) * inverse + 127) / 255);
            }
        }
    return output;
}
