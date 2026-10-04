#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyThreshold(const QImage &_source, const QVariantMap &_parameters);
}
