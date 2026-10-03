#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyLight(const QImage &_source, const QVariantMap &_parameters);
}
