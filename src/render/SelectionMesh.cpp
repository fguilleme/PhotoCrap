#include "SelectionMesh.h"
#include <QTransform>
#include <cmath>

namespace PhotoCrap::Render
{
namespace
{
bool clip(const QPointF &_a, const QPointF &_delta, const QRectF &_rect, qreal &_first,
          qreal &_last)
{
    const qreal p[]{-_delta.x(), _delta.x(), -_delta.y(), _delta.y()};
    const qreal q[]{_a.x() - _rect.left(), _rect.right() - _a.x(), _a.y() - _rect.top(),
                    _rect.bottom() - _a.y()};
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0) {
            if (q[i] < 0)
                return false;
        } else {
            const qreal t = q[i] / p[i];
            if (p[i] < 0)
                _first = qMax(_first, t);
            else
                _last = qMin(_last, t);
            if (_first > _last)
                return false;
        }
    }
    return true;
}
QPointF normal(const QPointF &_delta)
{
    const qreal length = std::hypot(_delta.x(), _delta.y());
    return length > 0 ? QPointF(-_delta.y() / length, _delta.x() / length) : QPointF{};
}
} // namespace
SelectionMesh selectionMesh(const QPainterPath &_path, const QSizeF &_scale,
                            const QRectF &_viewport, qreal _ratio, qsizetype _maximumVertices)
{
    SelectionMesh mesh;
    if (_viewport.isEmpty() || _ratio <= 0 || _scale.isEmpty())
        return mesh;
    const qreal  extent   = 1.25 / _ratio;
    const QRectF clipRect = _viewport.adjusted(-6 / _ratio, -6 / _ratio, 6 / _ratio, 6 / _ratio);
    const auto   polygons =
        _path.toSubpathPolygons(QTransform::fromScale(_scale.width(), _scale.height()));
    for (const auto &polygon : polygons) {
        QVector<QPointF> points;
        points.reserve(polygon.size());
        for (const auto &point : polygon)
            if (points.isEmpty() || points.last() != point)
                points.append(point);
        const bool closed = points.size() > 2 && points.first() == points.last();
        if (closed)
            points.removeLast();
        if (points.size() < 2)
            continue;
        const auto join = [&](int _index, const QPointF &_n, bool _start) {
            if (!closed && (_index == 0 || _index == points.size() - 1))
                return _n * extent;
            const int   previous = (_index + points.size() - 1) % points.size();
            const int   next     = (_index + 1) % points.size();
            const auto  a        = _start ? normal(points[_index] - points[previous]) : _n;
            const auto  b        = _start ? _n : normal(points[next] - points[_index]);
            const auto  sum      = a + b;
            const qreal length   = std::hypot(sum.x(), sum.y());
            if (length < 1e-8)
                return _n * extent;
            const auto  miter = sum / length;
            const qreal dot   = QPointF::dotProduct(miter, _n);
            return dot > 1e-8 ? miter * qMin(extent / dot, extent * 4) : _n * extent;
        };
        qreal     distance = 0;
        const int segments = closed ? points.size() : points.size() - 1;
        for (int i = 0; i < segments; ++i) {
            const int   next   = (i + 1) % points.size();
            const auto  delta  = points[next] - points[i];
            const qreal length = std::hypot(delta.x(), delta.y());
            qreal       first = 0, last = 1;
            if (length > 0 && clip(points[i], delta, clipRect, first, last)) {
                if (mesh.vertices.size() + 6 > _maximumVertices)
                    return {{}, true};
                const auto  n      = QPointF(-delta.y() / length, delta.x() / length);
                const auto  a      = points[i] + delta * first - _viewport.topLeft();
                const auto  b      = points[i] + delta * last - _viewport.topLeft();
                const auto  na     = first == 0 ? join(i, n, true) : n * extent;
                const auto  nb     = last == 1 ? join(next, n, false) : n * extent;
                const qreal start  = std::fmod((distance + length * first) * _ratio, 15.);
                const qreal end    = start + length * (last - first) * _ratio;
                const auto  vertex = [](const QPointF &_p, qreal _distance, float _side) {
                    return SelectionVertex{float(_p.x()), float(_p.y()), float(_distance), _side};
                };
                // Project the miter offset along the segment too: clipping away
                // a corner must not change the dash phase on the remaining line.
                const auto  along = delta / length;
                const qreal da    = QPointF::dotProduct(na, along) * _ratio;
                const qreal db    = QPointF::dotProduct(nb, along) * _ratio;
                const auto  ap    = vertex(a + na, start + da, 1.25f),
                            am    = vertex(a - na, start - da, -1.25f);
                const auto  bp    = vertex(b + nb, end + db, 1.25f),
                            bm    = vertex(b - nb, end - db, -1.25f);
                mesh.vertices << ap << am << bp << bp << am << bm;
            }
            distance += length;
        }
    }
    return mesh;
}
} // namespace PhotoCrap::Render
