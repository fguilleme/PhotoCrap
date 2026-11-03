#pragma once
#include <QRect>
#include <memory>
class QRhi;
class QRhiTexture;
class QRhiCommandBuffer;
class DisplayPyramidPass
{
public:
    DisplayPyramidPass(QRhi *rhi, const QRect &nativeRegion, const QSize &documentSize, int bits);
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
