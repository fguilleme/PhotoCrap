#pragma once
#include "CropShade.h"
#include <QRectF>
#include <QSGGeometryNode>

class QQuickWindow;
class CropShadeNode : public QSGGeometryNode
{
public:
    explicit CropShadeNode(QQuickWindow *_owner);
    bool          setRect(const QRectF &_rect);
    bool          setParameters(const PhotoCrap::Render::CropShadeParameters &_parameters);
    QQuickWindow *owner() const
    {
        return m_owner;
    }

private:
    QQuickWindow *m_owner;
    QSGGeometry   m_geometry;
    QRectF        m_rect;
};
