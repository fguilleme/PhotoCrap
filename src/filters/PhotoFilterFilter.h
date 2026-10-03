#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyPhotoFilter(const QImage &_source, const QVariantMap &_parameters);
}
