#include "ClippedOverlayNode.h"
#include <QSGGeometry>

ClippedOverlayNode::ClippedOverlayNode() : m_clip(new QSGClipNode)
{
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
    geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    m_clip->setGeometry(geometry);
    m_clip->setFlag(QSGNode::OwnsGeometry);
    m_clip->setIsRectangular(true);
    appendChildNode(m_clip);
}
void ClippedOverlayNode::appendContent(QSGNode *_node)
{
    m_clip->appendChildNode(_node);
}
void ClippedOverlayNode::setViewport(const QRectF &_viewport)
{
    QMatrix4x4 transform;
    transform.translate(float(_viewport.x()), float(_viewport.y()));
    if (matrix() != transform)
        setMatrix(transform);
    const QRectF clip(QPointF{}, _viewport.size());
    if (m_clip->clipRect() == clip)
        return;
    m_clip->setClipRect(clip);
    QSGGeometry::updateRectGeometry(m_clip->geometry(), clip);
    m_clip->geometry()->markVertexDataDirty();
    m_clip->markDirty(QSGNode::DirtyGeometry);
}
