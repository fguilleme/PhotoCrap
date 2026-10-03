#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyCurves(const QImage &_source, const QVariantMap &_parameters);
}
