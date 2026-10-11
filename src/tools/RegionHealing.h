#pragma once

#include <QImage>
#include <QRegion>
#include <atomic>
#include <memory>

namespace RegionHealing
{
QImage blend(const QImage &_source, const QImage &_reference, const QRegion &_region,
             const QPoint &_offset, int _diffusion, qreal _colorBlend, bool _composite,
             int _feather = 0, const std::shared_ptr<std::atomic_bool> &_cancel = {});
}
