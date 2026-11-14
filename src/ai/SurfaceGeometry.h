#pragma once
#include "AiImageTypes.h"
#include <optional>

namespace PhotoCrap::Ai
{
struct SurfaceSelection {
    QImage mask, preview;
};

// Maps are aligned to the source image. Depth is relative inverse depth
// (near = white); normals use X right, Y up and Z toward the camera.
struct SurfaceGeometry {
    QImage disparity, normals, validity;

    bool                   isValid() const;
    static SurfaceGeometry fromComfyPreview(const QByteArray &preview, const Input &source);
    std::optional<SurfaceSelection> selectRegion(const QPoint &seed, const QSize &canvas,
                                                 bool followRelief, bool subtract,
                                                 const QImage &previous,
                                                 const Cancel &cancel) const;
    static PixelPlane paintMask(const QImage &selected, const QSize &canvas, const Cancel &cancel,
                                const QImage &protection = {});
};

SurfaceSelection surfaceSelection(const QImage &mask, const QImage &protection,
                                  const Cancel &cancel);
} // namespace PhotoCrap::Ai
