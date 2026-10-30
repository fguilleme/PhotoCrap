#pragma once
#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyExposure(const QImage &_source, const QVariantMap &_parameters);
}
