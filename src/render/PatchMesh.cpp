#include "PatchMesh.h"

namespace PhotoCrap::Render
{
PatchMesh patchMesh(const QRegion &_region, const QRect &_target, const QSizeF &_scale,
                    const QRectF &_viewport, qsizetype _maximumVertices)
{
    PatchMesh mesh;
    if (_target.isEmpty() || _viewport.isEmpty() || _scale.isEmpty())
        return mesh;
    const QRectF target(_target.x() * _scale.width(), _target.y() * _scale.height(),
                        _target.width() * _scale.width(), _target.height() * _scale.height());
    const auto   vertex = [&](qreal _x, qreal _y) {
        return PatchVertex{float(_x - _viewport.x()), float(_y - _viewport.y()),
                           float((_x - target.x()) / target.width()),
                           float((_y - target.y()) / target.height())};
    };
    for (const auto &rect : _region) {
        const QRectF mapped(rect.x() * _scale.width(), rect.y() * _scale.height(),
                            rect.width() * _scale.width(), rect.height() * _scale.height());
        const auto   visible = mapped.intersected(_viewport).intersected(target);
        if (visible.isEmpty())
            continue;
        if (mesh.vertices.size() + 6 > _maximumVertices)
            return {{}, true};
        const auto a = vertex(visible.left(), visible.top()),
                   b = vertex(visible.right(), visible.top());
        const auto c = vertex(visible.left(), visible.bottom()),
                   d = vertex(visible.right(), visible.bottom());
        mesh.vertices << a << c << b << b << c << d;
    }
    return mesh;
}
} // namespace PhotoCrap::Render
