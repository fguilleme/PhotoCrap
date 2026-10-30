#pragma once
#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyVibrance(const QImage &_source, const QVariantMap &_parameters);
}
