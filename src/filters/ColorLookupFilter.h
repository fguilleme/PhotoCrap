#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyColorLookup(const QImage &_source, const QVariantMap &_parameters);
}
