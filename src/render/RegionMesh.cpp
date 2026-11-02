#include "RegionMesh.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace PhotoCrap::Render
{
RegionMesh regionMesh(const QRegion &_region, const QSizeF &_scale, const QRectF &_viewport,
                      qsizetype _maximumVertices)
{
    RegionMesh mesh;
    if (_viewport.isEmpty() || _scale.isEmpty() || _region.isEmpty())
        return mesh;
    const QRect bounds = _region.boundingRect();
    QRectF      mappedBounds(bounds.x() * _scale.width(), bounds.y() * _scale.height(),
                             bounds.width() * _scale.width(), bounds.height() * _scale.height());
    // A conservative margin prevents floating-point rounding of a large
    // bounding box from discarding a thin visible rectangle at its edge.
    const qreal magnitude =
        std::max({qreal(1), std::abs(mappedBounds.left()), std::abs(mappedBounds.right()),
                  std::abs(mappedBounds.top()), std::abs(mappedBounds.bottom())});
    const qreal margin = magnitude * std::numeric_limits<qreal>::epsilon() * 16;
    mappedBounds.adjust(-margin, -margin, margin, margin);
    if (std::isfinite(margin) && !mappedBounds.intersects(_viewport))
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
