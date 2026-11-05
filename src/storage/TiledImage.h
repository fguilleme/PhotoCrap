#pragma once

#include <QColor>
#include <QColorSpace>
#include <QHash>
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
    // Expand a one-pixel plane logically, preserving native bits and metadata.
    TiledImage(const QSize &_size, const QImage &_uniform);

    bool isNull() const
    {
        return !m_data;
    }
    QSize  size() const;
    int    tileCount() const;
    qint64 allocatedBytes() const;
    qint64 heapBytes() const;
    // Import native pixels exactly; updates retain immutable versions of
    // unchanged tiles, including their disk leases.
    bool replace(const QImage &_image);
    static TiledImage fromBands(const QSize &_size, QImage::Format _format,
                                const std::function<QImage(const QRect &)> &_read);
    // Convert native pixels per stored tile; failure/cancellation leaves this
    // snapshot intact. The callback is checked between tile conversions.
    TiledImage convertedTo(QImage::Format               _format,
                           const std::function<bool()> &_continue = {}) const;
    bool       writeRegion(const QPoint &_origin, const QImage &_pixels, bool *_changed = nullptr,
                           bool _preserveSnapshots = true);
    bool       sharedStorage() const;
    // Conservative native metadata, without reconstructing pixels.
    bool                  knownOpaque() const;
    QImage::Format        format() const;
    QColorSpace           colorSpace() const;
    qreal                 devicePixelRatio() const;
    QHash<qint64, qint64> storageBlocks() const;
    QHash<qint64, qint64> heapBlocks() const;
    QByteArray            regionKey(const QRect &_region) const;
    QImage                region(const QRect &_region) const;
    // Read-only snapshot; absent tiles return the compact uniform background.
    // QImage's copy-on-write keeps caller edits separate from the stored plane.
    QImage tileImage(int _column, int _row) const;
    QImage backgroundPixel() const;
    // Nearest-neighbor reductions read only sampled pixels; upscales use Qt.
    QImage scaledFast(const QSize &_size) const;
    bool   paint(const QRect &_region, const std::function<void(QPainter &)> &_paint);
    // Reuse unique tiles for disposable gesture buffers. On failure the caller
    // must discard this buffer; shared snapshots are still protected.
    bool paintTransient(const QRect &_region, const std::function<void(QPainter &)> &_paint);

    // Run on a worker. Successful tiles keep their own read-only file lease,
    // so clearing/evicting the disposable cache cannot lose editable content.
    // Failure keeps remaining tiles in RAM and never changes their pixels.
    bool offload(ScratchImageCache &_cache, const std::function<bool()> &_continue = {}) const;

private:
    bool paintRegion(const QRect &_region, const std::function<void(QPainter &)> &_paint,
                     bool _preserve);
    struct Data;
    std::shared_ptr<Data> m_data;
};
