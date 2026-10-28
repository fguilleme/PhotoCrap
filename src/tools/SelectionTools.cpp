#include "Document.h"

#include <QLineF>
#include <QtMath>
#include <cmath>

namespace
{
QPointF boundedPoint(const QSize &_size, qreal _x, qreal _y)
{
    return {qBound(0., _x, qreal(qMax(0, _size.width() - 1))),
            qBound(0., _y, qreal(qMax(0, _size.height() - 1)))};
}

QRegion polygonRegion(const QPolygonF &_path, const QSize &_size)
{
    if (_path.size() < 3)
        return {};
    return QRegion(_path.toPolygon(), Qt::WindingFill) & QRegion(QRect(QPoint{}, _size));
}
} // namespace

QVariantList Document::selectionPath() const
{
    QVariantList result;
    result.reserve(m_selectionPath.size());
    for (const QPointF &point : m_selectionPath)
        result.append(point);
    return result;
}

void Document::beginLasso(qreal _x, qreal _y)
{
    rememberSelection();
    m_drawing            = true;
    m_selection          = {};
    m_selectionRegion    = {};
    m_selectionShape     = 4;
    m_selectionPath      = {boundedPoint(m_size, _x, _y)};
    m_polygonLassoActive = false;
    emit selectionChanged();
}

void Document::continueLasso(qreal _x, qreal _y)
{
    const QPointF point = boundedPoint(m_size, _x, _y);
    if (!m_selectionPath.isEmpty() && QLineF(m_selectionPath.constLast(), point).length() < 1.5)
        return;
    m_selectionPath.append(point);
    m_selectionRegion   = polygonRegion(m_selectionPath, m_size);
    m_selection         = m_selectionRegion.boundingRect();
    m_selectionCombined = false;
    combineGestureSelection();
    emit selectionChanged();
}

void Document::finishLasso()
{
    m_selectionRegion = polygonRegion(m_selectionPath, m_size);
    m_selection       = m_selectionRegion.boundingRect();
    if (m_selectionRegion.isEmpty()) {
        m_selectionShape = 0;
        m_selectionPath.clear();
    }
    m_selectionCombined = false;
    combineGestureSelection();
    emit selectionChanged();
    finishSelectionGesture();
}

void Document::addPolygonLassoPoint(qreal _x, qreal _y)
{
    if (m_tool != QStringLiteral("polygonalLasso") && !(m_tool == "lasso" && m_polygonLassoActive))
        return;
    QPointF point = boundedPoint(m_size, _x, _y);
    if (m_toolShift && m_polygonLassoActive && !m_selectionPath.isEmpty()) {
        const QPointF anchor = m_selectionPath.constLast();
        const QPointF delta  = point - anchor;
        const qreal   angle  = std::round(std::atan2(delta.y(), delta.x()) / M_PI_4) * M_PI_4;
        point =
            boundedPoint(m_size, anchor.x() + std::cos(angle) * std::hypot(delta.x(), delta.y()),
                         anchor.y() + std::sin(angle) * std::hypot(delta.x(), delta.y()));
    }
    if (!m_polygonLassoActive) {
        rememberSelection();
        m_selection       = {};
        m_selectionRegion = {};
        m_selectionShape  = 5;
        m_selectionPath.clear();
        m_polygonLassoActive = true;
    } else if (m_selectionPath.size() >= 3 &&
               QLineF(m_selectionPath.constFirst(), point).length() <= 5.) {
        finishPolygonLasso();
        return;
    }
    if (m_selectionPath.isEmpty() || QLineF(m_selectionPath.constLast(), point).length() >= .5)
        m_selectionPath.append(point);
    emit selectionChanged();
}

void Document::continuePolygonLasso(qreal _x, qreal _y)
{
    if (!m_polygonLassoActive || !std::isfinite(_x) || !std::isfinite(_y))
        return;
    const QPointF point = boundedPoint(m_size, _x, _y);
    if (m_selectionPath.isEmpty() || QLineF(m_selectionPath.constLast(), point).length() >= 1.5) {
        m_selectionPath.append(point);
        emit selectionChanged();
    }
}

void Document::finishPolygonLasso()
{
    if (!m_polygonLassoActive)
        return;
    m_polygonLassoActive = false;
    m_selectionRegion    = polygonRegion(m_selectionPath, m_size);
    m_selection          = m_selectionRegion.boundingRect();
    if (m_selectionRegion.isEmpty()) {
        m_selectionShape = 0;
        m_selectionPath.clear();
    }
    m_selectionCombined = false;
    combineGestureSelection();
    emit selectionChanged();
    finishSelectionGesture();
}

void Document::removeLastPolygonLassoPoint()
{
    if (!m_polygonLassoActive || m_selectionPath.isEmpty())
        return;
    m_selectionPath.removeLast();
    if (m_selectionPath.isEmpty()) {
        m_polygonLassoActive = false;
        m_selectionShape     = 0;
    }
    emit selectionChanged();
}

void Document::cancelPolygonLasso()
{
    if (!m_polygonLassoActive)
        return;
    m_polygonLassoActive = false;
    restoreGestureSelection();
}
