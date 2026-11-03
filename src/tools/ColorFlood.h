#pragma once

#include <QImage>
#include <QRegion>
#include <atomic>
#include <functional>
#include <memory>

namespace PhotoCrap::Tools
{
struct ColorFloodResult {
    QRegion region;
    qint64  selectedPixels = 0, visitedBytes = 0, peakCachedBytes = 0;
    int     visitedTiles = 0, tileReads = 0;
    bool    valid = false, cancelled = false;
};

// Reads native document rectangles lazily. Returned pixels must cover exactly
// the requested rectangle; comparison uses the same straight 8-bit RGBA values
// as QImage::convertToFormat(ARGB32), regardless of native document precision.
ColorFloodResult colorFlood(const QSize &_size, const QPoint &_seed, int _tolerance,
                            bool _contiguous, const std::function<QImage(const QRect &)> &_read,
                            const std::shared_ptr<std::atomic_bool> &_cancel = {});
} // namespace PhotoCrap::Tools
