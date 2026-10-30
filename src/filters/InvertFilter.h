#pragma once
#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyInvert(const QImage &_source, const QVariantMap &_parameters);
}
