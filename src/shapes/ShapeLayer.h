#pragma once
#include "storage/PixelPlane.h"
#include "tools/PenPath.h"
#include <QVariantMap>

namespace PhotoCrap::Shape
{
QVariantMap defaults();
bool        normalize(QVariantMap &_parameters);
PenPath     path(const QVariantMap &_parameters);
void        setPath(QVariantMap &_parameters, const PenPath &_path);
bool        transform(QVariantMap &_parameters, const QTransform &_transform);
QRect       bounds(const QVariantMap &_parameters, const QSize &_canvas);
PixelPlane  renderPlane(const QVariantMap &_parameters, const QSize &_canvas,
                        QImage::Format _format);
} // namespace PhotoCrap::Shape
