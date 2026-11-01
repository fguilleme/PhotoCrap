#include "PenBandGeometry.h"
#include <QLineF>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <cmath>

QString PenBandGeometry::outline(QPointF _start, QPointF _control, QPointF _end, QPointF _origin,
                                 qreal _pixelScale) const
{
    const auto finite = [](QPointF _p) { return std::isfinite(_p.x()) && std::isfinite(_p.y()); };
    if (!finite(_start) || !finite(_control) || !finite(_end) || !finite(_origin) ||
        !std::isfinite(_pixelScale) || _pixelScale <= 0 || _pixelScale > 8 ||
        QLineF(_start, _control).length() + QLineF(_control, _end).length() > 8192)
        return {};
    _start -= _origin;
    _control -= _origin;
    _end -= _origin;
    // Match the Canvas' device-space flattening, including HiDPI dash lengths.
    _start *= _pixelScale;
    _control *= _pixelScale;
    _end *= _pixelScale;
    QPainterPath center(_start);
    center.cubicTo(_control, _end, _end);
    QPainterPathStroker stroker;
    stroker.setWidth(1.5 * _pixelScale);
    stroker.setCapStyle(Qt::FlatCap);
    stroker.setJoinStyle(Qt::MiterJoin);
    stroker.setMiterLimit(10);
    stroker.setDashPattern({4., 3.});
    const auto outline = stroker.createStroke(center);
    QString    svg;
    svg.reserve(outline.elementCount() * 40);
    const auto pair = [_pixelScale](const QPainterPath::Element &_e) {
        return QString::number(_e.x / _pixelScale, 'g', 17) + QLatin1Char(' ') +
               QString::number(_e.y / _pixelScale, 'g', 17);
    };
    for (int i = 0; i < outline.elementCount(); ++i) {
        const auto e = outline.elementAt(i);
        if (e.isMoveTo())
            svg += QLatin1Char('M') + pair(e);
        else if (e.isLineTo())
            svg += QLatin1Char('L') + pair(e);
        else if (e.type == QPainterPath::CurveToElement && i + 2 < outline.elementCount()) {
            svg += QLatin1Char('C') + pair(e) + QLatin1Char(' ') + pair(outline.elementAt(i + 1)) +
                   QLatin1Char(' ') + pair(outline.elementAt(i + 2));
            i += 2;
        }
    }
    return svg;
}
