#pragma once
#include <QSGClipNode>
#include <QSGTransformNode>

// Local overlay coordinates avoid huge vertex positions. The explicit clip
// geometry also supports Qt's stencil path when an ancestor is rotated.
class ClippedOverlayNode : public QSGTransformNode
{
public:
    ClippedOverlayNode();
    void appendContent(QSGNode *_node);
    void setViewport(const QRectF &_viewport);

private:
    QSGClipNode *m_clip;
};
