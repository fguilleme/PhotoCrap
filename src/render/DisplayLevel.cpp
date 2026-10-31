#include "DisplayLevel.h"
#include <QColorSpace>
#include <QtMath>
#include <cstring>

namespace PhotoCrap::Render
{
DisplayLevelUpdate updateDisplayLevel(QImage &_cache, const QImage &_source, const QSize &_size,
                                      const QRect &_changed)
{
    if (_source.isNull() || _size.isEmpty()) {
        _cache = {};
        return {};
    }
    const QRect bounds(QPoint{}, _size);
    const bool  reusable = _cache.size() == _size && _cache.format() == _source.format() &&
                           _cache.colorSpace() == _source.colorSpace() &&
                           _cache.devicePixelRatio() == _source.devicePixelRatio() &&
                           _source.depth() >= 8 && !_changed.isEmpty();
    QRect       changed  = bounds;
    if (reusable) {
        const qreal sx = qreal(_size.width()) / _source.width();
        const qreal sy = qreal(_size.height()) / _source.height();
        changed        = QRectF(_changed.x() * sx, _changed.y() * sy, _changed.width() * sx,
                                _changed.height() * sy)
                             .toAlignedRect()
                             .adjusted(-1, -1, 1, 1)
                             .intersected(bounds);
    }
    // Cropping an irregular grid changes Qt's sampling phase and fixed-point
    // weights. Preserve the full reduction in that case, but upload only the
    // affected output tiles. Exact integer grids allow an aligned local patch.
    if (reusable && _source.width() % _size.width() == 0 &&
        _source.height() % _size.height() == 0 && !changed.isEmpty()) {
        const int   fx = _source.width() / _size.width();
        const int   fy = _source.height() / _size.height();
        const QRect input(changed.x() * fx, changed.y() * fy, changed.width() * fx,
                          changed.height() * fy);
        const auto  source = input == _source.rect() ? _source : _source.copy(input);
        const auto  patch =
            source.scaled(changed.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (!patch.isNull() && patch.format() == _cache.format()) {
            const qsizetype bytes  = qsizetype(changed.width()) * _cache.depth() / 8;
            const qsizetype offset = qsizetype(changed.x()) * _cache.depth() / 8;
            for (int y = 0; y < changed.height(); ++y)
                std::memcpy(_cache.scanLine(changed.y() + y) + offset, patch.constScanLine(y),
                            bytes);
            return {changed, qint64(input.width()) * input.height(), true};
        }
    }
    _cache = _source.scaled(_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return {changed, qint64(_source.width()) * _source.height(), false};
}
} // namespace PhotoCrap::Render
