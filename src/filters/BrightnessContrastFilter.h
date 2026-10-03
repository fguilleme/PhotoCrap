#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyBrightnessContrast(const QImage &_source, const QVariantMap &_parameters);
}
