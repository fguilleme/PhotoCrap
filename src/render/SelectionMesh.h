#pragma once
#include <QPainterPath>
#include <QRectF>
#include <QVector>

namespace PhotoCrap::Render
{
struct SelectionVertex {
    float x, y, distance, side;
};
struct SelectionMesh {
    QVector<SelectionVertex> vertices;
    bool                     fallback = false;
};

// Vertices are relative to the viewport origin. Distance is in physical pixels
// and rebased modulo the dash period after clipping, retaining its path phase.
SelectionMesh selectionMesh(const QPainterPath &_path, const QSizeF &_scale,
                            const QRectF &_viewport, qreal _ratio,
                            qsizetype _maximumVertices = 1000000);
} // namespace PhotoCrap::Render
