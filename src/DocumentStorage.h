#pragma once

#include <QtGlobal>

namespace DocumentStorage
{
constexpr int MaxDimension = 16384;
constexpr int MaxLayers = 512;
constexpr qint64 MaxPixels = 128000000LL;
constexpr qint64 MaxLayerBytes = 2LL * 1024 * 1024 * 1024;

constexpr bool validDimensions(qint64 width, qint64 height)
{
    return width > 0 && width <= MaxDimension && height > 0 && height <= MaxDimension &&
           width * height <= MaxPixels;
}

// Reserve each stored plane independently, even when QImage currently shares
// pixels. Editing, resizing and reloading can detach that shared storage.
constexpr qint64 estimateBytes(int _width, int _height, int _colorPlanes, int _maskPlanes,
                               int _bits)
{
    const qint64 colorRow = qint64(_width) * (_bits / 2);
    const qint64 maskRow  = ((qint64(_width) + 3) / 4) * 4;
    return qint64(_height) * (colorRow * _colorPlanes + maskRow * _maskPlanes);
}
// Each occupied native tile is reserved independently for a sparse color plane.
constexpr qint64 tiledColorBytes(int width, int height, int bits, qint64 tiles)
{
    return qMin(estimateBytes(width, height, 1, 0, bits), tiles * 256 * 256 * (bits / 2));
}
} // namespace DocumentStorage
