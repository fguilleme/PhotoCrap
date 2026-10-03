#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyGradientMap(const QImage &_source, const QVariantMap &_parameters);
}
