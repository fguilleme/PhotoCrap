#pragma once
#include "AiImageTypes.h"

namespace PhotoCrap::Ai
{
// Maps are aligned to the source image. Depth is relative inverse depth
// (near = white); normals use X right, Y up and Z toward the camera.
struct SurfaceGeometry {
    QImage disparity, normals, validity;

    bool                   isValid() const;
    static SurfaceGeometry fromComfyPreview(const QByteArray &preview, const Input &source);
    PixelPlane connectedMask(const QPoint &seed, const QSize &canvas, const Cancel &cancel) const;
};
} // namespace PhotoCrap::Ai
