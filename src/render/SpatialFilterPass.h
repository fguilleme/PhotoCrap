#pragma once
#include "SpatialFilterSettings.h"
#include <QSize>
#include <memory>
namespace PhotoCrap::Render
{
struct StackStatistics;
}
class QRhi;
class QRhiTexture;
class QRhiBuffer;
class QRhiSampler;
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;
class SpatialFilterPass
{
public:
    struct Run {
        int          index   = -1;
        QRhiTexture *texture = nullptr;
    };
    SpatialFilterPass(QRhi *rhi, const QSize &size, int bits, QRhiBuffer &vertices,
                      QRhiSampler &sampler, PhotoCrap::Render::StackStatistics &statistics);
    ~SpatialFilterPass();
    void    resetFrame(bool reuseSteps);
    void    finishPreparation();
    Run     prepare(QRhiTexture *source, const PhotoCrap::Render::SpatialFilterSettings &settings,
                    QRhiResourceUpdateBatch *updates, bool uploaded = false, quint64 sourceId = 0);
    quint64 render(QRhiCommandBuffer *cb, int run);
    qint64  bytes() const;

private:
    struct Data;
    std::unique_ptr<Data> d;
};
