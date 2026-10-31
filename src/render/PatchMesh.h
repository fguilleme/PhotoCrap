#pragma once
#include <QRect>
#include <QRegion>
#include <QVector>

namespace PhotoCrap::Render
{
struct PatchVertex {
    float x, y, u, v;
};
struct PatchMesh {
    QVector<PatchVertex> vertices;
    bool                 fallback = false;
};
PatchMesh patchMesh(const QRegion &_region, const QRect &_target, const QSizeF &_scale,
                    const QRectF &_viewport, qsizetype _maximumVertices = 1000000);
} // namespace PhotoCrap::Render
