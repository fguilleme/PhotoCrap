#include "SelectionMove.h"
#include "ImageUtils.h"
#include "selection/SelectionCoverage.h"
#include <QPainter>
#include <cstring>

namespace PhotoCrap::Tools
{
bool canMoveLayerTiled(const PixelPlane &_source, const QSize &_size)
{
    if (_source.isNull() || _size.isEmpty() || _size.width() > 16384 || _size.height() > 16384 ||
        qint64(_size.width()) * _size.height() > 32LL * 1024 * 1024 ||
        (_source.format() != QImage::Format_ARGB32_Premultiplied &&
         _source.format() != QImage::Format_RGBA64_Premultiplied))
        return false;
    const QImage first = _source.tileImage(0, 0);
    if (first.isNull() || first.devicePixelRatio() != 1.)
        return false;
    return _source.size() == QSize(1, 1) || (_source.size() == _size && _source.freeze());
}

SelectionMoveResult moveLayerTiled(const PixelPlane &_source, const QSize &_size,
                                   const QPoint &_offset)
{
    SelectionMoveResult result;
    if (!canMoveLayerTiled(_source, _size))
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
    const QRect   canvas(QPoint{}, _size);
    constexpr int side = TiledImage::TileSize;
    const bool    outside =
        qAbs(qint64(_offset.x())) >= _size.width() || qAbs(qint64(_offset.y())) >= _size.height();
    const QRect  covered = outside ? QRect{} : canvas.translated(_offset).intersected(canvas);
    const QImage background =
        _source.size() == QSize(1, 1) ? _source.tileImage(0, 0) : _source.tiles().backgroundPixel();
    const bool retainBackground =
        background.pixelColor(0, 0).rgba64().alpha() > 0 &&
        qint64(covered.width()) * covered.height() * 2 >= qint64(_size.width()) * _size.height();
    PixelPlane next =
        retainBackground ? PixelPlane::uniform(_size, background) : PixelPlane(transparent);
    if (next.isNull())
        return result;
    if (retainBackground) {
        const QRegion uncovered = QRegion(canvas) - QRegion(covered);
        for (const QRect &rect : uncovered)
            for (int y = rect.y(); y <= rect.bottom(); y += side)
                for (int x = rect.x(); x <= rect.right(); x += side) {
                    const QRect part(x, y, qMin(side, rect.right() - x + 1),
                                     qMin(side, rect.bottom() - y + 1));
                    QImage      pixels(part.size(), _source.format());
                    if (pixels.isNull())
                        return result;
                    pixels.fill(Qt::transparent);
                    if (!next.writeRegion(part.topLeft(), pixels, _size))
                        return result;
                }
    }
    for (int y = 0; y < _size.height(); y += side)
        for (int x = 0; x < _size.width(); x += side) {
            const auto tile = _source.tileImage(x / side, y / side);
            if (tile.isNull())
                return result;
            if (tile.size() == QSize(1, 1) && tile.pixelColor(0, 0).rgba64().alpha() == 0)
                continue;
            const QRect from = QRect(x, y, side, side).intersected(canvas);
            const QRect to   = outside ? QRect{} : from.translated(_offset).intersected(canvas);
            result.region    = result.region.united(from).united(to);
            if (to.isEmpty() || (retainBackground && tile.size() == QSize(1, 1) &&
                                 std::memcmp(tile.constBits(), background.constBits(),
                                             size_t(tile.depth() / 8)) == 0))
                continue;
            const auto pixels = _source.region(to.translated(-_offset));
            if (pixels.isNull() || !next.writeRegion(to.topLeft(), pixels, _size))
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
