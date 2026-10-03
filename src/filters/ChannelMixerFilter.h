#pragma once

#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage applyChannelMixer(const QImage &_source, const QVariantMap &_parameters);
}
