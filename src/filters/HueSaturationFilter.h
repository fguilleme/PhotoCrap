#pragma once
#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyHueSaturation(const QImage &_source, const QVariantMap &_parameters);
}
