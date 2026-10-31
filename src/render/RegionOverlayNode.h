#pragma once
#include "RegionMesh.h"
#include <QColor>
#include <QSGGeometryNode>

class RegionOverlayNode : public QSGGeometryNode
{
public:
    explicit RegionOverlayNode(const QColor &_color);
    void setMesh(const QVector<PhotoCrap::Render::RegionVertex> &_vertices);

private:
    QSGGeometry m_geometry;
};
