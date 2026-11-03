#pragma once
#include "SpatialFilterSettings.h"
#include <QSize>
#include <memory>
class QRhi;
class QRhiTexture;
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;
class SpatialFilterPass
{
public:
    struct Run {
        int          index   = -1;
        QRhiTexture *texture = nullptr;
    };
    SpatialFilterPass(QRhi *rhi, const QSize &size, int bits);
    ~SpatialFilterPass();
    void    resetFrame();
    Run     prepare(QRhiTexture *source, const PhotoCrap::Render::SpatialFilterSettings &settings,
                    QRhiResourceUpdateBatch *updates);
    quint64 render(QRhiCommandBuffer *cb, int run);
    qint64  bytes() const;

private:
    struct Data;
    std::unique_ptr<Data> d;
};
