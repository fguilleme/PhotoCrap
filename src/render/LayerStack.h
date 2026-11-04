#pragma once
#include "../filters/ToneMappingTable.h"
#include "../storage/PixelPlane.h"
#include "SpatialFilterSettings.h"
#include <QVector>
#include <atomic>
#include <functional>

namespace PhotoCrap::Render
{
struct StackStatistics {
    std::atomic<quint64>         uploads{0}, passes{0}, bytes{0}, reductions{0};
    std::function<void(quint64)> changed;
};
struct StackLayer {
    PixelPlane                              image, mask, toneTable;
    float                                   opacity = 1, density = 1;
    int                                     blend   = 0;
    bool                                    clipped = false, reset = false;
    SpatialFilterSettings                   spatial, maskFilter;
    PhotoCrap::Filters::ToneMappingSettings tone;
};
// Immutable, shallow GUI-thread snapshot. Pixel reads and GPU resources belong
// to the scene graph thread; no live Document is accessed there.
struct LayerStack {
    QSize               size;
    int                 bits         = 8;
    int                 halo         = 0;
    int                 displayLevel = 0;
    QVector<StackLayer> layers;
    bool                valid        = false;
    bool                streamInputs = false;
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
            const int    padding = stack.halo + 33;
            const auto   size    = QRect(x * tile - padding, y * tile - padding, tile + 2 * padding,
                                         tile + 2 * padding)
                                       .intersected(QRect(QPoint{}, stack.size))
                                       .size();
            const qint64 pixels  = qint64(size.width()) * size.height();
            const int    colorBytes = stack.bits == 16 ? 16 : 4;
            bytes += pixels * (colorBytes + 1) * 2 + 64;
            bool   spatial = false, masks = false;
            qint64 tableBytes = 0;
            if (stack.streamInputs)
                bytes += pixels * (colorBytes + 1);
            for (const auto &layer : stack.layers) {
                spatial |= layer.spatial.type != SpatialFilter::None;
                masks |= layer.maskFilter.type != SpatialFilter::None;
                if (!stack.streamInputs) {
                    bytes += layer.image.size() == QSize(1, 1) || layer.image.isNull()
                                 ? colorBytes
                                 : pixels * colorBytes;
                    bytes += layer.mask.size() == QSize(1, 1) || layer.mask.isNull() ? 1 : pixels;
                }
                bytes += 256;
                const qint64 table =
                    qint64(layer.toneTable.width()) * layer.toneTable.height() * colorBytes;
                if (stack.streamInputs)
                    tableBytes = qMax(tableBytes, table);
                else
                    bytes += table;
                if (layer.spatial.type != SpatialFilter::None)
                    bytes += 7 * 96;
                if (layer.maskFilter.type != SpatialFilter::None)
                    bytes += 2 * 96;
            }
            bytes += tableBytes;
            if (spatial)
                bytes += pixels * (colorBytes + 16) + 64;
            if (masks)
                bytes += pixels * 20 + 64;
            // Four retained reductions, including their horizontal FP32 targets.
            bytes += pixels * (16 + (colorBytes + 2) / 3) + 4 * 256 + 64;
        }
    return bytes;
}
} // namespace PhotoCrap::Render
