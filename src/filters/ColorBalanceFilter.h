#pragma once
#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyColorBalance(const QImage &_source, const QVariantMap &_parameters);
} // namespace PhotoCrap::Filters
