#pragma once
#include "../filters/ToneMappingTable.h"
#include "../storage/PixelPlane.h"
#include "DisplayPyramidGeometry.h"
#include "SpatialFilterSettings.h"
#include <QSet>
#include <QVector>
#include <algorithm>
#include <atomic>
#include <functional>

namespace PhotoCrap::Render
{
struct StackStatistics {
    std::atomic<quint64>         uploads{0}, passes{0}, bytes{0}, reductions{0};
    std::atomic<quint64>         pipelineCreations{0}, bindingCreations{0}, uniformUpdates{0};
    std::function<void(quint64)> changed;
};
struct StackLayer {
    PixelPlane                              image, mask, toneTable, blendTable;
    float                                   opacity = 1, density = 1;
    int                                     blend   = 0;
    bool                                    clipped = false, reset = false;
    bool                                    tonal = false;
    SpatialFilterSettings                   spatial, maskFilter;
    PhotoCrap::Filters::ToneMappingSettings tone;

    bool needsColorInput() const
    {
        return !reset && !tonal && spatial.type == SpatialFilter::None;
    }
    bool needsMaskInput() const
    {
        return !reset && (!mask.isNull() || maskFilter.type != SpatialFilter::None);
    }
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
    bool                shareScratch = false;
};
inline bool stackUsesClipping(const LayerStack &stack)
{
    return std::any_of(stack.layers.cbegin(), stack.layers.cend(),
                       [](const StackLayer &layer) { return layer.clipped; });
}
inline bool compactStackInput(const PixelPlane &_plane, const QSize &_size)
{
    return _plane.isNull() || _plane.size() == QSize(1, 1) ||
           (_plane.size() == _size && _plane.isUniform());
}

inline QByteArray compactStackKey(const PixelPlane &plane)
{
    return plane.regionKey(QRect(0, 0, 1, 1));
}

inline constexpr int StackTileSize = 512;

inline QRect stackVisibleTiles(const QSize &size, const QRect &visible)
{
    const QRect area = visible.intersected(QRect(QPoint{}, size));
    if (area.isEmpty())
        return {};
    return QRect(QPoint(area.left() / StackTileSize, area.top() / StackTileSize),
                 QPoint(area.right() / StackTileSize, area.bottom() / StackTileSize));
}

// Conservative peak for simultaneously visible tiles. Refuse a GPU stack
// before allocation when even its pinned working set would exceed the budget.
inline qint64 stackWorkingBytes(const LayerStack &stack, const QRect &visible)
{
    const QRect tiles = stackVisibleTiles(stack.size, visible);
    if (tiles.isEmpty())
        return 0;
    const int colorBytes    = stack.bits == 16 ? 16 : 4;
    qint64           bytesPerPixel = (colorBytes + int(stackUsesClipping(stack))) * 2;
    const qint64 perTile       = 256 * stack.layers.size();
    qint64    scratchBytes = 0, tableBytes = 0, blendBytes = 0;
    qint64           sharedInputBytes = 0;
    QSet<qint64> sharedToneTables, sharedBlendTables;
    QSet<QByteArray> sharedColors, sharedMasks;
    int          spatialTargets = 0, maskTargets = 0;
    bool         colorInput = false, maskInput = false;
    bool         denseColor = false, denseMask = false;
    for (const auto &layer : stack.layers) {
        const int spatialPasses = layer.spatial.passCount();
        const int maskPasses    = layer.maskFilter.passCount();
        spatialTargets          = qMax(spatialTargets, qMin(2, spatialPasses));
        maskTargets             = qMax(maskTargets, qMin(2, maskPasses));
        if (layer.needsColorInput()) {
            colorInput = true;
            const bool compact = compactStackInput(layer.image, stack.size);
            denseColor |= !compact;
            if (!stack.streamInputs) {
                if (compact)
                    sharedColors.insert(compactStackKey(layer.image));
                else
                    bytesPerPixel += colorBytes;
            }
        }
        if (layer.needsMaskInput()) {
            maskInput = true;
            const bool compact = compactStackInput(layer.mask, stack.size);
            denseMask |= !compact;
            if (!stack.streamInputs) {
                if (compact)
                    sharedMasks.insert(compactStackKey(layer.mask));
                else
                    ++bytesPerPixel;
            }
        }
        const qint64 table = layer.toneTable.isNull() ? 0
                                                      : qint64(layer.toneTable.width()) *
                                                            layer.toneTable.height() * colorBytes;
        const qint64 blend = layer.blendTable.isNull() ? 0
                                                       : qint64(layer.blendTable.width()) *
                                                             layer.blendTable.height() * 16;
        if (stack.streamInputs) {
            tableBytes = qMax(tableBytes, table);
            blendBytes = qMax(blendBytes, blend);
        } else {
            if (table && !sharedToneTables.contains(layer.toneTable.cacheKey())) {
                sharedToneTables.insert(layer.toneTable.cacheKey());
                sharedInputBytes += table;
            }
            if (blend && !sharedBlendTables.contains(layer.blendTable.cacheKey())) {
                sharedBlendTables.insert(layer.blendTable.cacheKey());
                sharedInputBytes += blend;
            }
        }
        scratchBytes += qint64(spatialPasses + maskPasses) * 96;
    }
    if (stack.streamInputs) {
        bytesPerPixel += denseColor * colorBytes + denseMask;
        scratchBytes += tableBytes + blendBytes + (colorInput && !denseColor) * colorBytes +
                        (maskInput && !denseMask);
    }
    if (spatialTargets)
        bytesPerPixel += 16 + (spatialTargets - 1) * colorBytes;
    if (maskTargets)
        // The horizontal target may fall back to RGBA32F on the render thread.
        bytesPerPixel += 16 + (maskTargets - 1);

    qint64 bytes = 64 + sharedInputBytes + sharedColors.size() * colorBytes + sharedMasks.size();
    QSet<QSize> worksets, reductions;
    for (int y = tiles.top(); y <= tiles.bottom(); ++y)
        for (int x = tiles.left(); x <= tiles.right(); ++x) {
            const int    padding = stack.halo + 33;
            const QRect  region  = QRect(x * StackTileSize - padding, y * StackTileSize - padding,
                                         StackTileSize + 2 * padding, StackTileSize + 2 * padding)
                                       .intersected(QRect(QPoint{}, stack.size));
            const QSize  size    = region.size();
            const qint64 pixels  = qint64(size.width()) * size.height();
            if (!stack.shareScratch || !worksets.contains(size)) {
                bytes += pixels * bytesPerPixel + scratchBytes;
                if (stack.shareScratch)
                    worksets.insert(size);
            }
            bytes += perTile;
            if (stack.shareScratch && stack.displayLevel == 0)
                bytes += pixels * colorBytes;
            // All four retained levels are budgeted, with one horizontal
            // target per size and one output per tile and level.
            for (int level = 1; level <= 4; ++level) {
                const QSize reduced(qMax(1, stack.size.width() >> level),
                                    qMax(1, stack.size.height() >> level));
                const auto [horizontal, output] = pyramidRegions(region, stack.size, reduced);
                if (output.isEmpty())
                    continue;
                if (!reductions.contains(horizontal.size())) {
                    reductions.insert(horizontal.size());
                    bytes += qint64(horizontal.width()) * horizontal.height() * 16;
                }
                bytes += qint64(output.width()) * output.height() * colorBytes + 256;
            }
        }
    return bytes;
}
} // namespace PhotoCrap::Render
