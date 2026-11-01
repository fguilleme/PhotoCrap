#include "CurvesGeometry.h"
#include <QPainterPath>
#include <QPainterPathStroker>
#include <cmath>

QString CurvesGeometry::stroke(const QVariantList &_samples, qreal _width, qreal _height) const
{
    if (_samples.size() != 256 || !std::isfinite(_width) || !std::isfinite(_height) ||
        _width <= 0 || _height <= 0)
        return {};
    QPainterPath center;
    for (int i = 0; i < 256; ++i) {
        bool        ok    = false;
        const qreal value = _samples[i].toDouble(&ok);
        if (!ok || !std::isfinite(value))
            return {};
        const QPointF point(i * _width / 255, _height - value / 255 * _height);
        if (i == 0)
            center.moveTo(point);
        else
            center.lineTo(point);
    }
    QPainterPathStroker stroker;
    stroker.setWidth(2);
    stroker.setCapStyle(Qt::FlatCap);
    stroker.setJoinStyle(Qt::MiterJoin);
    stroker.setMiterLimit(10);
    const auto outline = stroker.createStroke(center);
    QString    svg;
    svg.reserve(outline.elementCount() * 40);
    const auto pair = [](const QPainterPath::Element &_element) {
        return QString::number(_element.x, 'g', 17) + QLatin1Char(' ') +
               QString::number(_element.y, 'g', 17);
    };
    for (int i = 0; i < outline.elementCount(); ++i) {
        const auto element = outline.elementAt(i);
        if (element.isMoveTo())
            svg += QLatin1Char('M') + pair(element);
        else if (element.isLineTo())
            svg += QLatin1Char('L') + pair(element);
        else if (element.type == QPainterPath::CurveToElement && i + 2 < outline.elementCount()) {
            svg += QLatin1Char('C') + pair(element) + QLatin1Char(' ') +
                   pair(outline.elementAt(i + 1)) + QLatin1Char(' ') +
                   pair(outline.elementAt(i + 2));
            i += 2;
        }
    }
    return svg;
}
