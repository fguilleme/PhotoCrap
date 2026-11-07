#pragma once

#include <QBrush>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QVariantMap>
#include <array>
#include <vector>

class QPainter;

namespace PhotoCrap::Tools
{
struct GradientRaster {
    QPointF start, end;
    QGradientStops stops;
    int shape = 0;
    int method = 0;
    bool dither = false;

    mutable std::vector<std::array<float, 4>> ramp;

    static bool decode(const QVariantMap &data, const QSize &size, GradientRaster &result);
    QVariantMap encode(const QSize &size) const;
    QImage palette() const;
    QImage image(const QRect &area, bool wide) const;
    void prepareRamp() const;
    bool paint(QPainter &painter, const QRect &area, bool wide) const;
};
}
