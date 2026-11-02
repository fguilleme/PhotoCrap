#include "DashedGuideItem.h"
#include "render/DashedGuideNode.h"
#include <QJSValue>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <cmath>

DashedGuideItem::DashedGuideItem(QQuickItem *_parent) : QQuickItem(_parent)
{
    setFlag(ItemHasContents);
    connect(this, &QQuickItem::windowChanged, this, [this] { invalidateGeometry(); });
}
QVariantList DashedGuideItem::points() const
{
    QVariantList result;
    for (const auto &p : m_points)
        result.append(QVariantMap{{"x", p.x()}, {"y", p.y()}});
    return result;
}
void DashedGuideItem::setPoints(const QVariantList &_points)
{
    QPolygonF points;
    bool      valid = _points.size() <= 8192;
    if (valid) {
        for (const auto &value : _points) {
            // Resolve QML values on the GUI thread, never during rendering.
            const auto    plain       = value.metaType() == QMetaType::fromType<QJSValue>()
                                            ? value.value<QJSValue>().toVariant()
                                            : value;
            const auto    map         = plain.toMap();
            const bool    nativePoint = plain.metaType() == QMetaType::fromType<QPointF>();
            const QPointF point = nativePoint
                                      ? plain.toPointF()
                                      : QPointF(map.value("x").toReal(), map.value("y").toReal());
            if ((!nativePoint && (!map.contains("x") || !map.contains("y"))) ||
                !std::isfinite(point.x()) || !std::isfinite(point.y()) || qAbs(point.x()) > 1e8 ||
                qAbs(point.y()) > 1e8) {
                valid = false;
                points.clear();
                break;
            }
            points.append(point);
        }
    }
    if (points == m_points && valid == m_validPoints)
        return;
    m_points      = std::move(points);
    m_validPoints = valid;
    emit pointsChanged();
    invalidateGeometry();
}
void DashedGuideItem::setZoom(qreal _zoom)
{
    if (m_zoom == _zoom)
        return;
    m_zoom = _zoom;
    emit zoomChanged();
    invalidateGeometry();
}
void DashedGuideItem::setOrigin(QPointF _origin)
{
    if (m_origin == _origin)
        return;
    m_origin = _origin;
    emit originChanged();
    invalidateGeometry();
}
void DashedGuideItem::setPhase(qreal _phase)
{
    if (!std::isfinite(_phase) || m_phase == _phase)
        return;
    m_phase = _phase;
    emit phaseChanged();
    if (hasGeometry())
        update();
}
void DashedGuideItem::invalidateGeometry()
{
    m_dirty = true;
    polish();
    update();
}
void DashedGuideItem::geometryChange(const QRectF &_new, const QRectF &_old)
{
    QQuickItem::geometryChange(_new, _old);
    if (_new.size() != _old.size())
        invalidateGeometry();
}
void DashedGuideItem::itemChange(ItemChange _change, const ItemChangeData &_data)
{
    QQuickItem::itemChange(_change, _data);
    if (_change == ItemDevicePixelRatioHasChanged)
        invalidateGeometry();
}
void DashedGuideItem::updatePolish()
{
    if (!m_dirty)
        return;
    m_dirty                = false;
    const bool hadGeometry = hasGeometry();
    ++m_generation;
    m_mesh         = {};
    m_ratio        = window() ? window()->effectiveDevicePixelRatio() : 1;
    bool supported = m_validPoints && std::isfinite(m_zoom) && m_zoom > 0 && m_ratio >= 1 &&
                     m_ratio <= 8 && width() > 0 && height() > 0 && std::isfinite(m_origin.x()) &&
                     std::isfinite(m_origin.y());
    QPolygonF points;
    if (supported) {
        for (const auto &p : m_points) {
            const auto scaled = p * m_zoom;
            if (!std::isfinite(scaled.x()) || !std::isfinite(scaled.y()) ||
                qAbs(scaled.x()) > 1e8 || qAbs(scaled.y()) > 1e8) {
                supported = false;
                break;
            }
            if (points.isEmpty() || points.last() != scaled)
                points.append(scaled);
        }
        if (points.size() > 1 && points.first() == points.last())
            points.removeLast();
        supported = supported && points.size() >= 3;
    }
    QVector<qreal> setbacks;
    if (supported) {
        setbacks.reserve(points.size());
        for (int i = 0; i < points.size(); ++i) {
            const auto  a    = points[(i + points.size() - 1) % points.size()] - points[i];
            const auto  b    = points[(i + 1) % points.size()] - points[i];
            const qreal norm = std::hypot(a.x(), a.y()) * std::hypot(b.x(), b.y());
            // At acute corners the projected dash can cut across a wide miter
            // differently from QPainter's dashed stroker. Preserve those joins
            // through the raster fallback instead of changing their appearance.
            if (norm <= 1e-12 || QPointF::dotProduct(a, b) > 1e-8 * norm) {
                supported = false;
                break;
            }
            const qreal dot = QPointF::dotProduct(a, b);
            setbacks.append(1.25 * std::sqrt(qMax(0., (norm + dot) / (norm - dot))));
        }
    }
    if (supported) {
        for (int i = 0; i < points.size(); ++i) {
            const int  next  = (i + 1) % int(points.size());
            const auto delta = points[next] - points[i];
            // Do not invert a short inner edge between neighbouring miters.
            if (std::hypot(delta.x(), delta.y()) < setbacks[i] + setbacks[next]) {
                supported = false;
                break;
            }
        }
    }
    if (supported) {
        m_closure = {points.first() - m_origin, points[1] - m_origin, points.last() - m_origin};
        QPainterPath path;
        path.addPolygon(points);
        path.closeSubpath();
        // A ratio of one retains Canvas' logical 1.5 px width and 15 px period.
        // The shader adjusts antialiasing to the real device pixel ratio.
        m_mesh = PhotoCrap::Render::selectionMesh(
            path, {1, 1}, QRectF(m_origin, QSizeF(width(), height())), 1, 65536);
        supported = !m_mesh.fallback;
    }
    if (m_supported != supported) {
        m_supported = supported;
        emit supportedChanged();
    }
    if (hadGeometry != hasGeometry())
        emit hasGeometryChanged();
    update();
}
QSGNode *DashedGuideItem::updatePaintNode(QSGNode *_old, UpdatePaintNodeData *)
{
#ifdef PHOTOCRAP_TESTING
    ++m_renderUpdates;
#endif
    auto      *node     = static_cast<DashedGuideNode *>(_old);
    const bool hardware = window() && QSGRendererInterface::isApiRhiBased(
                                          window()->rendererInterface()->graphicsApi());
    if (!hardware || !m_supported || m_mesh.vertices.isEmpty()) {
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
        node = new DashedGuideNode(window());
    const bool geometry   = node->setMesh(m_mesh.vertices, m_generation, m_closure);
    const bool parameters = node->setParameters(m_phase, m_ratio);
#ifdef PHOTOCRAP_TESTING
    m_shaderActive = true;
    if (geometry)
        m_geometryUploads += 2;
    if (parameters)
        m_parameterUpdates += 2;
#else
    Q_UNUSED(geometry)
    Q_UNUSED(parameters)
#endif
    return node;
}
