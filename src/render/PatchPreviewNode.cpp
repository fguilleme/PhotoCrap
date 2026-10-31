#include "PatchPreviewNode.h"
#include <QSGTextureMaterial>

PatchPreviewNode::PatchPreviewNode(QSGTexture *_texture)
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0)
{
    m_geometry.setDrawingMode(QSGGeometry::DrawTriangles);
    m_geometry.setVertexDataPattern(QSGGeometry::StaticPattern);
    setGeometry(&m_geometry);
    auto *material = new QSGTextureMaterial;
    material->setFiltering(QSGTexture::Nearest);
    material->setHorizontalWrapMode(QSGTexture::ClampToEdge);
    material->setVerticalWrapMode(QSGTexture::ClampToEdge);
    material->setFlag(QSGMaterial::Blending);
    setMaterial(material);
    setFlag(OwnsMaterial);
    setTexture(_texture);
}
PatchPreviewNode::~PatchPreviewNode()
{
    delete m_texture;
}
bool PatchPreviewNode::setTexture(QSGTexture *_texture)
{
    if (m_texture == _texture)
        return false;
    const auto before   = m_texture ? m_texture->normalizedTextureSubRect() : QRectF{};
    auto      *previous = m_texture;
    m_texture           = _texture;
    static_cast<QSGTextureMaterial *>(material())->setTexture(_texture);
    delete previous;
    markDirty(DirtyMaterial);
    return before != _texture->normalizedTextureSubRect();
}
void PatchPreviewNode::setMesh(const QVector<PhotoCrap::Render::PatchVertex> &_vertices)
{
    m_geometry.allocate(int(_vertices.size()));
    const auto sub    = m_texture->normalizedTextureSubRect();
    auto      *points = m_geometry.vertexDataAsTexturedPoint2D();
    for (qsizetype i = 0; i < _vertices.size(); ++i) {
        const auto &v = _vertices[i];
        points[i].set(v.x, v.y, float(sub.x() + v.u * sub.width()),
                      float(sub.y() + v.v * sub.height()));
    }
    m_geometry.markVertexDataDirty();
    markDirty(DirtyGeometry);
}
