#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyPhotoshopLight(const QImage &_source, const QVariantMap &_parameters);
}
