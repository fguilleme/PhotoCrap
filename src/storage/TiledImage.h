#pragma once

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <functional>
#include <memory>

class ScratchImageCache;

// Copies share immutable tile versions. Painting copies only touched versions;
// a uniform background occupies one pixel. Separate copies may be used on workers.
class TiledImage final
{
public:
    static constexpr int TileSize = 256;
    TiledImage()                  = default;
    TiledImage(const QSize &_size, QImage::Format _format, const QColor &_background);

    bool isNull() const
    {
        return !m_data;
    }
    QSize  size() const;
    int    tileCount() const;
    qint64 allocatedBytes() const;
    qint64 heapBytes() const;
    QImage region(const QRect &_region) const;
    bool   paint(const QRect &_region, const std::function<void(QPainter &)> &_paint);
    // Reuse unique tiles for disposable gesture buffers. On failure the caller
    // must discard this buffer; shared snapshots are still protected.
    bool paintTransient(const QRect &_region, const std::function<void(QPainter &)> &_paint);

    // Run on a worker. Successful tiles keep their own read-only file lease,
    // so clearing/evicting the disposable cache cannot lose editable content.
    // Failure keeps remaining tiles in RAM and never changes their pixels.
    bool offload(ScratchImageCache &_cache) const;

private:
    bool paintRegion(const QRect &_region, const std::function<void(QPainter &)> &_paint,
                     bool _preserve);
    struct Data;
    std::shared_ptr<Data> m_data;
};
