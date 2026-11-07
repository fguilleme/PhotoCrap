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
    const PixelPlane &sample = _options.sample.isNull() ? _source : _options.sample;
    if (_options.tolerance < 0 || _options.tolerance > 255 || sample.isNull() ||
        (sample.size() != QSize(1, 1) && sample.size() != _options.size) || !sample.freeze())
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
            tile = sample.tileImage(x / side, y / side);
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
    if (_options.sample.isNull() && _options.pattern.isNull() && !_options.compose &&
        _options.tolerance == 0 && target == _options.replacement &&
        qFuzzyCompare(_options.opacity, 1.)) {
        result.valid = true;
        return result;
    }
    const auto matches = [&](int x, int y) {
        if (failed || (selected && !_options.selection.contains(QPoint(x, y))))
            return false;
        const auto &mask = visited[(y / side) * columns + x / side];
        if (!mask.isNull() && mask.constScanLine(y % side)[x % side])
            return false;
        const auto color = colorAt(x, y);
        if (_options.tolerance == 0)
            return color == target;
        const auto a = color.rgba64(), b = target.rgba64();
        if (b.alpha() == 0)
            return a.alpha() == 0;
        const int tolerance = _options.tolerance * 257;
        return a.alpha() > 0 && qAbs(int(a.red()) - b.red()) <= tolerance &&
               qAbs(int(a.green()) - b.green()) <= tolerance &&
               qAbs(int(a.blue()) - b.blue()) <= tolerance;
    };
    const auto markRun = [&](int left, int right, int y) {
        for (int x = left; x <= right;) {
            const int column = x / side, row = y / side;
            auto &    mask = visited[row * columns + column];
            if (mask.isNull()) {
                mask = QImage(qMin(side, _options.size.width() - column * side),
                              qMin(side, _options.size.height() - row * side),
                              QImage::Format_Grayscale8);
                if (mask.isNull()) {
                    failed = true;
                    return;
                }
                mask.fill(0);
                result.visitedBytes += mask.sizeInBytes();
                ++result.visitedTiles;
            }
            const int end = qMin(right + 1, (column + 1) * side);
            std::memset(mask.scanLine(y % side) + x % side, 255, size_t(end - x));
            footprints[row * columns + column] |= QRect(x % side, y % side, end - x, 1);
            x = end;
        }
        result.filledPixels += right - left + 1;
        result.region |= QRect(left, y, right - left + 1, 1);
    };
    if (!_options.contiguous) {
        for (int y = bounds.top(); y <= bounds.bottom() && !failed; ++y) {
            if (cancelled()) {
                result.cancelled = true;
                return result;
            }
            for (int x = bounds.left(); x <= bounds.right();) {
                while (x <= bounds.right() && !matches(x, y))
                    ++x;
                const int first = x;
                while (x <= bounds.right() && matches(x, y))
                    ++x;
                if (first < x)
                    markRun(first, x - 1, y);
            }
            if (_progress)
                _progress->store(50 * (y - bounds.top() + 1) / bounds.height());
        }
    }
    QVector<QPoint> pending;
    if (_options.contiguous)
        pending.append(_seed);
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
        markRun(left, right, seed.y());
        if (failed)
            break;
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
        if (_options.pattern.isNull())
            overlay.fill(_options.replacement);
        else {
            QPainter painter(&overlay);
            painter.setBrushOrigin(-origin);
            painter.fillRect(overlay.rect(), QBrush(_options.pattern));
        }
        QImage coverage;
        if (_options.antialias) {
            coverage = QImage(footprint.size(), QImage::Format_Grayscale8);
            if (coverage.isNull())
                return result;
            const auto marked = [&](int x, int y) {
                if (!bounds.contains(x, y) ||
                    (selected && !_options.selection.contains(QPoint(x, y))))
                    return 255;
                const auto &tile = visited[(y / side) * columns + x / side];
                return tile.isNull() ? 0 : int(tile.constScanLine(y % side)[x % side]);
            };
            for (int y = 0; y < coverage.height(); ++y) {
                auto *row = coverage.scanLine(y);
                for (int x = 0; x < coverage.width(); ++x) {
                    const int px = origin.x() + x, py = origin.y() + y;
                    if (!mask.constScanLine(py % side)[px % side]) {
                        row[x] = 0;
                        continue;
                    }
                    int sum = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            sum += marked(px + dx, py + dy) * (dx == 0 ? 2 : 1) * (dy == 0 ? 2 : 1);
                    row[x] = uchar((sum + 8) / 16);
                }
            }
            SelectionCoverage::attenuate(overlay, coverage, {});
        } else
            SelectionCoverage::attenuate(overlay, mask, footprint.topLeft());
        SelectionCoverage::attenuate(overlay, _options.selectionMask, origin);
        if (_options.compose)
            output = _options.compose(output, overlay, origin).convertToFormat(format);
        else {
            QPainter painter(&output);
            painter.setOpacity(_options.opacity);
            painter.drawImage(0, 0, overlay);
        }
        if (output.isNull())
            return result;
        if (!result.image.writeRegion(origin, output, _options.size))
            return result;
        if (_progress)
            _progress->store(50 + 50 * ++written / qMax(1, result.visitedTiles));
    }
    result.valid = true;
    return result;
}
} // namespace PhotoCrap::Tools
