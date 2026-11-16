#pragma once

#include "ModelPaint.h"
#include "storage/PixelPlane.h"
#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QTransform>
#include <QVariantMap>
#include <functional>
#include <memory>

class QTemporaryFile;

namespace PhotoCrap::Model3D
{
constexpr qsizetype MaxFileBytes      = 512 * 1024 * 1024;
constexpr qsizetype MaxSceneBytes     = 1024 * 1024 * 1024;
constexpr int       MaxTriangles      = 8'000'000;
constexpr int       MinTriangleTarget = 100;
inline bool         validTriangleTarget(int target)
{
    return target == 0 || (target >= MinTriangleTarget && target <= MaxTriangles);
}
struct Asset;
struct PaintPicking;
using Continue = std::function<bool()>;

std::shared_ptr<const Asset> load(QByteArray _bytes, const Continue &_continue = {},
                                  QString *_error = nullptr, int _triangleTarget = 0);
std::shared_ptr<const Asset> loadTemporary(const std::shared_ptr<QTemporaryFile> &_file,
                                           const Continue &_continue, QString *_error,
                                           int _triangleTarget = 0);
const QByteArray &           encoded(const Asset &_asset);
qsizetype                    storageBytes(const Asset &_asset);
const QVariantMap &          statistics(const Asset &_asset);
int                          triangleTarget(const Asset &_asset);
QVariantMap                  pose(const QSize &_canvas);
QVariantMap fittedPose(const Asset &_asset, const QRectF &_bounds, const Continue &_continue = {});
QVariantMap light();
bool        normalize(QVariantMap &_parameters);
bool        transformPlacement(QVariantMap &_parameters, const QTransform &_transform);
PixelPlane  render(const Asset &_asset, const QVariantMap &_parameters, const QSize &_canvas,
                   QImage::Format _format, const Continue &_continue = {}, const Paint &paint = {});
PixelPlane renderPreview(const std::shared_ptr<const Asset> &_asset, const QVariantMap &_parameters,
                         const QSize &_canvas, QImage::Format _format, const Paint &paint = {});
std::shared_ptr<const PaintPicking> paintPicking(const Asset &asset);
bool paintDab(const PaintPicking &picking, const QVariantMap &parameters, const QPointF &point,
              qreal radius, qreal hardness, const QColor &color, qreal opacity, PaintDab &dab);
} // namespace PhotoCrap::Model3D
