#pragma once
#include <QSGGeometryNode>
#include <QSGTexture>

// Source and LUT textures are borrowed from the owning canvas scene graph node.
class ToneMappingNode : public QSGGeometryNode
{
public:
    ToneMappingNode(QSGTexture *_source, QSGTexture *_table, bool _luminance);
    void setTable(QSGTexture *_table, bool _luminance);
    void setImageRect(const QRectF &_rect, const QRectF &_sourceRect);
    void setFiltering(QSGTexture::Filtering _filtering);

private:
    QSGGeometry m_geometry;
};
