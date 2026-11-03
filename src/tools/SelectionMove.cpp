#include "SelectionMove.h"
#include "ImageUtils.h"
#include "selection/SelectionCoverage.h"
#include <QPainter>

namespace PhotoCrap::Tools
{
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
