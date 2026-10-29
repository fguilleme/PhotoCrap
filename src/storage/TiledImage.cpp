#include "TiledImage.h"
#include "ScratchImageCache.h"

#include <QColorSpace>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <atomic>
#include <cstring>
#include <utility>

namespace
{
std::atomic<quint64> nextTileId{1};

struct Tile {
    mutable QMutex mutex;
    QImage         pixels;
    quint64        id         = nextTileId.fetch_add(1);
    bool           diskBacked = false;
    explicit Tile(QImage _pixels) : pixels(std::move(_pixels))
    {
    }
    QImage image() const
    {
        QMutexLocker locker(&mutex);
        return pixels;
    }
};
} // namespace

struct TiledImage::Data {
    QSize                             size;
    QImage                            background;
    QHash<int, std::shared_ptr<Tile>> tiles;
    int                               columns() const
    {
        return (size.width() + TileSize - 1) / TileSize;
    }
    int key(int _column, int _row) const
    {
        return _row * columns() + _column;
    }
    QRect rect(int _column, int _row) const
    {
        return QRect(_column * TileSize, _row * TileSize, TileSize, TileSize)
            .intersected(QRect(QPoint{}, size));
    }
    QImage blank(const QSize &_size) const
    {
        QImage image(_size, background.format());
        if (background.format() == QImage::Format_Grayscale8)
            image.fill(background.constScanLine(0)[0]);
        else
            image.fill(background.pixelColor(0, 0));
        image.setColorSpace(background.colorSpace());
        return image;
    }
};

TiledImage::TiledImage(const QSize &_size, QImage::Format _format, const QColor &_background)
{
    if (!_size.isValid() || _size.isEmpty() || _size.width() > 16384 || _size.height() > 16384 ||
        qint64(_size.width()) * _size.height() > 32LL * 1024 * 1024 ||
        (_format != QImage::Format_ARGB32_Premultiplied &&
         _format != QImage::Format_RGBA64_Premultiplied && _format != QImage::Format_Grayscale8))
        return;
    m_data             = std::make_shared<Data>();
    m_data->size       = _size;
    m_data->background = QImage(1, 1, _format);
    m_data->background.fill(_background);
    m_data->background.setColorSpace(QColorSpace::SRgb);
}

QSize TiledImage::size() const
{
    return m_data ? m_data->size : QSize{};
}

int TiledImage::tileCount() const
{
    return m_data ? m_data->tiles.size() : 0;
}

qint64 TiledImage::allocatedBytes() const
{
    qint64 bytes = 0;
    if (m_data)
        for (const auto &tile : std::as_const(m_data->tiles)) {
            QMutexLocker locker(&tile->mutex);
            bytes += tile->pixels.sizeInBytes();
        }
    return bytes;
}

qint64 TiledImage::heapBytes() const
{
    qint64 bytes = 0;
    if (m_data)
        for (const auto &tile : std::as_const(m_data->tiles)) {
            QMutexLocker locker(&tile->mutex);
            if (!tile->diskBacked)
                bytes += tile->pixels.sizeInBytes();
        }
    return bytes;
}

QImage TiledImage::region(const QRect &_region) const
{
    if (!m_data)
        return {};
    const QRect region = _region.intersected(QRect(QPoint{}, m_data->size));
    if (region.isEmpty())
        return {};
    QImage result = m_data->blank(region.size());
    if (result.isNull())
        return {};
    const int pixelBytes = result.depth() / 8;
    for (int row = region.top() / TileSize; row <= region.bottom() / TileSize; ++row)
        for (int column = region.left() / TileSize; column <= region.right() / TileSize; ++column) {
            const auto tile = m_data->tiles.value(m_data->key(column, row));
            if (!tile)
                continue;
            const QImage image   = tile->image();
            const QRect  bounds  = m_data->rect(column, row);
            const QRect  overlap = bounds.intersected(region);
            for (int y = overlap.top(); y <= overlap.bottom(); ++y)
                std::memcpy(result.scanLine(y - region.top()) +
                                (overlap.left() - region.left()) * pixelBytes,
                            image.constScanLine(y - bounds.top()) +
                                (overlap.left() - bounds.left()) * pixelBytes,
                            size_t(overlap.width()) * pixelBytes);
        }
    return result;
}

bool TiledImage::paint(const QRect &_region, const std::function<void(QPainter &)> &_paint)
{
    return paintRegion(_region, _paint, true);
}

bool TiledImage::paintTransient(const QRect &_region, const std::function<void(QPainter &)> &_paint)
{
    return paintRegion(_region, _paint, false);
}

bool TiledImage::paintRegion(const QRect &_region, const std::function<void(QPainter &)> &_paint,
                             bool _preserve)
{
    if (!m_data || !_paint)
        return false;
    const QRect region = _region.intersected(QRect(QPoint{}, m_data->size));
    if (region.isEmpty())
        return true;
    // Persistent versions stay unchanged until success. Disposable buffers can
    // reuse unique tiles; either path protects tiles shared with snapshots.
    const bool inPlace = !_preserve && m_data.use_count() == 1;
    auto       next    = inPlace ? m_data : std::make_shared<Data>(*m_data);
    for (int row = region.top() / TileSize; row <= region.bottom() / TileSize; ++row)
        for (int column = region.left() / TileSize; column <= region.right() / TileSize; ++column) {
            const int   key    = next->key(column, row);
            const QRect bounds = next->rect(column, row);
            auto        tile   = next->tiles.take(key);
            if (!tile)
                tile = std::make_shared<Tile>(next->blank(bounds.size()));
            else if (!inPlace || tile.use_count() != 1)
                tile = std::make_shared<Tile>(tile->image());
            {
                QMutexLocker locker(&tile->mutex);
                if (tile->pixels.isNull())
                    return false;
                QPainter painter(&tile->pixels);
                if (!painter.isActive())
                    return false;
                painter.translate(-bounds.topLeft());
                painter.setClipRect(region);
                _paint(painter);
                painter.end();
                tile->id         = nextTileId.fetch_add(1);
                tile->diskBacked = false;
            }
            next->tiles.insert(key, std::move(tile));
        }
    m_data = std::move(next);
    return true;
}

bool TiledImage::offload(ScratchImageCache &_cache) const
{
    if (!m_data)
        return true;
    bool complete = true;
    for (const auto &tile : std::as_const(m_data->tiles)) {
        QImage  image;
        QString key;
        {
            QMutexLocker locker(&tile->mutex);
            if (tile->diskBacked)
                continue;
            image = tile->pixels;
            key   = QStringLiteral("tile/%1").arg(tile->id);
        }
        if (!_cache.insert(key, image)) {
            complete = false;
            continue;
        }
        QImage mapped = _cache.find(key);
        if (mapped.isNull()) {
            complete = false;
            continue;
        }
        QMutexLocker locker(&tile->mutex);
        if (!tile->diskBacked) {
            tile->pixels     = std::move(mapped);
            tile->diskBacked = true;
        }
    }
    return complete;
}
