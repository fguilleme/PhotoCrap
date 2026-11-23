#include "ContentFill.h"
#include "HealingPixels.h"
#include "RegionHealing.h"
#include <QtMath>
#include <array>
#include <limits>

namespace
{
using Color = std::array<qreal, 4>;
// Even spacing over the covered pixels also samples a diagonal one-pixel strip.
QVector<QPoint> regionProbes(const QRegion &region, int limit)
{
    qint64 area = 0;
    for (const QRect &span : region)
        area += qint64(span.width()) * span.height();
    const int count = int(qMin<qint64>(area, limit));
    QVector<QPoint> points;
    points.reserve(count);
    qint64 start = 0;
    for (const QRect &span : region) {
        const qint64 end = start + qint64(span.width()) * span.height();
        while (points.size() < count) {
            const qint64 index = (2 * points.size() + 1) * area / (2 * count);
            if (index >= end)
                break;
            const qint64 local = index - start;
            points.append(span.topLeft() + QPoint(int(local % span.width()), int(local / span.width())));
        }
        start = end;
    }
    return points;
}

// Match a bounded set of nearby donor patches against the healthy selection rim.
// The selected object and its destination can never be sampled as replacement
// texture. Pattern residuals, gradients and variance discourage copying a second
// object into the hole. Full-resolution texture is copied only after the search.
bool findDonor(const QImage &_image, const QRegion &_region, const QRegion &_excluded,
               int _structure, QPoint &_offset, const std::shared_ptr<std::atomic_bool> &_cancel)
{
    const QRect     bounds    = _region.boundingRect();
    const QRegion rim = ((_region.translated(4, 0) | _region.translated(-4, 0) |
                          _region.translated(0, 4) | _region.translated(0, -4)) -
                         _excluded - _region) & _image.rect();
    const auto probes = regionProbes(rim, 128);
    const auto interior = regionProbes(_region, 128);
    if (probes.isEmpty())
        return false;
    Color rimMean{}, rimVariance{};
    for (const QPoint &p : probes) {
        const Color color = HealingPixels::color(_image, p);
        for (int c = 0; c < 4; ++c) {
            rimMean[c] += color[c];
            rimVariance[c] += color[c] * color[c];
        }
    }
    qreal variance = 0;
    for (int c = 0; c < 4; ++c) {
        rimMean[c] /= probes.size();
        variance += rimVariance[c] / probes.size() - rimMean[c] * rimMean[c];
    }
    qreal      best     = std::numeric_limits<qreal>::max();
    bool       found    = false;
    const auto consider = [&](const QPoint &_candidate) {
        if (_cancel->load() || _candidate.isNull() ||
            !_image.rect().contains(bounds.translated(_candidate)) ||
            !(_region.translated(_candidate) & _excluded).isEmpty())
            return;
        Color difference{}, square{};
        qreal gradient = 0;
        int   count    = 0;
        for (const QPoint &p : probes) {
            const QPoint donor = p + _candidate;
            if (!_image.rect().contains(donor) || _excluded.contains(donor))
                continue;
            const Color a = HealingPixels::color(_image, p),
                        b = HealingPixels::color(_image, donor);
            for (int c = 0; c < 4; ++c) {
                const qreal delta = a[c] - b[c];
                difference[c] += delta;
                square[c] += delta * delta;
            }
            const QPoint next = p + QPoint(1, 0), donorNext = donor + QPoint(1, 0);
            if (_image.rect().contains(next) && _image.rect().contains(donorNext) &&
                !_region.contains(next) && !_excluded.contains(donorNext)) {
                const Color an = HealingPixels::color(_image, next),
                            bn = HealingPixels::color(_image, donorNext);
                for (int c = 0; c < 3; ++c)
                    gradient += qPow((an[c] - a[c]) - (bn[c] - b[c]), 2);
            }
            ++count;
        }
        if (count < qMax(1, probes.size() / 2))
            return;
        qreal score = gradient / count * _structure / 7.;
        for (int c = 0; c < 4; ++c) {
            const qreal mean = difference[c] / count;
            score += qMax(0., square[c] / count - mean * mean) + mean * mean * .04;
        }
        Color donorMean{}, donorSquare{};
        for (const QPoint &p : interior) {
            const Color b = HealingPixels::color(_image, p + _candidate);
            for (int c = 0; c < 4; ++c) {
                donorMean[c] += b[c];
                donorSquare[c] += b[c] * b[c];
            }
        }
        qreal donorVariance = 0;
        for (int c = 0; c < 4; ++c)
            donorVariance += donorSquare[c] / interior.size() - qPow(donorMean[c] / interior.size(), 2);
        score += .5 * qMax(0., donorVariance - variance * 4);
        score += .00001 *
                 (_candidate.x() * qreal(_candidate.x()) + _candidate.y() * qreal(_candidate.y()));
        if (score < best) {
            best    = score;
            _offset = _candidate;
            found   = true;
        }
    };
    const int radius = qMax(bounds.width(), bounds.height());
    // A long, thin mark can have a clean donor just one brush width away.
    // Searching only at distances proportional to its length can copy an
    // unrelated object from the other side of the image into the stroke.
    qint64 markedPixels = 0;
    for (const QRect &span : _region)
        markedPixels += qint64(span.width()) * span.height();
    const qreal meanWidth = qreal(markedPixels) / radius;
    for (qreal distance = qMax(2., meanWidth); distance < radius * .75 + 8; distance *= 1.5)
        for (int ray = 0; ray < 32; ++ray) {
            const qreal angle = ray * 2 * M_PI / 32;
            consider(QPoint(qRound(qCos(angle) * distance), qRound(qSin(angle) * distance)));
        }
    for (int shell = 0; shell < 6; ++shell)
        for (int ray = 0; ray < 32; ++ray) {
            const qreal angle    = ray * 2 * M_PI / 32;
            const qreal distance = radius * (.75 + shell * .5) + 8;
            consider(QPoint(qRound(qCos(angle) * distance), qRound(qSin(angle) * distance)));
        }
    if (found) {
        const QPoint coarse = _offset;
        for (int dy = -8; dy <= 8; dy += 2)
            for (int dx = -8; dx <= 8; dx += 2)
                consider(coarse + QPoint(dx, dy));
        const QPoint fine = _offset;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                consider(fine + QPoint(dx, dy));
    }
    return found && !_cancel->load();
}

// Irregular or edge-spanning selections may leave no intact donor patch. In
// that case propagate the nearest healthy boundary, using a bounded grid, rather
// than sampling the object itself or scanning the whole document.
QImage boundaryFill(const QImage &_image, const QRegion &_region,
                    const std::shared_ptr<std::atomic_bool> &_cancel)
{
    const QRect bounds = _region.boundingRect();
    const QRect work   = bounds.adjusted(-4, -4, 4, 4).intersected(_image.rect());
    QSize       grid   = work.size();
    grid.scale(512, 512, Qt::KeepAspectRatio);
    grid = grid.boundedTo(work.size());
    QVector<QPoint> seeds(grid.width() * grid.height(), QPoint(-1, -1));
    QVector<int>    queue;
    for (int y = 0; y < grid.height(); ++y)
        for (int x = 0; x < grid.width(); ++x) {
            const QPoint p(work.x() + x * (work.width() - 1) / qMax(1, grid.width() - 1),
                           work.y() + y * (work.height() - 1) / qMax(1, grid.height() - 1));
            if (!_region.contains(p)) {
                const int index = y * grid.width() + x;
                seeds[index]    = p;
                queue.append(index);
            }
        }
    if (queue.isEmpty())
        return {};
    for (qsizetype i = 0; i < queue.size(); ++i) {
        if (_cancel->load())
            return {};
        const int index = queue[i], x = index % grid.width(), y = index / grid.width();
        for (const QPoint &delta : {QPoint(-1, 0), QPoint(1, 0), QPoint(0, -1), QPoint(0, 1)}) {
            const int nx = x + delta.x(), ny = y + delta.y();
            if (nx < 0 || nx >= grid.width() || ny < 0 || ny >= grid.height())
                continue;
            const int next = ny * grid.width() + nx;
            if (seeds[next].x() >= 0)
                continue;
            seeds[next] = seeds[index];
            queue.append(next);
        }
    }
    QImage output(bounds.size(), ImageUtils::workingFormat(_image));
    output.fill(Qt::transparent);
    for (const QRect &span : _region)
        for (int y = span.top(); y <= span.bottom(); ++y) {
            if (_cancel->load())
                return {};
            auto *line = output.scanLine(y - bounds.y());
            for (int x = span.left(); x <= span.right(); ++x) {
                const int gx =
                    qBound(0, (x - work.x()) * grid.width() / work.width(), grid.width() - 1);
                const int gy =
                    qBound(0, (y - work.y()) * grid.height() / work.height(), grid.height() - 1);
                const QPoint seed = seeds[gy * grid.width() + gx];
                if (ImageUtils::is16Bit(_image))
                    reinterpret_cast<QRgba64 *>(line)[x - bounds.x()] =
                        reinterpret_cast<const QRgba64 *>(_image.constScanLine(seed.y()))[seed.x()];
                else
                    reinterpret_cast<QRgb *>(line)[x - bounds.x()] = _image.pixel(seed);
            }
        }
    if (ImageUtils::is16Bit(_image))
        output.setColorSpace(_image.colorSpace());
    return output;
}
} // namespace

QImage ContentFill::reconstruct(const QImage &_image, const QRegion &_region,
                                const QRegion &_excluded, int _structure,
                                const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (_region.isEmpty() || _image.isNull() || _cancel->load())
        return {};
    QPoint donor;
    if (findDonor(_image, _region, _excluded, _structure, donor, _cancel))
        return RegionHealing::blend(_image, _image, _region, donor, 8 - _structure, 1., false, 0,
                                    _cancel, _excluded);
    return _cancel->load() ? QImage{} : boundaryFill(_image, _region, _cancel);
}
