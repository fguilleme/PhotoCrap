#pragma once
#include "render/SelectionMesh.h"
#include <QPolygonF>
#include <QQuickItem>
#include <QVariantList>
#ifdef PHOTOCRAP_TESTING
#include <atomic>
#endif

// A closed, logical-pixel guide. Only geometry changes prepare a new mesh.
class DashedGuideItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList points READ points WRITE setPoints NOTIFY pointsChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(QPointF origin READ origin WRITE setOrigin NOTIFY originChanged)
    Q_PROPERTY(qreal phase READ phase WRITE setPhase NOTIFY phaseChanged)
    Q_PROPERTY(bool supported READ supported NOTIFY supportedChanged)
    Q_PROPERTY(bool hasGeometry READ hasGeometry NOTIFY hasGeometryChanged)
public:
    explicit DashedGuideItem(QQuickItem *_parent = nullptr);
    QVariantList points() const;
    void         setPoints(const QVariantList &_points);
    qreal        zoom() const
    {
        return m_zoom;
    }
    void    setZoom(qreal _zoom);
    QPointF origin() const
    {
        return m_origin;
    }
    void  setOrigin(QPointF _origin);
    qreal phase() const
    {
        return m_phase;
    }
    void setPhase(qreal _phase);
    bool supported() const
    {
        return m_supported;
    }
    bool hasGeometry() const
    {
        return m_supported && !m_mesh.vertices.isEmpty();
    }
#ifdef PHOTOCRAP_TESTING
    quint64 renderUpdates() const
    {
        return m_renderUpdates.load();
    }
    quint64 geometryBuilds() const
    {
        return m_generation;
    }
    quint64 geometryUploads() const
    {
        return m_geometryUploads.load();
    }
    quint64 parameterUpdates() const
    {
        return m_parameterUpdates.load();
    }
    bool shaderActive() const
    {
        return m_shaderActive.load();
    }
#endif
signals:
    void pointsChanged();
    void zoomChanged();
    void originChanged();
    void phaseChanged();
    void supportedChanged();
    void hasGeometryChanged();

protected:
    void     updatePolish() override;
    QSGNode *updatePaintNode(QSGNode *_node, UpdatePaintNodeData *) override;
    void     geometryChange(const QRectF &_new, const QRectF &_old) override;
    void     itemChange(ItemChange _change, const ItemChangeData &_data) override;

private:
    void                             invalidateGeometry();
    QPolygonF                        m_points, m_closure;
    PhotoCrap::Render::SelectionMesh m_mesh;
    QPointF                          m_origin;
    qreal                            m_zoom = 1, m_phase = 0, m_ratio = 1;
    quint64                          m_generation  = 0;
    bool                             m_validPoints = true, m_dirty = true, m_supported = false;
#ifdef PHOTOCRAP_TESTING
    std::atomic<quint64> m_geometryUploads{0}, m_parameterUpdates{0}, m_renderUpdates{0};
    std::atomic<bool>    m_shaderActive{false};
#endif
};
