#pragma once
#include "AiImageTypes.h"
#include <QTransform>

namespace PhotoCrap::Ai
{
inline constexpr int MaxSurfaceNormalEdge = 768;

struct SurfaceSelection {
    QImage mask, preview;
};

// Maps are aligned to the source image. Depth is relative inverse depth
// (near = white); normals use X right, Y up and Z toward the camera.
struct SurfaceGeometry {
    QImage disparity, normals, validity;

    bool                   isValid() const;
    static SurfaceGeometry fromComfyPreview(const QByteArray &preview, const Input &source);
    QImage                 regionAt(const QPoint &seed, const QSize &canvas, bool followRelief,
                                    const Cancel &cancel) const;
    static PixelPlane paintMask(const QImage &selected, const QSize &canvas, const Cancel &cancel,
                                const QImage &protection = {});
};

SurfaceSelection surfaceSelection(const QImage &mask, const QImage &protection,
                                  const Cancel &cancel);
QImage           restrictSurfaceRegion(QImage region, const QImage &boundary, const Cancel &cancel);
QImage           combineSurfaceRegions(QImage region, const QImage &previous, bool subtract,
                                       const Cancel &cancel);
QTransform surfaceBrushProjection(const QImage &normals, const QSize &canvas, const QPointF &center,
                                  const QTransform &mapping = {});
QImage projectSurfaceText(const QImage &text, const QPoint &origin, const QPointF &anchor,
                          const QImage &normals, const QSize &canvas, const QTransform &mapping);
} // namespace PhotoCrap::Ai
