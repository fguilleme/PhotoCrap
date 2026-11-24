#pragma once
#include <QImage>
#include <QVariantMap>

namespace PhotoCrap::Filters
{
QImage photoshopColorControls(const QImage &source, const QVariantMap &parameters);
QImage photoshopColorBalance(const QImage &source, const QVariantMap &parameters);
QImage photoshopPhotoFilter(const QImage &source, const QVariantMap &parameters);
QImage photoshopGradientMap(const QImage &source, const QVariantMap &parameters);
} // namespace PhotoCrap::Filters
