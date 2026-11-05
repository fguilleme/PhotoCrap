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
    std::function<void(quint64)> changed;
};
struct StackLayer {
    PixelPlane                              image, mask, toneTable, blendTable;
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
    bool                shareScratch = false;
};
// Conservative peak for simultaneously visible tiles. Refuse a GPU stack
// before allocation when even its pinned working set would exceed the budget.
inline qint64 stackWorkingBytes(const LayerStack &stack, const QRect &visible)
{
    constexpr int tile = 512;
    const QRect   area = visible.intersected(QRect(QPoint{}, stack.size));
    if (area.isEmpty())
        return 0;
    qint64      bytes = 0;
    QSet<QSize> worksets, reductions;
    for (int y = area.top() / tile; y <= area.bottom() / tile; ++y)
        for (int x = area.left() / tile; x <= area.right() / tile; ++x) {
            const int    padding = stack.halo + 33;
            const QRect  region  = QRect(x * tile - padding, y * tile - padding, tile + 2 * padding,
                                         tile + 2 * padding)
                                       .intersected(QRect(QPoint{}, stack.size));
            const QSize  size    = region.size();
            const qint64 pixels  = qint64(size.width()) * size.height();
            const int    colorBytes = stack.bits == 16 ? 16 : 4;
            const bool   scratch    = !stack.shareScratch || !worksets.contains(size);
            worksets.insert(size);
            qint64 working    = pixels * (colorBytes + 1) * 2;
            qint64 tableBytes = 0, blendBytes = 0;
            bool   spatial = false, masks = false;
            if (stack.streamInputs)
                working += pixels * (colorBytes + 1);
            for (const auto &layer : stack.layers) {
                spatial |= layer.spatial.type != SpatialFilter::None;
                masks |= layer.maskFilter.type != SpatialFilter::None;
                if (!stack.streamInputs) {
                    working += layer.image.size() == QSize(1, 1) || layer.image.isNull()
                                   ? colorBytes
                                   : pixels * colorBytes;
                    working += layer.mask.size() == QSize(1, 1) || layer.mask.isNull() ? 1 : pixels;
                }
                bytes += 256;
                const qint64 table =
                    layer.toneTable.isNull()
                        ? 0
                        : qint64(layer.toneTable.width()) * layer.toneTable.height() * colorBytes;
                if (stack.streamInputs)
                    tableBytes = qMax(tableBytes, table);
                else
                    working += table;
                const qint64 blend = layer.blendTable.isNull() ? 0
                                                               : qint64(layer.blendTable.width()) *
                                                                     layer.blendTable.height() * 16;
                if (stack.streamInputs)
                    blendBytes = qMax(blendBytes, blend);
                else
                    working += blend;
                if (layer.spatial.type != SpatialFilter::None)
                    working += 7 * 96;
                if (layer.maskFilter.type != SpatialFilter::None)
                    working += 2 * 96;
            }
            working += tableBytes + blendBytes;
            if (spatial)
                working += pixels * (colorBytes + 16) + 64;
            if (masks)
                working += pixels * 20 + 64;
            if (scratch)
                bytes += working;
            bytes += 64;
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
