#pragma once

#include "storage/PixelPlane.h"
#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QTransform>
#include <QVariantMap>
#include <functional>
#include <memory>

namespace PhotoCrap::Model3D
{
constexpr qsizetype MaxFileBytes  = 64 * 1024 * 1024;
constexpr qsizetype MaxSceneBytes = 256 * 1024 * 1024;
struct Asset;
using Continue = std::function<bool()>;

std::shared_ptr<const Asset> load(QByteArray _bytes, const Continue &_continue = {},
                                  QString *_error = nullptr);
const QByteArray &           encoded(const Asset &_asset);
qsizetype                    storageBytes(const Asset &_asset);
QVariantMap                  pose(const QSize &_canvas);
QVariantMap                  light();
bool                         normalize(QVariantMap &_parameters);
bool       transformPlacement(QVariantMap &_parameters, const QTransform &_transform);
PixelPlane render(const Asset &_asset, const QVariantMap &_parameters, const QSize &_canvas,
                  QImage::Format _format, const Continue &_continue = {});
} // namespace PhotoCrap::Model3D
