#include "FloodFill.h"
#include "ImageUtils.h"
#include "selection/SelectionCoverage.h"
#include <QPainter>
#include <cmath>
#include <cstring>

namespace PhotoCrap::Tools
{
FloodFillResult floodFill(const PixelPlane &_source, const QPoint &_seed,
                          const FloodFillOptions                  &_options,
                          const std::shared_ptr<std::atomic_bool> &_cancel,
                          const std::shared_ptr<std::atomic_int>  &_progress)
{
    FloodFillResult result;
    if (_cancel && _cancel->load()) {
        result.cancelled = true;
        return result;
    }
    const auto format = _source.format();
    if (_options.size.width() <= 0 || _options.size.height() <= 0 ||
        _options.size.width() > 16384 || _options.size.height() > 16384 ||
        qint64(_options.size.width()) * _options.size.height() > 32LL * 1024 * 1024 ||
        (format != QImage::Format_ARGB32_Premultiplied &&
         format != QImage::Format_RGBA64_Premultiplied && format != QImage::Format_Grayscale8) ||
        (_options.overlayFormat != QImage::Format_ARGB32_Premultiplied &&
         _options.overlayFormat != QImage::Format_RGBA64_Premultiplied) ||
        !_options.replacement.isValid() || !std::isfinite(_options.opacity) ||
        _options.opacity < 0 || _options.opacity > 1)
        return result;
    const QRect documentRect(QPoint{}, _options.size);
    const bool  selected = !_options.selection.isEmpty();
    const QRect bounds =
        selected ? _options.selection.boundingRect().intersected(documentRect) : documentRect;
    if (_source.isNull() || !documentRect.isValid() ||
        (_source.size() != QSize(1, 1) && _source.size() != _options.size) || !_source.freeze())
        return result;
    result.image = _source;
    if (!bounds.contains(_seed) || (selected && !_options.selection.contains(_seed))) {
        result.valid = true;
        return result;
    }
    const auto      cancelled = [&] { return _cancel && _cancel->load(); };
    constexpr int   side      = TiledImage::TileSize;
    const int       columns   = (_options.size.width() + side - 1) / side;
    const int       rows      = (_options.size.height() + side - 1) / side;
    QVector<QImage> inputs(columns * rows), visited(columns * rows);
    QVector<QRect>  footprints(columns * rows);
    bool            failed  = false;
    const auto      colorAt = [&](int x, int y) {
        auto &tile = inputs[(y / side) * columns + x / side];
        if (tile.isNull())
            tile = _source.tileImage(x / side, y / side);
        if (tile.isNull()) {
            failed = true;
            return QColor{};
        }
        return tile.size() == QSize(1, 1) ? tile.pixelColor(0, 0)
                                          : tile.pixelColor(x % side, y % side);
    };
    const QColor target = colorAt(_seed.x(), _seed.y());
    if (failed)
        return result;
    if (target == _options.replacement && qFuzzyCompare(_options.opacity, 1.)) {
        result.valid = true;
        return result;
    }
    const auto matches = [&](int x, int y) {
        if (failed || (selected && !_options.selection.contains(QPoint(x, y))))
            return false;
        const auto &mask = visited[(y / side) * columns + x / side];
        return (mask.isNull() || !mask.constScanLine(y % side)[x % side]) &&
               colorAt(x, y) == target;
    };
    QVector<QPoint> pending{_seed};
    while (!pending.isEmpty() && !failed) {
        if (cancelled()) {
            result.cancelled = true;
            return result;
        }
        const auto seed = pending.takeLast();
        if (!matches(seed.x(), seed.y()))
            continue;
        int left = seed.x(), right = seed.x();
        while (left > bounds.left() && matches(left - 1, seed.y()))
            --left;
        while (right < bounds.right() && matches(right + 1, seed.y()))
            ++right;
        for (int x = left; x <= right;) {
            const int column = x / side, row = seed.y() / side;
            auto     &mask = visited[row * columns + column];
            if (mask.isNull()) {
                mask = QImage(qMin(side, _options.size.width() - column * side),
                              qMin(side, _options.size.height() - row * side),
                              QImage::Format_Grayscale8);
                if (mask.isNull()) {
                    failed = true;
                    break;
                }
                mask.fill(0);
                result.visitedBytes += mask.sizeInBytes();
                ++result.visitedTiles;
            }
            const int end = qMin(right + 1, (column + 1) * side);
            std::memset(mask.scanLine(seed.y() % side) + x % side, 255, size_t(end - x));
            footprints[row * columns + column] |= QRect(x % side, seed.y() % side, end - x, 1);
            x = end;
        }
        if (failed)
            break;
        result.filledPixels += right - left + 1;
        result.region |= QRect(left, seed.y(), right - left + 1, 1);
        if (_progress)
            _progress->store(
                int(50 * result.filledPixels / (qint64(bounds.width()) * bounds.height())));
        for (int y : {seed.y() - 1, seed.y() + 1})
            if (y >= bounds.top() && y <= bounds.bottom()) {
                bool run = false;
                for (int x = left; x <= right; ++x) {
                    const bool match = matches(x, y);
                    if (match && !run)
                        pending.append({x, y});
                    run = match;
                }
            }
    }
    if (failed)
        return result;
    int written = 0;
    for (int index = 0; index < visited.size(); ++index) {
        const auto &mask = visited[index];
        if (mask.isNull())
            continue;
        if (cancelled()) {
            result.cancelled = true;
            return result;
        }
        const QRect  footprint = footprints[index];
        const QPoint origin =
            QPoint((index % columns) * side, (index / columns) * side) + footprint.topLeft();
        const QRect tileRect(origin, footprint.size());
        QImage      overlay = ImageUtils::blank(footprint.size(), _options.overlayFormat);
        QImage      output  = _source.region(tileRect);
        if (overlay.isNull() || output.isNull())
            return result;
        overlay.fill(_options.replacement);
        SelectionCoverage::attenuate(overlay, mask, footprint.topLeft());
        SelectionCoverage::attenuate(overlay, _options.selectionMask, origin);
        {
            QPainter painter(&output);
            painter.setOpacity(_options.opacity);
            painter.drawImage(0, 0, overlay);
        }
        if (!result.image.writeRegion(origin, output, _options.size))
            return result;
        if (_progress)
            _progress->store(50 + 50 * ++written / qMax(1, result.visitedTiles));
    }
    result.valid = true;
    return result;
}
} // namespace PhotoCrap::Tools
