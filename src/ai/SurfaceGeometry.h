#pragma once
#include "AiImageTypes.h"
#include <QPolygonF>
#include <QTransform>
#include <QVector3D>
#include <optional>
#include <vector>

namespace PhotoCrap::Ai
{
inline constexpr int MaxSurfaceNormalEdge = 768;

struct SurfaceSelection {
    QImage mask, preview;
    QList<QPoint> planarSeeds;
};

struct SurfaceCrease {
    // The face used to place and orient the text is on the negative side.
    QLineF    edge;
    QVector3D negativeNormal, positiveNormal;

    QVariantList                        parameters() const;
    static std::optional<SurfaceCrease> fromParameters(const QVariant &value);
};

struct SurfaceHinge {
    QTransform across;
    QVector3D  referenceNormal; // Camera coordinates, with Z pointing into the scene.
};

// Maps are aligned to the source image. Depth is relative inverse depth
// (near = white); normals use X right, Y up and Z toward the camera.
struct SurfaceGeometry {
    QImage disparity, normals, validity;
    qreal  focalRatio = .85; // Focal length divided by the image diagonal.

    bool                   isValid() const;
    static SurfaceGeometry fromComfyPreview(const QByteArray &preview, const Input &source);
    QImage                 regionAt(const QPoint &seed, const QSize &canvas, bool followRelief,
                                    const Cancel &cancel) const;
    std::optional<SurfaceCrease> textCrease(const QImage &selected, const QList<QPoint> &seeds,
                                            const QSize &canvas, const QPoint &anchor,
                                            const Cancel &cancel) const;
    void closeTextCrease(QImage &selected, const QSize &canvas, const SurfaceCrease &crease,
                         const Cancel &cancel) const;
    static PixelPlane paintMask(const QImage &selected, const QSize &canvas, const Cancel &cancel,
                                const QImage &protection = {}, const QImage &face = {});
};

SurfaceSelection surfaceSelection(const QImage &mask, const QImage &protection,
                                  const Cancel &cancel);
QImage           restrictSurfaceRegion(QImage region, const QImage &boundary, const Cancel &cancel);
QImage           combineSurfaceRegions(QImage region, const QImage &previous, bool subtract,
                                       const Cancel &cancel);
QTransform surfaceBrushProjection(const QImage &normals, const QSize &canvas, const QPointF &center,
                                  const QTransform &mapping = {});
QTransform surfaceTextProjection(const QImage &normals, const QSize &canvas, const QPointF &anchor,
                                 const QTransform &mapping, qreal angle, qreal focalRatio);
QTransform planeTextProjection(const QSize &canvas, const QPointF &anchor, const QVector3D &normal,
                               qreal angle, qreal focalRatio);
std::optional<SurfaceHinge> surfaceTextHinge(const SurfaceCrease &crease, const QSize &canvas,
                                             qreal focalRatio);
QImage     projectSurfaceText(const QImage &text, const QPoint &origin, const QPointF &anchor,
                              const QImage &normals, const QSize &canvas, const QTransform &mapping,
                              qreal focalRatio = .85, qreal strength = 1.);
// The same sampling grid drives the rasterizer and the editable text overlay.
struct SurfaceTextWarp {
    QRect                region;
    QSize                steps;
    std::vector<QPointF> grid;
    QPointF              unmap(const QPointF &canvasPoint) const;
    QPolygonF            outline(const QPolygonF &polygon, QPointF *bottomRight = nullptr) const;

private:
    QPointF sourcePoint(const QPointF &point) const;
    QPointF outputPoint(const QPointF &target, const QPointF &guess) const;
};

SurfaceTextWarp surfaceTextWarp(const QRect &region, const QPointF &anchor, const QImage &normals,
                                const QSize &canvas, const QTransform &mapping, qreal focalRatio,
                                qreal strength);
} // namespace PhotoCrap::Ai
