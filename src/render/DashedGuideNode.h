#pragma once
#include "SelectionMesh.h"
#include <QPolygonF>
#include <QSGNode>
#include <limits>

class QQuickWindow;
class DashedGuidePass;
class DashedGuideNode : public QSGNode
{
public:
    explicit DashedGuideNode(QQuickWindow *_owner);
    QQuickWindow *owner() const
    {
        return m_owner;
    }
    bool setMesh(const QVector<PhotoCrap::Render::SelectionVertex> &_vertices, quint64 _generation,
                 const QPolygonF &_closure);
    bool setParameters(qreal _phase, qreal _ratio);

private:
    QQuickWindow    *m_owner;
    DashedGuidePass *m_black, *m_white;
    quint64          m_generation = std::numeric_limits<quint64>::max();
};
