#pragma once
#include <QRectF>
#include <QRegion>
#include <QVector>

namespace PhotoCrap::Render
{
struct RegionVertex {
    float x, y;
};
struct RegionMesh {
    QVector<RegionVertex> vertices;
    bool                  fallback = false;
};
RegionMesh regionMesh(const QRegion &_region, const QSizeF &_scale, const QRectF &_viewport,
                      qsizetype _maximumVertices = 1000000);
} // namespace PhotoCrap::Render
