#pragma once
#include "../storage/PixelPlane.h"
#include <QVector>
#include <atomic>

namespace PhotoCrap::Render
{
struct StackStatistics {
    std::atomic<quint64> uploads{0}, passes{0}, bytes{0};
};
struct StackLayer {
    PixelPlane image, mask;
    float      opacity = 1, density = 1;
    int        blend   = 0;
    bool       clipped = false, reset = false;
};
// Immutable, shallow GUI-thread snapshot. Pixel reads and GPU resources belong
// to the scene graph thread; no live Document is accessed there.
struct LayerStack {
    QSize               size;
    int                 bits = 8;
    QVector<StackLayer> layers;
    bool                valid = false;
};
// Conservative peak for simultaneously visible tiles. Refuse a GPU stack
// before allocation when even its pinned working set would exceed the budget.
inline qint64 stackWorkingBytes(const LayerStack &stack, const QRect &visible)
{
    constexpr int tile = 512;
    const QRect   area = visible.intersected(QRect(QPoint{}, stack.size));
    if (area.isEmpty())
        return 0;
    qint64 bytes = 0;
    for (int y = area.top() / tile; y <= area.bottom() / tile; ++y)
        for (int x = area.left() / tile; x <= area.right() / tile; ++x) {
            const auto   size       = QRect(x * tile - 1, y * tile - 1, tile + 2, tile + 2)
                                          .intersected(QRect(QPoint{}, stack.size))
                                          .size();
            const qint64 pixels     = qint64(size.width()) * size.height();
            const int    colorBytes = stack.bits == 16 ? 16 : 4;
            bytes += pixels * (colorBytes + 1) * 2 + 64;
            for (const auto &layer : stack.layers) {
                bytes += layer.image.size() == QSize(1, 1) || layer.image.isNull()
                             ? colorBytes
                             : pixels * colorBytes;
                bytes += layer.mask.size() == QSize(1, 1) || layer.mask.isNull() ? 1 : pixels;
                bytes += 96;
            }
        }
    return bytes;
}
} // namespace PhotoCrap::Render
