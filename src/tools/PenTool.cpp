#include "Document.h"
#include <QLineF>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace
{
QPointF snapAngle(const QPointF &_origin, const QPointF &_point)
{
    const QPointF d      = _point - _origin;
    const qreal   angle  = std::round(std::atan2(d.y(), d.x()) / (M_PI / 4)) * (M_PI / 4);
    const qreal   length = std::hypot(d.x(), d.y());
    return _origin + QPointF(std::cos(angle) * length, std::sin(angle) * length);
}
bool validPoint(qreal _x, qreal _y)
{
    return std::isfinite(_x) && std::isfinite(_y) && std::abs(_x) <= 1e6 && std::abs(_y) <= 1e6;
}
int pointCount(const QVector<PenPath> &_paths)
{
    int count = 0;
    for (const auto &path : _paths)
        count += int(path.points.size());
    return count;
}
// Liang-Barsky clipping retains the true line direction at the canvas edge.
// Off-canvas handles must not produce millions of invisible brush dabs.
bool clipLine(QLineF &_line, const QRectF &_bounds)
{
    const QPointF start = _line.p1(), delta = _line.p2() - start;
    qreal         lo = 0, hi = 1;
    const qreal   p[4] = {-delta.x(), delta.x(), -delta.y(), delta.y()};
    const qreal   q[4] = {start.x() - _bounds.left(), _bounds.right() - start.x(),
                          start.y() - _bounds.top(), _bounds.bottom() - start.y()};
    for (int i = 0; i < 4; ++i) {
        if (qAbs(p[i]) < 1e-12) {
            if (q[i] < 0)
                return false;
            continue;
        }
        const qreal t = q[i] / p[i];
        if (p[i] < 0)
            lo = qMax(lo, t);
        else
            hi = qMin(hi, t);
        if (lo > hi)
            return false;
    }
    _line = QLineF(start + lo * delta, start + hi * delta);
    return true;
}
} // namespace

