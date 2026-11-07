#pragma once
#include "storage/PixelPlane.h"
#include <QRegion>
#include <functional>

namespace PhotoCrap::Tools
{
struct FloodFillOptions {
    QSize          size;
    PixelPlane     sample;
    QImage         pattern;
    int            tolerance  = 0;
    bool           contiguous = true, antialias = false;
    std::function<QImage(const QImage &, const QImage &, const QPoint &)> compose;
    QRegion        selection;
    QImage         selectionMask;
    QColor         replacement;
    qreal          opacity       = 1.;
    QImage::Format overlayFormat = QImage::Format_ARGB32_Premultiplied;
};
struct FloodFillResult {
    PixelPlane image;
    QRect      region;
    qint64     visitedBytes = 0, filledPixels = 0;
    int        visitedTiles = 0;
    bool       valid = false, cancelled = false;
};
FloodFillResult floodFill(const PixelPlane &_source, const QPoint &_seed,
                          const FloodFillOptions                  &_options,
                          const std::shared_ptr<std::atomic_bool> &_cancel   = {},
                          const std::shared_ptr<std::atomic_int>  &_progress = {});
} // namespace PhotoCrap::Tools
