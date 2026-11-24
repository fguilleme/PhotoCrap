#pragma once
#include "model3d/ModelPaint.h"
#include "storage/PixelPlane.h"
#include <QImage>
#include <QString>
#include <QTransform>
#include <QUuid>
#include <QVariantMap>

namespace PhotoCrap::Ai
{
struct ImageSource;
}

namespace PhotoCrap::Model3D
{
struct Asset;
}

struct Layer {
    Layer() = default;
    Layer(const QString &_layerName, const QImage &_layerImage, bool _layerVisible = true,
          bool _layerLocked = false, qreal _layerOpacity = 1, int _layerBlend = 0)
        : name(_layerName), image(_layerImage), visible(_layerVisible), locked(_layerLocked),
          opacity(_layerOpacity), blend(_layerBlend)
    {
    }
    // Runtime identity follows a layer through reordering and undo.
    QString     historyId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString     name;
    PixelPlane  image;
    bool        visible = true;
    bool        locked  = false;
    qreal       opacity = 1;
    int         blend   = 0;
    QString     kind    = QStringLiteral("pixel");
    QString     adjustmentType;
    QVariantMap parameters;
    PixelPlane  mask;
    bool        maskEnabled  = true;
    bool        clippingMask = false;
    QString     groupId;
    QString     parentGroupId; // Group headers have their own groupId.
    QString parentGroup() const { return kind == "group" ? parentGroupId : groupId; }
    bool        groupCollapsed = false;
    // Runtime-only preview: never serialized as document parameters.
    QString                                           directType;
    QVariantMap                                       directParameters;
    PixelPlane                                        directSelection;
    bool                                              directMask    = false;
    bool                                              directPreview = true;
    PixelPlane  source;
    QImage                                            surfaceNormals;
    // Keep geometry anchored to the photo when moving the paint or its mask.
    QTransform surfaceNormalTransform;
    QSize      surfaceNormalCanvas;
    void       transformSurfaceNormals(const QTransform &transform)
    {
        if (!surfaceNormals.isNull())
            surfaceNormalTransform *= transform;
    }
    std::shared_ptr<const PhotoCrap::Ai::ImageSource> aiSource;
    std::shared_ptr<const PhotoCrap::Model3D::Asset>  model3D;
    PhotoCrap::Model3D::Paint                         modelPaint;
};
