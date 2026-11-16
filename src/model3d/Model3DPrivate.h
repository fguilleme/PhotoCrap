#pragma once

#include "Model3D.h"
#include <QQuaternion>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <array>

namespace PhotoCrap::Model3D
{
struct Vertex {
    QVector3D position, normal;
    QVector2D uv;
};

using Face = std::array<quint32, 3>;

struct Material {
    QVector4D color{1, 1, 1, 1};
    QImage    texture;
    bool      doubleSided = false, unlit = false, alphaMask = false;
    float     alphaCutoff = .5f;
    enum class Wrap { repeat, clamp, mirror };
    Wrap u = Wrap::repeat, v = Wrap::repeat;
};

struct Primitive {
    QVector<Vertex>    vertices;
    QVector<Face>      triangles;
    QVector<QVector4D> vertexColors;
    Material           material;
};

#ifdef Q_OS_MACOS
struct MetalMesh;
#endif
struct Asset {
    // The mapped file must outlive the QByteArray view.
    std::shared_ptr<QTemporaryFile> backing;
    QByteArray                      bytes;
    qsizetype                       bytesInMemory = 0;
    QVariantMap                     statistics;
    int                             triangleTarget = 0;
    QVector<Primitive>              primitives;
#ifdef Q_OS_MACOS
    mutable std::shared_ptr<const MetalMesh> metalMesh;
#endif
};

struct RenderView {
    QQuaternion rotation;
    QTransform  placement;
    QVector3D   lightDirection, lightColor;
    float       scale, ambient, intensity;
};
inline constexpr float CameraDistance = 3.f;

RenderView renderView(const QVariantMap &parameters);
QRgb       materialTexel(const Material &material, const QVector2D &uv);
float      linear(int byte);

constexpr int PaintGridEdge = 8;
struct PaintGrid {
    explicit PaintGrid(const Paint &paint);
    QList<quint32> offsets, indices;
};
QVector3D paintedColor(const Paint &paint, const PaintGrid &grid, const QVector3D &position,
                       const QVector3D &normal, QVector3D color);

PixelPlane renderSoftwarePreview(const Asset &_asset, const QVariantMap &_parameters,
                                 const QSize &_canvas, QImage::Format _format);

#ifdef Q_OS_MACOS
PixelPlane renderMetalPreview(const std::shared_ptr<const Asset> &_asset, const RenderView &_view,
                              const QSize &_canvas, const Paint &paint);
#endif
} // namespace PhotoCrap::Model3D
