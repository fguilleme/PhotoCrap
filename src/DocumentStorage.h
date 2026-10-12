#pragma once

#include <QtGlobal>

namespace DocumentStorage
{
constexpr qint64 MaxLayerBytes = 2LL * 1024 * 1024 * 1024;

// Reserve each stored plane independently, even when QImage currently shares
// pixels. Editing, resizing and reloading can detach that shared storage.
constexpr qint64 estimateBytes(int _width, int _height, int _colorPlanes, int _maskPlanes,
                               int _bits)
{
    const qint64 colorRow = qint64(_width) * (_bits / 2);
    const qint64 maskRow  = ((qint64(_width) + 3) / 4) * 4;
    return qint64(_height) * (colorRow * _colorPlanes + maskRow * _maskPlanes);
}
} // namespace DocumentStorage
