#pragma once
#include <QRect>
#include <functional>
#include <memory>
namespace PhotoCrap::Render
{
struct StackStatistics;
}
class QRhi;
class QRhiTexture;
class QRhiCommandBuffer;
// Shared horizontal reduction targets. Outputs always remain private to a tile.
class DisplayPyramidScratchPool
{
public:
    DisplayPyramidScratchPool(QRhi *rhi, std::function<void(qint64)> delta);
    ~DisplayPyramidScratchPool();
    qint64 bytes() const;

private:
    friend class DisplayPyramidPass;
    struct Data;
    std::unique_ptr<Data> d;
};
class DisplayPyramidPass
{
public:
    DisplayPyramidPass(QRhi *rhi, const QRect &nativeRegion, const QSize &documentSize, int bits,
                       PhotoCrap::Render::StackStatistics        &statistics,
                       std::shared_ptr<DisplayPyramidScratchPool> pool);
    ~DisplayPyramidPass();
    // Creates a retained level; input remains owned by the native tile.
    QRhiTexture *ensure(QRhiTexture *source, int level);
    QRect        region(int level) const;
    quint64      render(QRhiCommandBuffer *cb, int level, quint64 generation);
    qint64       bytes() const;

private:
    struct Data;
    std::unique_ptr<Data> d;
};
