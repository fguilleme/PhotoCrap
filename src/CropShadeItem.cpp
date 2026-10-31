#include "CropShadeItem.h"
#include "render/CropShadeNode.h"
#include <QJSValue>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QVariantMap>

CropShadeItem::CropShadeItem(QQuickItem *_parent) : QQuickItem(_parent)
{
    setFlag(ItemHasContents);
    connect(this, &QQuickItem::windowChanged, this, [this] { update(); });
}
QVariantList CropShadeItem::points() const
{
    QVariantList result;
    for (const auto &point : m_points)
        result.append(QVariantMap{{"x", point.x()}, {"y", point.y()}});
    return result;
}
void CropShadeItem::setPoints(const QVariantList &_points)
{
    QPolygonF points;
    for (const auto &value : _points) {
        // Convert while on the GUI thread; the scene graph only sees value points.
        const auto map = value.metaType() == QMetaType::fromType<QJSValue>()
                             ? value.value<QJSValue>().toVariant().toMap()
                             : value.toMap();
        points.append({map.value("x").toReal(), map.value("y").toReal()});
    }
    if (points == m_points)
        return;
    m_points = std::move(points);
    emit pointsChanged();
    update();
}
void CropShadeItem::setZoom(qreal _zoom)
{
    if (m_zoom == _zoom)
        return;
    m_zoom = _zoom;
    emit zoomChanged();
    update();
}
void CropShadeItem::setOrigin(const QPointF &_origin)
{
    if (m_origin == _origin)
        return;
    m_origin = _origin;
    emit originChanged();
    update();
}
void CropShadeItem::geometryChange(const QRectF &_newGeometry, const QRectF &_oldGeometry)
{
    QQuickItem::geometryChange(_newGeometry, _oldGeometry);
    update();
}
QSGNode *CropShadeItem::updatePaintNode(QSGNode *_oldNode, UpdatePaintNodeData *)
{
    auto      *node     = static_cast<CropShadeNode *>(_oldNode);
    const bool hardware = window() && QSGRendererInterface::isApiRhiBased(
                                          window()->rendererInterface()->graphicsApi());
    if (!hardware || width() <= 0 || height() <= 0) {
        delete node;
#ifdef PHOTOCRAP_TESTING
        m_shaderActive = false;
#endif
        return nullptr;
    }
    if (node && node->owner() != window()) {
        delete node;
        node = nullptr;
    }
    if (!node)
        node = new CropShadeNode(window());
    const bool geometry = node->setRect(boundingRect());
    const bool parameters =
        node->setParameters(PhotoCrap::Render::cropShadeParameters(m_points, m_zoom, m_origin));
#ifdef PHOTOCRAP_TESTING
    m_shaderActive = true;
    if (geometry)
        ++m_geometryUploads;
    if (parameters)
        ++m_parameterUpdates;
#else
    Q_UNUSED(geometry)
    Q_UNUSED(parameters)
#endif
    return node;
}
