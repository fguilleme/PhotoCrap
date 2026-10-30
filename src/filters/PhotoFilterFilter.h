#pragma once

#include <QColor>
#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QColor photoFilterColor(const QVariantMap &_parameters);
QImage applyPhotoFilter(const QImage &_source, const QVariantMap &_parameters);
} // namespace PhotoCrap::Filters
