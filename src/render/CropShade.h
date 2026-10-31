#pragma once
#include <QPointF>
#include <QPolygonF>
#include <QVector4D>
#include <array>

namespace PhotoCrap::Render
{
struct CropShadeParameters {
    std::array<QVector4D, 4> edges{};
    float                    valid                                         = 0;
    bool                     operator==(const CropShadeParameters &) const = default;
};

// Outward unit planes in viewport-local coordinates. Double precision on the
// CPU avoids cancellation between huge offscreen endpoints in the shader.
CropShadeParameters cropShadeParameters(const QPolygonF &_points, qreal _zoom,
                                        const QPointF &_origin);
} // namespace PhotoCrap::Render
