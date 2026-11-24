#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
bool readLookTable(const QByteArray &xml, QVariantMap &parameters);
QImage applyColorLookup(const QImage &_source, const QVariantMap &_parameters, QPoint origin = {});
}
