#pragma once
#include "../filters/ToneMappingTable.h"
#include <QSGGeometryNode>
#include <QSGTexture>

// Source and LUT textures are borrowed from the owning canvas scene graph node.
class ToneMappingNode : public QSGGeometryNode
{
public:
    ToneMappingNode(QSGTexture *_source, QSGTexture *_table,
                    const PhotoCrap::Filters::ToneMappingSettings &_settings,
                    QSGTexture                                    *_mask = nullptr);
    void setTable(QSGTexture *_table, const PhotoCrap::Filters::ToneMappingSettings &_settings,
                  QSGTexture *_mask = nullptr);
    void setImageRect(const QRectF &_rect, const QRectF &_sourceRect, const QPointF &_origin);
    void setFiltering(QSGTexture::Filtering _filtering);

private:
    QSGGeometry m_geometry;
};
