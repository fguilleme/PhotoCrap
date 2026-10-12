#include "Document.h"
#include <QLineF>
#include <QtMath>
#include <cmath>

void Document::setToolModifiers(bool _shift, bool _alternate, bool _space)
{
    m_toolShift     = _shift;
    m_toolAlternate = _alternate;
    m_toolSpace     = _space;
}
QPointF Document::constrainedToolPoint(const QPointF &_point, bool _diagonal) const
{
    if (!m_toolShift)
        return _point;
    const QPointF delta = _point - m_anchor;
    if (!_diagonal)
        return qAbs(delta.x()) >= qAbs(delta.y()) ? QPointF(_point.x(), m_anchor.y())
                                                  : QPointF(m_anchor.x(), _point.y());
    const qreal angle  = std::round(std::atan2(delta.y(), delta.x()) / M_PI_4) * M_PI_4;
    const qreal length = std::hypot(delta.x(), delta.y());
    return m_anchor + QPointF(std::cos(angle), std::sin(angle)) * length;
}
QRectF Document::toolRectangle(const QPointF &_anchor, const QPointF &_point, bool _center) const
{
    QPointF delta = _point - _anchor;
    if (m_toolShift) {
        const qreal side = qMax(qAbs(delta.x()), qAbs(delta.y()));
        delta            = QPointF(delta.x() < 0 ? -side : side, delta.y() < 0 ? -side : side);
    }
    return QRectF(_center ? _anchor - delta : _anchor, _anchor + delta).normalized();
}
void Document::rememberSelection()
{
    m_selectionCombined           = false;
    m_selectionRegionBeforeStroke = m_selectionRegion;
    m_selectionBoundsBeforeStroke = m_selection;
    m_selectionShapeBeforeStroke  = m_selectionShape;
    m_selectionPathBeforeStroke   = m_selectionPath;
}
void Document::restoreGestureSelection()
{
    m_selectionRegion = m_selectionRegionBeforeStroke;
    m_selection       = m_selectionBoundsBeforeStroke;
    m_selectionShape  = m_selectionShapeBeforeStroke;
    m_selectionPath   = m_selectionPathBeforeStroke;
    emit selectionChanged();
}
void Document::combineGestureSelection()
{
    if (!m_selectionMode || m_selectionCombined)
        return;
    m_selectionCombined     = true;
    const QRegion candidate = m_selectionRegion;
    if (m_selectionMode == 1)
        m_selectionRegion = m_selectionRegionBeforeStroke.united(candidate);
    if (m_selectionMode == 2)
        m_selectionRegion = m_selectionRegionBeforeStroke.subtracted(candidate);
    if (m_selectionMode == 3)
        m_selectionRegion = m_selectionRegionBeforeStroke.intersected(candidate);
    m_selection      = m_selectionRegion.boundingRect();
    m_selectionShape = m_selectionRegion.isEmpty() ? 0 : 6;
    emit selectionChanged();
}
void Document::nudgeTool(int _dx, int _dy, bool _copy)
{
    if (m_drawing || m_cropBusy || m_sliceBusy || qAbs(_dx) > 10 || qAbs(_dy) > 10)
        return;
    if (m_tool == "crop" || m_tool == "perspectiveCrop") {
        if (!hasCrop())
            return;
        const QRectF  bounds = m_cropPoints.boundingRect();
        const QPointF delta(qBound(-bounds.left(), qreal(_dx), width() - bounds.right()),
                            qBound(-bounds.top(), qreal(_dy), height() - bounds.bottom()));
        m_cropPoints.translate(delta);
        emit cropChanged();
    } else if (m_tool == "sliceSelect") {
        if (m_activeSlice < 0 || m_activeSlice >= m_slices.size())
            return;
        const QRect rect = m_slices[m_activeSlice];
        setActiveSliceRect(qBound(0, rect.x() + _dx, width() - rect.width()),
                           qBound(0, rect.y() + _dy, height() - rect.height()), rect.width(),
                           rect.height());
    } else if (m_tool == "move") {
        beginStroke(0, 0, _copy);
        continueStroke(_dx, _dy);
        endStroke();
    } else if (hasSelection() &&
               (m_tool == "select" || m_tool == "ellipticalSelect" || m_tool == "singleRowSelect" ||
                m_tool == "singleColumnSelect" || m_tool == "lasso" || m_tool == "polygonalLasso" ||
                m_tool == "magicWand" || m_tool == "quickSelection")) {
        const QPoint delta(qBound(-m_selection.left(), _dx, width() - 1 - m_selection.right()),
                           qBound(-m_selection.top(), _dy, height() - 1 - m_selection.bottom()));
        m_selection.translate(delta);
        m_selectionRegion.translate(delta);
        m_selectionPath.translate(delta);
        emit selectionChanged();
    }
}