QVariantList Document::penPaths() const
{
    QVariantList paths;
    for (int i = 0; i < m_penPaths.size(); ++i) {
        const auto  &path = m_penPaths[i];
        QVariantList points;
        for (int j = 0; j < path.points.size(); ++j) {
            const auto &p = path.points[j];
            points.append(
                QVariantMap{{"x", p.position.x()},
                            {"y", p.position.y()},
                            {"inX", p.incoming.x()},
                            {"inY", p.incoming.y()},
                            {"outX", p.outgoing.x()},
                            {"outY", p.outgoing.y()},
                            {"smooth", p.smooth},
                            {"selected", i == m_activePenPath && m_selectedPenPoints.contains(j)}});
        }
        paths.append(QVariantMap{{"name", path.name}, {"closed", path.closed}, {"points", points}});
    }
    return paths;
}
void Document::setPenRubberBand(bool _value)
{
    if (m_penRubberBand != _value) {
        m_penRubberBand = _value;
        emit penOptionsChanged();
    }
}
void Document::setPenAutoAddDelete(bool _value)
{
    if (m_penAutoAddDelete != _value) {
        m_penAutoAddDelete = _value;
        emit penOptionsChanged();
    }
}
void Document::commitPenEdit(const Snapshot &_before, const QString &_label)
{
    if (_before.penPaths == m_penPaths) {
        emit penPathsChanged();
        return;
    }
    auto before    = _before;
    before.label   = _label;
    before.penOnly = true;
    m_undo.append(before);
    m_redo.clear();
    m_stateId = m_contentId = m_nextId++;
    trimHistory();
    emit penPathsChanged();
    emit historyChanged();
}
void Document::setActivePenPath(int _index)
{
    endPenGesture();
    m_penDrawing    = false;
    m_activePenPath = _index >= 0 && _index < m_penPaths.size() ? _index : -1;
    m_selectedPenPoints.clear();
    emit penPathsChanged();
}
void Document::newPenPath()
{
    setActivePenPath(-1);
}
void Document::renamePenPath(const QString &_name)
{
    endPenGesture();
    const QString name = _name.trimmed().left(256);
    if (m_activePenPath < 0 || name.isEmpty() || m_penPaths[m_activePenPath].name == name)
        return;
    const auto before                = snapshot({});
    m_penPaths[m_activePenPath].name = name;
    commitPenEdit(before, tr("Renommer le tracé"));
}
void Document::deletePenPath()
{
    endPenGesture();
    if (m_activePenPath < 0)
        return;
    const auto before = snapshot({});
    m_penPaths.removeAt(m_activePenPath);
    m_activePenPath = qMin(m_activePenPath, int(m_penPaths.size()) - 1);
    m_penDrawing    = false;
    m_selectedPenPoints.clear();
    commitPenEdit(before, tr("Supprimer le tracé"));
}
void Document::deletePenPoints()
{
    endPenGesture();
    if (m_activePenPath < 0)
        return;
    const auto before = snapshot({});
    auto      &path   = m_penPaths[m_activePenPath];
    if (m_selectedPenPoints.isEmpty() && m_penDrawing && !path.points.isEmpty())
        m_selectedPenPoints.insert(int(path.points.size()) - 1);
    if (m_selectedPenPoints.isEmpty())
        return;
    auto selected = m_selectedPenPoints.values();
    std::sort(selected.begin(), selected.end(), std::greater<int>());
    for (int i : selected)
        if (i >= 0 && i < path.points.size())
            path.points.removeAt(i);
    if (path.points.size() < 2)
        path.closed = false;
    if (path.points.isEmpty()) {
        m_penPaths.removeAt(m_activePenPath);
        m_activePenPath = -1;
        m_penDrawing    = false;
    }
    m_selectedPenPoints.clear();
    if (m_penDrawing && m_activePenPath >= 0)
        m_selectedPenPoints.insert(int(m_penPaths[m_activePenPath].points.size()) - 1);
    commitPenEdit(before, tr("Supprimer un point d’ancrage"));
}
void Document::nudgePenPoints(int _dx, int _dy)
{
    endPenGesture();
    if (m_activePenPath < 0 || m_selectedPenPoints.isEmpty())
        return;
    const auto before = snapshot({});
    auto      &points = m_penPaths[m_activePenPath].points;
    for (int i : m_selectedPenPoints)
        if (i >= 0 && i < points.size()) {
            auto         &p = points[i];
            const QPointF d(_dx, _dy);
            if (!validPoint((p.position + d).x(), (p.position + d).y()) ||
                !validPoint((p.incoming + d).x(), (p.incoming + d).y()) ||
                !validPoint((p.outgoing + d).x(), (p.outgoing + d).y()))
                return;
        }
    for (int i : m_selectedPenPoints)
        if (i >= 0 && i < points.size()) {
            auto         &p = points[i];
            const QPointF d(_dx, _dy);
            p.position += d;
            p.incoming += d;
            p.outgoing += d;
        }
    commitPenEdit(before, tr("Déplacer les points d’ancrage"));
}
void Document::finishPenPath()
{
    endPenGesture();
    m_penDrawing = false;
    emit penPathsChanged();
}
void Document::closePenPath()
{
    endPenGesture();
    if (m_activePenPath < 0 || m_penPaths[m_activePenPath].points.size() < 2)
        return;
    const auto before                  = snapshot({});
    m_penPaths[m_activePenPath].closed = true;
    m_penDrawing                       = false;
    commitPenEdit(before, tr("Fermer le tracé"));
}
QPair<int, int> Document::hitPenPoint(const QPointF &_point, qreal _tolerance) const
{
    if (m_activePenPath < 0)
        return {-1, 0};
    const auto &points = m_penPaths[m_activePenPath].points;
    for (int i = 0; i < points.size(); ++i)
        if (QLineF(points[i].position, _point).length() <= _tolerance)
            return {i, 0};
    for (int i : m_selectedPenPoints)
        if (i >= 0 && i < points.size()) {
            if (points[i].incoming != points[i].position &&
                QLineF(points[i].incoming, _point).length() <= _tolerance)
                return {i, 1};
            if (points[i].outgoing != points[i].position &&
                QLineF(points[i].outgoing, _point).length() <= _tolerance)
                return {i, 2};
        }
    return {-1, 0};
}
QString Document::penCursorAt(qreal _x, qreal _y, qreal _tolerance, bool _direct,
                              bool _alternate) const
{
    if (!validPoint(_x, _y) || !std::isfinite(_tolerance))
        return "penStart";
    const auto hit = hitPenPoint(QPointF(_x, _y), _tolerance);
    if (_direct)
        return "penMove";
    if (_alternate || m_tool == "convertPoint")
        return "penConvert";
    if (m_tool == "addAnchor")
        return "penAdd";
    if (m_tool == "deleteAnchor")
        return "penDelete";
    if (hit.first >= 0) {
        if (hit.second > 0)
            return "penMove";
        const auto &path = m_penPaths[m_activePenPath];
        if (m_penDrawing && hit.first == 0 && path.points.size() > 1)
            return "penClose";
        if (m_penDrawing && hit.first == path.points.size() - 1)
            return "penConvert";
        if (!path.closed && !m_penDrawing &&
            (hit.first == 0 || hit.first == path.points.size() - 1))
            return "penResume";
        if (m_tool == "deleteAnchor" || (m_penAutoAddDelete && m_tool == "pen"))
            return "penDelete";
    }
    if (m_activePenPath >= 0 && (m_penAutoAddDelete || m_tool == "addAnchor") &&
        PenGeometry::nearestSegment(m_penPaths[m_activePenPath], QPointF(_x, _y), _tolerance)
                .index >= 0)
        return "penAdd";
    return m_penDrawing ? "pen" : "penStart";
}
void Document::beginPenGesture(qreal _x, qreal _y, qreal _tolerance, bool _direct, bool _alternate,
                               bool _shift)
{
    cancelSelectionOperation();
    endStroke();
    if (!validPoint(_x, _y) || !std::isfinite(_tolerance) || _tolerance <= 0)
        return;
    QPointF point(_x, _y);
    m_penHover           = point;
    m_beforePenGesture   = snapshot({});
    m_penWasDrawing      = m_penDrawing;
    m_penSelectionBefore = m_selectedPenPoints;
    m_penOrigin          = point;
    m_penLastPointer     = point;
    m_penBroken          = false;
    m_penGesture         = 7;
    const auto hit       = hitPenPoint(point, _tolerance);
    const bool convert   = _alternate || m_tool == "convertPoint";
    if (hit.first >= 0) {
        m_penPoint = hit.first;
        auto &path = m_penPaths[m_activePenPath];
        auto &node = path.points[m_penPoint];
        if (_direct || hit.second > 0) {
            if (_direct && _shift && hit.second == 0 && m_selectedPenPoints.contains(m_penPoint)) {
                m_selectedPenPoints.remove(m_penPoint);
                emit penPathsChanged();
                return;
            }
            if (!_shift && !m_selectedPenPoints.contains(m_penPoint))
                m_selectedPenPoints.clear();
            m_selectedPenPoints.insert(m_penPoint);
            m_penGesture = hit.second == 1 ? 3 : hit.second == 2 ? 4 : 2;
            if (_alternate && hit.second > 0)
                node.smooth = false;
        } else if (convert) {
            m_selectedPenPoints = {m_penPoint};
            node.incoming = node.outgoing = node.position;
            node.smooth                   = false;
            m_penGesture                  = 5;
        } else if (m_tool == "pen" && m_penDrawing && m_penPoint == 0 && path.points.size() > 1) {
            path.closed         = true;
            m_penDrawing        = false;
            m_selectedPenPoints = {0};
            m_penGesture        = 6;
        } else if (m_tool == "pen" && !m_penDrawing && !path.closed &&
                   (m_penPoint == 0 || m_penPoint == path.points.size() - 1)) {
            if (m_penPoint == 0) {
                std::reverse(path.points.begin(), path.points.end());
                for (auto &p : path.points)
                    std::swap(p.incoming, p.outgoing);
            }
            m_penPoint          = int(path.points.size()) - 1;
            m_selectedPenPoints = {m_penPoint};
            m_penDrawing        = true;
        } else if (m_tool == "pen" && m_penDrawing && m_penPoint == path.points.size() - 1) {
            node.outgoing = node.position;
            node.smooth   = false;
        } else if (m_tool == "deleteAnchor" || (m_tool == "pen" && m_penAutoAddDelete)) {
            path.points.removeAt(m_penPoint);
            m_selectedPenPoints.clear();
            if (path.points.size() < 2)
                path.closed = false;
            if (path.points.isEmpty()) {
                m_penPaths.removeAt(m_activePenPath);
                m_activePenPath = -1;
                m_penDrawing    = false;
            }
        }
    } else if (_direct) {
        m_penDrawing = false;
        m_selectedPenPoints.clear();
    } else {
        bool inserted = false;
        if (m_activePenPath >= 0 && !convert && m_tool != "deleteAnchor" &&
            (m_tool == "addAnchor" || (m_tool == "pen" && m_penAutoAddDelete))) {
            auto      &path    = m_penPaths[m_activePenPath];
            const auto segment = PenGeometry::nearestSegment(path, point, _tolerance);
            if (segment.index >= 0) {
                if (path.points.size() >= PenGeometry::MaxPoints ||
                    pointCount(m_penPaths) >= PenGeometry::MaxTotalPoints) {
                    endPenGesture(true);
                    fail(tr("Limite de points du tracé atteinte."));
                    return;
                }
                m_penPoint          = PenGeometry::splitSegment(path, segment.index, segment.t);
                m_selectedPenPoints = {m_penPoint};
                inserted            = true;
            }
        }
        if (!inserted && m_tool == "pen" && !convert) {
            if (pointCount(m_penPaths) >= PenGeometry::MaxTotalPoints ||
                (m_penDrawing && m_activePenPath >= 0 &&
                 m_penPaths[m_activePenPath].points.size() >= PenGeometry::MaxPoints)) {
                endPenGesture(true);
                fail(tr("Limite de points du tracé atteinte."));
                return;
            }
            if (!m_penDrawing || m_activePenPath < 0) {
                if (m_penPaths.size() >= PenGeometry::MaxPaths) {
                    endPenGesture(true);
                    fail(tr("Limite de tracés atteinte."));
                    return;
                }
                m_penPaths.append({tr("Tracé %1").arg(m_penPaths.size() + 1), {}, false});
                m_activePenPath = int(m_penPaths.size()) - 1;
            }
            auto &path = m_penPaths[m_activePenPath];
            if (_shift && !path.points.isEmpty())
                point = snapAngle(path.points.last().position, point);
            if (!validPoint(point.x(), point.y())) {
                endPenGesture(true);
                return;
            }
            path.points.append({point, point, point, false});
            m_penPoint          = int(path.points.size()) - 1;
            m_selectedPenPoints = {m_penPoint};
            m_penDrawing        = true;
            m_penGesture        = 1;
        }
    }
    emit penPathsChanged();
    emit penHoverChanged();
}
void Document::updatePenGesture(qreal _x, qreal _y, bool _shift, bool _alternate, bool _space)
{
    if (!validPoint(_x, _y))
        return;
    m_penHover = QPointF(_x, _y);
    emit penHoverChanged();
    if (!m_penGesture || m_activePenPath < 0 || m_penPoint < 0 ||
        m_penPoint >= m_penPaths[m_activePenPath].points.size())
        return;
    auto      &path           = m_penPaths[m_activePenPath];
    const auto originalPoints = path.points;
    const auto originalOrigin = m_penOrigin;
    auto      &p              = path.points[m_penPoint];
    QPointF    point(_x, _y);
    if (m_penGesture == 2) {
        if (_shift)
            point = snapAngle(m_penOrigin, point);
        const auto &before = m_beforePenGesture.penPaths[m_activePenPath];
        const auto  delta  = point - m_penOrigin;
        for (int i : m_selectedPenPoints)
            if (i >= 0 && i < before.points.size()) {
                path.points[i] = before.points[i];
                auto &node     = path.points[i];
                node.position += delta;
                node.incoming += delta;
                node.outgoing += delta;
            }
    } else if (m_penGesture == 1 || m_penGesture == 5 || m_penGesture == 6) {
        if (_space) {
            const auto delta = point - m_penLastPointer;
            p.position += delta;
            p.incoming += delta;
            p.outgoing += delta;
        } else {
            if (_shift)
                point = snapAngle(p.position, point);
            if (QLineF(point, p.position).length() > .001) {
                p.outgoing = point;
                if (_alternate && m_penGesture != 5)
                    m_penBroken = true;
                if (!m_penBroken || m_penGesture == 5) {
                    p.incoming = 2 * p.position - point;
                    p.smooth   = true;
                } else
                    p.smooth = false;
            }
        }
    } else if (m_penGesture == 3 || m_penGesture == 4) {
        if (_shift)
            point = snapAngle(p.position, point);
        QPointF    &handle    = m_penGesture == 3 ? p.incoming : p.outgoing;
        QPointF    &other     = m_penGesture == 3 ? p.outgoing : p.incoming;
        const qreal length    = QLineF(p.position, other).length(),
                    newLength = QLineF(p.position, point).length();
        handle                = point;
        if (_alternate)
            p.smooth = false;
        if (p.smooth && newLength > .001)
            other = p.position - (point - p.position) * (length / newLength);
    }
    for (const auto &node : path.points)
        if (!validPoint(node.position.x(), node.position.y()) ||
            !validPoint(node.incoming.x(), node.incoming.y()) ||
            !validPoint(node.outgoing.x(), node.outgoing.y())) {
            path.points = originalPoints;
            m_penOrigin = originalOrigin;
            return;
        }
    m_penLastPointer = QPointF(_x, _y);
    emit penPathsChanged();
}
void Document::endPenGesture(bool _cancel)
{
    if (!m_penGesture)
        return;
    m_penGesture = 0;
    if (_cancel) {
        m_penPaths          = m_beforePenGesture.penPaths;
        m_activePenPath     = m_beforePenGesture.activePenPath;
        m_selectedPenPoints = m_penSelectionBefore;
        m_penDrawing        = m_penWasDrawing;
        emit penPathsChanged();
    } else
        commitPenEdit(m_beforePenGesture, tr("Modifier le tracé"));
    m_beforePenGesture = {};
}
void Document::updatePenHover(qreal _x, qreal _y)
{
    if (validPoint(_x, _y) && m_penHover != QPointF(_x, _y)) {
        m_penHover = QPointF(_x, _y);
        emit penHoverChanged();
    }
}
void Document::penPathToSelection(int _mode)
{
    finishPenPath();
    if (m_activePenPath < 0 || _mode < 0 || _mode > 3)
        return;
    const auto path = m_penPaths[m_activePenPath].painterPath();
    if (path.boundingRect().isEmpty())
        return;
    const auto region = QRegion(path.toFillPolygon().toPolygon(), Qt::OddEvenFill) &
                        QRegion(QRect(QPoint{}, m_size));
    const auto before = snapshot({});
    if (_mode == 1)
        m_selectionRegion |= region;
    else if (_mode == 2)
        m_selectionRegion -= region;
    else if (_mode == 3)
        m_selectionRegion &= region;
    else
        m_selectionRegion = region;
    combineSoftSelection(region, _mode, before);
    m_selection      = m_selectionRegion.boundingRect();
    m_selectionShape = 6;
    m_selectionPath.clear();
    m_polygonLassoActive = false;
    // Keep the saved path, but leave path editing so only the selection is shown.
    setActivePenPath(-1);
    commitSelectionEdit(before, tr("Sélection depuis le tracé"));
    emit selectionChanged();
}
void Document::fillPenPath()
{
    finishPenPath();
    if (m_activePenPath < 0 || !editable())
        return;
    const auto path = m_penPaths[m_activePenPath].painterPath();
    if (path.boundingRect().isEmpty() || m_brushOpacity <= 0 || m_color.alpha() == 0)
        return;
    QRect bounds = path.boundingRect()
                       .toAlignedRect()
                       .adjusted(-1, -1, 1, 1)
                       .intersected(QRect(QPoint{}, m_size));
    if (hasSelection())
        bounds = bounds.intersected(m_selectionRegion.boundingRect());
    if (bounds.isEmpty() || path.isEmpty())
        return;
    const auto before = snapshot({});
    QImage    *target = paintTarget();
    if (!target)
        return;
    QPainter painter(target);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(bounds);
    if (hasSelection())
        painter.setClipRegion(m_selectionRegion, Qt::IntersectClip);
    painter.setOpacity(m_brushOpacity);
    painter.fillPath(path, m_color);
    painter.end();
    SelectionCoverage::blend(
        *target, m_editingMask ? before.layers[m_active].mask : before.layers[m_active].image,
        m_selectionMask, bounds);
    m_dirtyRegion = m_dirtyRegion.united(bounds);
    commit(before, tr("Remplir le tracé"), false);
    ++m_thumbnailRevision;
    emit thumbnailsChanged();
}
void Document::strokePenPath()
{
    finishPenPath();
    if (m_activePenPath < 0 || !editable())
        return;
    const auto  polygons = m_penPaths[m_activePenPath].painterPath().toSubpathPolygons();
    const qreal margin   = m_brushSize / 2 + 3;
    QRectF      bounds   = QRectF(QPointF{}, m_size);
    if (hasSelection())
        bounds = bounds.intersected(QRectF(m_selectionRegion.boundingRect()));
    bounds.adjust(-margin, -margin, margin, margin);
    QVector<QLineF> segments;
    for (const auto &polygon : polygons)
        for (int i = 1; i < polygon.size(); ++i) {
            QLineF line(polygon[i - 1], polygon[i]);
            if (clipLine(line, bounds))
                segments.append(line);
        }
    if (segments.isEmpty())
        return;
    const QString tool      = m_tool;
    const qreal   smoothing = m_brushSmoothing;
    const auto    state     = m_stateId;
    m_brushSmoothing        = 0;
    setTool("brush");
    beginStroke(segments.first().x1(), segments.first().y1());
    for (const auto &line : segments) {
        if (QLineF(m_last, line.p1()).length() > .001) {
            m_smoothedLast = m_last = line.p1();
            m_distanceToNextDab     = qMax(1., m_brushSize * m_brushSpacing);
            m_strokeBounds          = {};
            stampBrush(line.p1());
            renderBrushStroke();
            changed(false);
        }
        continueStroke(line.x2(), line.y2());
    }
    endStroke();
    m_brushSmoothing = smoothing;
    setTool(tool);
    if (m_stateId != state && !m_undo.isEmpty()) {
        m_undo.last().label = tr("Contour du tracé");
        emit historyChanged();
    }
}
