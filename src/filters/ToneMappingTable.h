#pragma once
#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
// CPU reference lookup: per-channel RGB for Curves/Levels, luminance for Light.
QImage toneMappingTable(const QString &_type, const QVariantMap &_parameters);
} // namespace PhotoCrap::Filters
