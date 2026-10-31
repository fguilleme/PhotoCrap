#pragma once
#include "PatchMesh.h"
#include <QSGGeometryNode>
#include <QSGTexture>

// Owns the bounded donor-preview texture. Geometry positions are local to the
// clipped viewport, while UVs cover the target rectangle before clipping.
class PatchPreviewNode : public QSGGeometryNode
{
public:
    explicit PatchPreviewNode(QSGTexture *_texture);
    ~PatchPreviewNode() override;
    bool setTexture(QSGTexture *_texture);
    void setMesh(const QVector<PhotoCrap::Render::PatchVertex> &_vertices);

private:
    QSGGeometry m_geometry;
    QSGTexture *m_texture = nullptr;
};
