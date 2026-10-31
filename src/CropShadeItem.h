#pragma once
#include <QPointF>
#include <QPolygonF>
#include <QQuickItem>
#include <QVariantList>
#ifdef PHOTOCRAP_TESTING
#include <atomic>
#endif

class CropShadeItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList points READ points WRITE setPoints NOTIFY pointsChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(QPointF origin READ origin WRITE setOrigin NOTIFY originChanged)
public:
    explicit CropShadeItem(QQuickItem *_parent = nullptr);
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
    void setOrigin(const QPointF &_origin);
#ifdef PHOTOCRAP_TESTING
    bool shaderActive() const
    {
        return m_shaderActive.load();
    }
    quint64 geometryUploads() const
    {
        return m_geometryUploads.load();
    }
    quint64 parameterUpdates() const
    {
        return m_parameterUpdates.load();
    }
#endif
signals:
    void pointsChanged();
    void zoomChanged();
    void originChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *_oldNode, UpdatePaintNodeData *) override;
    void     geometryChange(const QRectF &_newGeometry, const QRectF &_oldGeometry) override;

private:
    QPolygonF m_points;
    qreal     m_zoom = 1;
    QPointF   m_origin;
#ifdef PHOTOCRAP_TESTING
    std::atomic<bool>    m_shaderActive{false};
    std::atomic<quint64> m_geometryUploads{0}, m_parameterUpdates{0};
#endif
};
