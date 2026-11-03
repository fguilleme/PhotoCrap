#include "ColorFlood.h"
#include "storage/TiledImage.h"

#include <QBitArray>
#include <QCache>
#include <new>
#include <utility>

namespace PhotoCrap::Tools
{
ColorFloodResult colorFlood(const QSize &_size, const QPoint &_seed, int _tolerance,
                            bool _contiguous, const std::function<QImage(const QRect &)> &_read,
                            const std::shared_ptr<std::atomic_bool> &_cancel)
{
    ColorFloodResult result;
    const auto       cancelled = [&] { return _cancel && _cancel->load(); };
    if (cancelled()) {
        result.cancelled = true;
        return result;
    }
    const QRect bounds(QPoint{}, _size);
    if (!_read || !bounds.contains(_seed) || _size.width() > 16384 || _size.height() > 16384 ||
        qint64(_size.width()) * _size.height() > 32LL * 1024 * 1024 || _tolerance < 0 ||
        _tolerance > 255)
        return result;
    try {
        constexpr int side    = TiledImage::TileSize;
        const int     columns = (_size.width() + side - 1) / side;
        const int     rows    = (_size.height() + side - 1) / side;
        // Keep one horizontal band: even a 16K-wide scan fits in 16 MiB.
        // Small connected selections allocate only the tiles they encounter.
        QCache<int, QImage> inputs(columns * side * side * 4);
        QVector<QBitArray>  visited(_contiguous ? columns * rows : 0);
        int                 currentIndex = -1;
        const QImage       *current      = nullptr;
        bool                failed       = false;
        const auto          colorAt      = [&](int _x, int _y) -> QRgb {
            const int index = (_y / side) * columns + _x / side;
            if (index != currentIndex) {
                current = inputs.object(index);
                if (!current) {
                    if (cancelled()) {
                        failed = true;
                        return 0;
                    }
                    const QRect rect = QRect((_x / side) * side, (_y / side) * side, side, side)
                                           .intersected(bounds);
                    QImage      tile = _read(rect).convertToFormat(QImage::Format_ARGB32);
                    if (tile.isNull() || tile.size() != rect.size()) {
                        failed = true;
                        return 0;
                    }
                    const int cost  = int(tile.sizeInBytes());
                    auto     *entry = new QImage(std::move(tile));
                    if (!inputs.insert(index, entry, cost)) {
                        failed = true;
                        return 0;
                    }
                    current = entry;
                    ++result.tileReads;
                    result.peakCachedBytes =
                        qMax(result.peakCachedBytes, qint64(inputs.totalCost()));
                }
                currentIndex = index;
            }
            return reinterpret_cast<const QRgb *>(current->constScanLine(_y % side))[_x % side];
        };
        const QRgb target  = colorAt(_seed.x(), _seed.y());
        const auto matches = [&](int _x, int _y) {
            if (failed)
                return false;
            const QRgb pixel = colorAt(_x, _y);
            return !failed && qAbs(qRed(pixel) - qRed(target)) <= _tolerance &&
                   qAbs(qGreen(pixel) - qGreen(target)) <= _tolerance &&
                   qAbs(qBlue(pixel) - qBlue(target)) <= _tolerance &&
                   qAbs(qAlpha(pixel) - qAlpha(target)) <= _tolerance;
        };
        const auto addRun = [&](int _left, int _right, int _y) {
            result.region |= QRect(_left, _y, _right - _left + 1, 1);
            result.selectedPixels += _right - _left + 1;
        };
        if (!_contiguous) {
            // Scan globally ordered rows so QRegion keeps the same canonical
            // bands as the original implementation. The cache retains a whole
            // horizontal tile band, avoiding source reconversion on every row.
            for (int y = 0; y < _size.height() && !failed; ++y) {
                if (cancelled()) {
                    result.cancelled = true;
                    return result;
                }
                int x = 0;
                while (x < _size.width() && !failed) {
                    while (x < _size.width() && !matches(x, y))
                        ++x;
                    const int first = x;
                    while (x < _size.width() && matches(x, y))
                        ++x;
                    if (first < x)
                        addRun(first, x - 1, y);
                }
            }
        } else {
            const auto seen = [&](int _x, int _y) {
                const auto &tile = visited.at((_y / side) * columns + _x / side);
                return !tile.isEmpty() && tile.testBit((_y % side) * side + _x % side);
            };
            const auto available = [&](int _x, int _y) { return !seen(_x, _y) && matches(_x, _y); };
            QVector<QPoint> pending{_seed};
            while (!pending.isEmpty() && !failed) {
                if (cancelled()) {
                    result.cancelled = true;
                    return result;
                }
                const QPoint seed = pending.takeLast();
                if (!available(seed.x(), seed.y()))
                    continue;
                int left = seed.x(), right = seed.x();
                while (left > 0 && available(left - 1, seed.y()))
                    --left;
                while (right < bounds.right() && available(right + 1, seed.y()))
                    ++right;
                for (int x = left; x <= right;) {
                    auto &tile = visited[(seed.y() / side) * columns + x / side];
                    if (tile.isEmpty()) {
                        tile = QBitArray(side * side);
                        result.visitedBytes += side * side / 8;
                        ++result.visitedTiles;
                    }
                    const int end      = qMin(right + 1, (x / side + 1) * side);
                    const int firstBit = (seed.y() % side) * side + x % side;
                    tile.fill(true, firstBit, firstBit + end - x);
                    x = end;
                }
                addRun(left, right, seed.y());
                for (int y : {seed.y() - 1, seed.y() + 1})
                    if (y >= 0 && y <= bounds.bottom()) {
                        bool run = false;
                        for (int x = left; x <= right && !failed; ++x) {
                            const bool match = available(x, y);
                            if (match && !run)
                                pending.append({x, y});
                            run = match;
                        }
                    }
            }
        }
        result.cancelled = cancelled();
        result.valid     = !failed && !result.cancelled;
    } catch (const std::bad_alloc &) {
        result.valid = false;
    }
    return result;
}
} // namespace PhotoCrap::Tools
