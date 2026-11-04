#pragma once
#include "storage/PixelPlane.h"
#include <QRegion>

namespace PhotoCrap::Tools
{
struct SelectionMoveResult {
    PixelPlane image;
    QRect      region;
    bool       valid = false;
};
// Source and destination are prepared separately so a distant drop does not
// allocate the intervening rectangle. The source snapshot remains immutable.
SelectionMoveResult moveSelection(const PixelPlane &_source, const QSize &_size,
                                  const QRect &_bounds, const QRegion &_selection,
                                  const QImage &_coverage, const QPoint &_offset, bool _copy);
// Whole-layer translation stays tiled for dense photos and uniform backgrounds.
bool                canMoveLayerTiled(const PixelPlane &_source, const QSize &_size);
SelectionMoveResult moveLayerTiled(const PixelPlane &_source, const QSize &_size,
                                   const QPoint &_offset);
} // namespace PhotoCrap::Tools
