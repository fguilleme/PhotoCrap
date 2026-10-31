#include "RegionMesh.h"

namespace PhotoCrap::Render
{
RegionMesh regionMesh(const QRegion &_region, const QSizeF &_scale, const QRectF &_viewport,
                      qsizetype _maximumVertices)
{
    RegionMesh mesh;
    if (_viewport.isEmpty() || _scale.isEmpty())
        return mesh;
    for (const auto &rect : _region) {
        const QRectF mapped(rect.x() * _scale.width(), rect.y() * _scale.height(),
                            rect.width() * _scale.width(), rect.height() * _scale.height());
        const auto   visible = mapped.intersected(_viewport).translated(-_viewport.topLeft());
        if (visible.isEmpty())
            continue;
        if (mesh.vertices.size() + 6 > _maximumVertices)
            return {{}, true};
        const RegionVertex a{float(visible.left()), float(visible.top())};
        const RegionVertex b{float(visible.right()), float(visible.top())};
        const RegionVertex c{float(visible.left()), float(visible.bottom())};
        const RegionVertex d{float(visible.right()), float(visible.bottom())};
        mesh.vertices << a << c << b << b << c << d;
    }
    return mesh;
}
} // namespace PhotoCrap::Render
