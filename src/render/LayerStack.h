#pragma once
#include "../filters/ToneMappingTable.h"
#include "../storage/PixelPlane.h"
#include "SpatialFilterSettings.h"
#include <QSet>
#include <QVector>
#include <atomic>
#include <cmath>
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
inline bool compactStackInput(const PixelPlane &_plane, const QSize &_size)
{
    return _plane.isNull() || _plane.size() == QSize(1, 1) ||
           (_plane.size() == _size && _plane.isUniform());
}

// Conservative peak for simultaneously visible tiles. Refuse a GPU stack
// before allocation when even its pinned working set would exceed the budget.
inline qint64 stackWorkingBytes(const LayerStack &stack, const QRect &visible)
{
    constexpr int tile = 512;
    const QRect   area = visible.intersected(QRect(QPoint{}, stack.size));
    if (area.isEmpty())
        return 0;
    const int colorBytes    = stack.bits == 16 ? 16 : 4;
    qint64    bytesPerPixel = (colorBytes + 1) * 2;
    const qint64 perTile       = 256 * stack.layers.size();
    qint64    scratchBytes = 0, tableBytes = 0, blendBytes = 0;
    qint64       sharedTableBytes = 0;
    QSet<qint64> sharedToneTables, sharedBlendTables;
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
                    scratchBytes += colorBytes;
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
                    ++scratchBytes;
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
                sharedTableBytes += table;
            }
            if (blend && !sharedBlendTables.contains(layer.blendTable.cacheKey())) {
                sharedBlendTables.insert(layer.blendTable.cacheKey());
                sharedTableBytes += blend;
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
        bytesPerPixel += 16 + (maskTargets - 1) * 4;

    qint64      bytes = 64 + sharedTableBytes;
    QSet<QSize> worksets, reductions;
    for (int y = area.top() / tile; y <= area.bottom() / tile; ++y)
        for (int x = area.left() / tile; x <= area.right() / tile; ++x) {
            const int    padding = stack.halo + 33;
            const QRect  region  = QRect(x * tile - padding, y * tile - padding, tile + 2 * padding,
                                         tile + 2 * padding)
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
            if (stack.shareScratch) {
                // All four retained zoom levels are budgeted. Horizontal native
                // scratch is shared by dimensions, but each output stays private.
                for (int level = 1; level <= 4; ++level) {
                    const QSize reduced(qMax(1, stack.size.width() >> level),
                                        qMax(1, stack.size.height() >> level));
                    const int   left =
                        int(std::ceil(double(region.x()) * reduced.width() / stack.size.width()));
                    const int right = int(std::floor(double(region.x() + region.width()) *
                                                     reduced.width() / stack.size.width()));
                    const int top =
                        int(std::ceil(double(region.y()) * reduced.height() / stack.size.height()));
                    const int   bottom = int(std::floor(double(region.y() + region.height()) *
                                                        reduced.height() / stack.size.height()));
                    const QSize horizontal(qMax(0, right - left), region.height());
                    if (!reductions.contains(horizontal)) {
                        reductions.insert(horizontal);
                        bytes += qint64(horizontal.width()) * horizontal.height() * 16;
                    }
                    bytes += qint64(qMax(0, right - left)) * qMax(0, bottom - top) * colorBytes +
                             256 + 64;
                }
            } else
                bytes += pixels * (16 + (colorBytes + 2) / 3) + 4 * 256 + 64;
        }
    return bytes;
}
} // namespace PhotoCrap::Render
