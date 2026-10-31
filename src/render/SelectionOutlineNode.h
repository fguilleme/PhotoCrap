#pragma once
#include "SelectionMesh.h"
#include <QSGGeometryNode>

class SelectionOutlineNode : public QSGGeometryNode
{
public:
    SelectionOutlineNode();
    void setMesh(const QVector<PhotoCrap::Render::SelectionVertex> &_vertices);
    void setPhase(qreal _phase);

private:
    QSGGeometry m_geometry;
};
