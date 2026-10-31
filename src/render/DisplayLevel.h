#pragma once
#include <QImage>
#include <QRect>

namespace PhotoCrap::Render
{
struct DisplayLevelUpdate {
    QRect  changed;
    qint64 sourcePixels = 0;
    bool   incremental  = false;
};

// Keep a detached reduced image. A known source rectangle can be rescaled
// locally only when its sampling grid is identical to the full Qt reduction.
DisplayLevelUpdate updateDisplayLevel(QImage &_cache, const QImage &_source, const QSize &_size,
                                      const QRect &_changed = {});
} // namespace PhotoCrap::Render
