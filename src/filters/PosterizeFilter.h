#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyPosterize(const QImage &_source, const QVariantMap &_parameters);
}
