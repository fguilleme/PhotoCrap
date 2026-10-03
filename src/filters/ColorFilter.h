#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyColor(const QImage &_source, const QVariantMap &_parameters);
}
