#include "CropShade.h"
#include <cmath>

namespace PhotoCrap::Render
{
CropShadeParameters cropShadeParameters(const QPolygonF &_points, qreal _zoom,
                                        const QPointF &_origin)
{
    CropShadeParameters result;
    if (_points.size() != 4 || !std::isfinite(_zoom) || _zoom <= 0)
        return result;
    QPolygonF projected;
    for (const auto &p : _points) {
        const auto point = p * _zoom - _origin;
        if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
            return result;
        projected.append(point);
    }
    for (int i = 0; i < 4; ++i) {
        const auto  a = projected[i], delta = projected[(i + 1) % 4] - a;
        const auto  next   = projected[(i + 2) % 4] - projected[(i + 1) % 4];
        const qreal length = std::hypot(delta.x(), delta.y());
        if (length <= 0 || delta.x() * next.y() - delta.y() * next.x() <= 0)
            return {};
        const QPointF normal(delta.y() / length, -delta.x() / length);
        const qreal   distance = -QPointF::dotProduct(normal, a);
        if (!std::isfinite(distance) || !std::isfinite(float(distance)))
            return {};
        result.edges[i] = QVector4D(float(normal.x()), float(normal.y()), float(distance), 0);
    }
    result.valid = 1;
    return result;
}
} // namespace PhotoCrap::Render
