#pragma once
#include <QImage>
#include <QRegion>
#include <atomic>
#include <memory>

namespace ContentFill
{
// Returns a bounding-rectangle-sized replacement; never samples excluded pixels.
QImage reconstruct(const QImage &_image, const QRegion &_region, const QRegion &_excluded,
                   int _structure, const std::shared_ptr<std::atomic_bool> &_cancel);
} // namespace ContentFill
