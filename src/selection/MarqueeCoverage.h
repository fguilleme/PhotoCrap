#pragma once
#include "SelectionCoverage.h"
#include <QPainterPath>

namespace MarqueeCoverage
{
SelectionCoverage::Result create(const QRegion &_region, const QPainterPath &_path,
                                 const QSize &_size, bool _antialias, int _feather,
                                 const QRegion &_previous, const QImage &_previousMask, int _mode,
                                 const std::shared_ptr<std::atomic_bool> &_cancel);
}
