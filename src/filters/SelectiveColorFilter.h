#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QVariantMap defaultSelectiveColorParameters();
QImage      applySelectiveColor(const QImage &_source, const QVariantMap &_parameters);
} // namespace PhotoCrap::Filters
