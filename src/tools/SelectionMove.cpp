#include "SelectionMove.h"
#include "ImageUtils.h"
#include "selection/SelectionCoverage.h"
#include <QPainter>

namespace PhotoCrap::Tools
{
bool canMoveSparseLayer(const PixelPlane &_source, const QSize &_size)
{
    if (_source.isNull() || _size.isEmpty() || _size.width() > 16384 || _size.height() > 16384 ||
        qint64(_size.width()) * _size.height() > 32LL * 1024 * 1024 ||
        (_source.format() != QImage::Format_ARGB32_Premultiplied &&
         _source.format() != QImage::Format_RGBA64_Premultiplied))
        return false;
    const QImage first = _source.tileImage(0, 0);
    if (first.isNull() || first.devicePixelRatio() != 1.)
        return false;
    if (_source.size() == QSize(1, 1))
        return first.pixelColor(0, 0).rgba64().alpha() == 0;
    if (_source.size() != _size || !_source.freeze())
        return false;
    const auto &tiles   = _source.tiles();
    const int   columns = (_size.width() + TiledImage::TileSize - 1) / TiledImage::TileSize;
    const int   rows    = (_size.height() + TiledImage::TileSize - 1) / TiledImage::TileSize;
    return tiles.backgroundPixel().pixelColor(0, 0).rgba64().alpha() == 0 &&
           tiles.tileCount() <= qMax(8, columns * rows / 4);
}

SelectionMoveResult moveSparseLayer(const PixelPlane &_source, const QSize &_size,
                                    const QPoint &_offset)
{
    SelectionMoveResult result;
    if (!canMoveSparseLayer(_source, _size))
        return result;
    if (_offset.isNull()) {
        result.image = _source;
        result.valid = true;
        return result;
    }
    QImage transparent = _source.tileImage(0, 0).copy({0, 0, 1, 1});
    if (transparent.isNull())
        return result;
    transparent.fill(Qt::transparent);
    PixelPlane    next(transparent);
    const QRect   canvas(QPoint{}, _size);
    constexpr int side = TiledImage::TileSize;
    const bool    outside =
        qAbs(qint64(_offset.x())) >= _size.width() || qAbs(qint64(_offset.y())) >= _size.height();
    if (_source.size() != QSize(1, 1))
        for (int y = 0; y < _size.height(); y += side)
            for (int x = 0; x < _size.width(); x += side) {
                const QImage pixels = _source.tileImage(x / side, y / side);
                if (pixels.isNull())
                    return result;
                // A missing tile is the shared transparent background pixel.
                if (pixels.size() == QSize(1, 1) && pixels.pixelColor(0, 0).rgba64().alpha() == 0)
                    continue;
                const QRect from = QRect(x, y, pixels.width(), pixels.height());
                const QRect to   = outside ? QRect{} : from.translated(_offset).intersected(canvas);
                result.region    = result.region.united(from).united(to);
                if (!to.isEmpty() && !next.writeRegion(from.topLeft() + _offset, pixels, _size))
                    return result;
            }
    result.image = std::move(next);
    result.valid = true;
    return result;
}

SelectionMoveResult moveSelection(const PixelPlane &_source, const QSize &_size,
                                  const QRect &_bounds, const QRegion &_selection,
                                  const QImage &_coverage, const QPoint &_offset, bool _copy)
{
    SelectionMoveResult result;
    const QRect         canvas(QPoint{}, _size);
    if (_source.isNull() || _bounds.isEmpty() || !canvas.contains(_bounds) ||
        (_source.size() != QSize(1, 1) && _source.size() != _size))
        return result;
    const auto format = _source.format();
    if (format != QImage::Format_ARGB32_Premultiplied &&
        format != QImage::Format_RGBA64_Premultiplied)
        return result;
    QImage source   = _source.region(_bounds);
    QImage selected = ImageUtils::blank(_bounds.size(), format);
    if (source.isNull() || selected.isNull())
        return result;
    const QRegion localSelection = _selection.translated(-_bounds.topLeft());
    QPainter      selectedPainter(&selected);
    selectedPainter.setClipRegion(localSelection);
    selectedPainter.drawImage(QPoint{}, source);
    selectedPainter.end();
    SelectionCoverage::attenuate(selected, _coverage, _bounds.topLeft());

    PixelPlane next = _source;
    if (!_copy) {
        QPainter painter(&source);
        painter.setClipRegion(localSelection);
        if (_coverage.isNull()) {
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.fillRect(source.rect(), Qt::transparent);
        } else {
            QImage removal(_bounds.size(), format);
            if (removal.isNull())
                return result;
            removal.fill(Qt::white);
            SelectionCoverage::attenuate(removal, _coverage, _bounds.topLeft());
            painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
            painter.drawImage(QPoint{}, removal);
        }
        painter.end();
        if (!next.writeRegion(_bounds.topLeft(), source, _size))
            return result;
        result.region = _bounds;
    }
    const QPoint destination       = _bounds.topLeft() + _offset;
    const QRect  destinationBounds = QRect(destination, _bounds.size()).intersected(canvas);
    if (!destinationBounds.isEmpty()) {
        QImage image = next.region(destinationBounds);
        if (image.isNull())
            return result;
        QPainter painter(&image);
        painter.drawImage(destination - destinationBounds.topLeft(), selected);
        painter.end();
        if (!next.writeRegion(destinationBounds.topLeft(), image, _size))
            return result;
        result.region = result.region.united(destinationBounds);
    }
    result.image = std::move(next);
    result.valid = true;
    return result;
}
} // namespace PhotoCrap::Tools
