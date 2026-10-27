#include "PenPath.h"
#include <QJsonObject>
#include <QLineF>
#include <cmath>
#include <limits>

namespace
{
QPointF mix(const QPointF &_a, const QPointF &_b, qreal _t)
{
    return _a * (1 - _t) + _b * _t;
}
QPointF cubic(const PenPoint &_a, const PenPoint &_b, qreal _t)
{
    const qreal u = 1 - _t;
    return _a.position * (u * u * u) + _a.outgoing * (3 * u * u * _t) +
           _b.incoming * (3 * u * _t * _t) + _b.position * (_t * _t * _t);
}
} // namespace
QPainterPath PenPath::painterPath() const
{
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);
    if (points.isEmpty())
        return path;
    path.moveTo(points.first().position);
    for (int i = 1; i < points.size(); ++i)
        path.cubicTo(points[i - 1].outgoing, points[i].incoming, points[i].position);
    if (closed && points.size() > 1) {
        path.cubicTo(points.last().outgoing, points.first().incoming, points.first().position);
        path.closeSubpath();
    }
    return path;
}
PenGeometry::SegmentHit PenGeometry::nearestSegment(const PenPath &_path, const QPointF &_point,
                                                    qreal _tolerance)
{
    SegmentHit hit{-1, 0, _tolerance};
    const int  count = int(_path.points.size()) - (_path.closed ? 0 : 1);
    for (int i = 0; i < count; ++i) {
        const auto  &a = _path.points[i], &b = _path.points[(i + 1) % _path.points.size()];
        const QRectF bounds = QRectF(a.position, b.position)
                                  .normalized()
                                  .united(QRectF(a.outgoing, b.incoming).normalized())
                                  .adjusted(-_tolerance, -_tolerance, _tolerance, _tolerance);
        if (!bounds.contains(_point))
            continue;
        qreal best = std::numeric_limits<qreal>::max(), t = 0;
        for (int j = 0; j <= 32; ++j) {
            const qreal candidate = qreal(j) / 32,
                        distance  = QLineF(cubic(a, b, candidate), _point).length();
            if (distance < best) {
                best = distance;
                t    = candidate;
            }
        }
        qreal left = qMax(0., t - 1. / 32), right = qMin(1., t + 1. / 32);
        for (int j = 0; j < 20; ++j) {
            const qreal l = (2 * left + right) / 3, r = (left + 2 * right) / 3;
            if (QLineF(cubic(a, b, l), _point).length() < QLineF(cubic(a, b, r), _point).length())
                right = r;
            else
                left = l;
        }
        t    = (left + right) / 2;
        best = QLineF(cubic(a, b, t), _point).length();
        if (best < hit.distance)
            hit = {i, t, best};
    }
    return hit;
}
int PenGeometry::splitSegment(PenPath &_path, int _index, qreal _t)
{
    if (_index < 0 || _index >= _path.points.size() || _path.points.size() >= MaxPoints ||
        (!_path.closed && _index + 1 >= _path.points.size()))
        return -1;
    const int  next = (_index + 1) % int(_path.points.size());
    auto      &a = _path.points[_index], &b = _path.points[next];
    const auto ab = mix(a.position, a.outgoing, _t), bc = mix(a.outgoing, b.incoming, _t),
               cd = mix(b.incoming, b.position, _t), abc = mix(ab, bc, _t), bcd = mix(bc, cd, _t),
               point = mix(abc, bcd, _t);
    a.outgoing       = ab;
    b.incoming       = cd;
    _path.points.insert(_index + 1, {point, abc, bcd, true});
    return _index + 1;
}
QJsonArray PenGeometry::encode(const QVector<PenPath> &_paths)
{
    QJsonArray paths;
    for (const auto &path : _paths) {
        QJsonArray points;
        for (const auto &p : path.points)
            points.append(QJsonArray{p.position.x(), p.position.y(), p.incoming.x(), p.incoming.y(),
                                     p.outgoing.x(), p.outgoing.y(), p.smooth});
        paths.append(QJsonObject{{"name", path.name}, {"closed", path.closed}, {"points", points}});
    }
    return paths;
}
bool PenGeometry::decode(const QJsonArray &_array, QVector<PenPath> &_paths)
{
    if (_array.size() > MaxPaths)
        return false;
    QVector<PenPath> result;
    int              total = 0;
    for (const auto &value : _array) {
        if (!value.isObject())
            return false;
        const auto object = value.toObject();
        if (!object["name"].isString() || !object["closed"].isBool() || !object["points"].isArray())
            return false;
        PenPath path;
        path.name         = object["name"].toString();
        path.closed       = object["closed"].toBool();
        const auto points = object["points"].toArray();
        total += int(points.size());
        if (path.name.isEmpty() || path.name.size() > 256 || points.isEmpty() ||
            points.size() > MaxPoints || total > MaxTotalPoints)
            return false;
        for (const auto &entry : points) {
            if (!entry.isArray())
                return false;
            const auto p = entry.toArray();
            if (p.size() != 7 || !p[6].isBool())
                return false;
            for (int i = 0; i < 6; ++i)
                if (!p[i].isDouble() || !std::isfinite(p[i].toDouble()) ||
                    std::abs(p[i].toDouble()) > 1e6)
                    return false;
            path.points.append({QPointF(p[0].toDouble(), p[1].toDouble()),
                                QPointF(p[2].toDouble(), p[3].toDouble()),
                                QPointF(p[4].toDouble(), p[5].toDouble()), p[6].toBool()});
        }
        if (path.closed && path.points.size() < 2)
            return false;
        result.append(path);
    }
    _paths = result;
    return true;
}
bool PenGeometry::transform(QVector<PenPath> &_paths, const QTransform &_transform)
{
    auto result = _paths;
    int  total  = 0;
    for (auto &path : result) {
        if (!_transform.isAffine()) {
            int sign = 0;
            for (const auto &node : path.points)
                for (const QPointF point : {node.position, node.incoming, node.outgoing}) {
                    const qreal w = _transform.m13() * point.x() + _transform.m23() * point.y() +
                                    _transform.m33();
                    const auto  mappedPoint = _transform.map(point);
                    if (std::abs(w) < 1e-6 || (sign && (w > 0 ? 1 : -1) != sign) ||
                        !std::isfinite(mappedPoint.x()) || !std::isfinite(mappedPoint.y()) ||
                        std::abs(mappedPoint.x()) > 1e6 || std::abs(mappedPoint.y()) > 1e6)
                        return false;
                    sign = w > 0 ? 1 : -1;
                }
            // Map the complete path: projective images of cubic Beziers are
            // rational curves, not cubics with simply mapped control points.
            const auto        mapped = _transform.map(path.painterPath());
            QVector<PenPoint> points;
            for (int i = 0; i < mapped.elementCount(); ++i) {
                const auto    element = mapped.elementAt(i);
                const QPointF point(element.x, element.y);
                if (element.isMoveTo() && !points.isEmpty())
                    return false;
                if (element.isMoveTo() || element.isLineTo())
                    points.append({point, point, point, false});
                else if (element.type == QPainterPath::CurveToElement &&
                         i + 2 < mapped.elementCount() && !points.isEmpty()) {
                    const auto handle = mapped.elementAt(++i), end = mapped.elementAt(++i);
                    points.last().outgoing = point;
                    const QPointF target(end.x, end.y);
                    points.append({target, QPointF(handle.x, handle.y), target, false});
                }
            }
            if (path.closed && points.size() > 1 &&
                QLineF(points.first().position, points.last().position).length() < 1e-6) {
                points.first().incoming = points.last().incoming;
                points.removeLast();
            }
            path.points = points;
        } else {
            for (auto &p : path.points) {
                p.position = _transform.map(p.position);
                p.incoming = _transform.map(p.incoming);
                p.outgoing = _transform.map(p.outgoing);
            }
        }
        if (path.points.isEmpty() || path.points.size() > MaxPoints ||
            (path.closed && path.points.size() < 2))
            return false;
        total += int(path.points.size());
        if (total > MaxTotalPoints)
            return false;
        for (const auto &p : path.points)
            for (const QPointF point : {p.position, p.incoming, p.outgoing})
                if (!std::isfinite(point.x()) || !std::isfinite(point.y()) ||
                    std::abs(point.x()) > 1e6 || std::abs(point.y()) > 1e6)
                    return false;
    }
    _paths = result;
    return true;
}
