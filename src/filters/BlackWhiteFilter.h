#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyBlackWhite(const QImage &_source, const QVariantMap &_parameters);
}
