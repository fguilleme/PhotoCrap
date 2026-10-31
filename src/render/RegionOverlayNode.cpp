#include "RegionOverlayNode.h"
#include <QSGFlatColorMaterial>
#include <cstring>

RegionOverlayNode::RegionOverlayNode(const QColor &_color)
    : m_geometry(QSGGeometry::defaultAttributes_Point2D(), 0)
{
    m_geometry.setDrawingMode(QSGGeometry::DrawTriangles);
    m_geometry.setVertexDataPattern(QSGGeometry::StaticPattern);
    setGeometry(&m_geometry);
    auto *material = new QSGFlatColorMaterial;
    material->setColor(_color);
    setMaterial(material);
    setFlag(OwnsMaterial);
    static_assert(sizeof(PhotoCrap::Render::RegionVertex) == sizeof(QSGGeometry::Point2D));
}
void RegionOverlayNode::setMesh(const QVector<PhotoCrap::Render::RegionVertex> &_vertices)
{
    m_geometry.allocate(int(_vertices.size()));
    if (!_vertices.isEmpty())
        std::memcpy(m_geometry.vertexData(), _vertices.constData(),
                    size_t(_vertices.size()) * sizeof(PhotoCrap::Render::RegionVertex));
    m_geometry.markVertexDataDirty();
    markDirty(DirtyGeometry);
}
