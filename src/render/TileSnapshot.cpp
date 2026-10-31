#include "TileSnapshot.h"
#include <QColorSpace>
#include <cstring>

namespace PhotoCrap::Render
{
QImage tileSnapshot(const QImage &_source, const QRect &_region, const QImage &_previous)
{
    if (_source.isNull() || _region.isEmpty())
        return {};
    if (_source.rect().contains(_region) && _source.depth() >= 8 &&
        _previous.size() == _region.size() && _previous.format() == _source.format() &&
        _previous.colorSpace() == _source.colorSpace() &&
        _previous.devicePixelRatio() == _source.devicePixelRatio()) {
        const qsizetype bytes  = qsizetype(_region.width()) * _source.depth() / 8;
        const qsizetype offset = qsizetype(_region.x()) * _source.depth() / 8;
        bool            same   = true;
        for (int y = 0; y < _region.height(); ++y)
            if (std::memcmp(_source.constScanLine(_region.y() + y) + offset,
                            _previous.constScanLine(y), bytes) != 0) {
                same = false;
                break;
            }
        if (same)
            return _previous;
    }
    return _source.copy(_region);
}
} // namespace PhotoCrap::Render
