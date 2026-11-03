#include "TiledImage.h"
#include "ScratchImageCache.h"

#include <QColorSpace>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
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
        image.setDevicePixelRatio(background.devicePixelRatio());
        image.setDotsPerMeterX(background.dotsPerMeterX());
        image.setDotsPerMeterY(background.dotsPerMeterY());
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

TiledImage::TiledImage(const QSize &_size, const QImage &_uniform)
    : TiledImage(_size, _uniform.format(), Qt::transparent)
{
    if (_uniform.size() != QSize(1, 1))
        m_data.reset();
    else if (m_data)
        m_data->background = _uniform;
}

QSize TiledImage::size() const
{
    return m_data ? m_data->size : QSize{};
}

int TiledImage::tileCount() const
{
    return m_data ? m_data->tiles.size() : 0;
}
QImage TiledImage::backgroundPixel() const
{
    return m_data ? m_data->background : QImage{};
}
QImage::Format TiledImage::format() const
{
    return m_data ? m_data->background.format() : QImage::Format_Invalid;
}
QColorSpace TiledImage::colorSpace() const
{
    return m_data ? m_data->background.colorSpace() : QColorSpace{};
}
QHash<qint64, qint64> TiledImage::storageBlocks() const
{
    QHash<qint64, qint64> result;
    if (m_data)
        for (const auto &tile : std::as_const(m_data->tiles)) {
            QMutexLocker locker(&tile->mutex);
            result.insert(-qint64(tile->id), tile->pixels.sizeInBytes());
        }
    return result;
}
QHash<qint64, qint64> TiledImage::heapBlocks() const
{
    QHash<qint64, qint64> result;
    if (m_data)
        for (const auto &tile : std::as_const(m_data->tiles)) {
            QMutexLocker locker(&tile->mutex);
            if (!tile->diskBacked)
                result.insert(-qint64(tile->id), tile->pixels.sizeInBytes());
        }
    return result;
}

QByteArray TiledImage::regionKey(const QRect &_region) const
{
    if (!m_data)
        return {};
    const QRect region = _region.intersected(QRect(QPoint{}, size()));
    QByteArray  key;
    const auto  append = [&](qint64 value) {
        key.append(reinterpret_cast<const char *>(&value), sizeof(value));
    };
    for (int value : {region.x(), region.y(), region.width(), region.height(), int(format())})
        append(value);
    key.append(reinterpret_cast<const char *>(m_data->background.constBits()),
               m_data->background.depth() / 8);
    if (!region.isEmpty())
        for (int y = region.top() / TileSize; y <= region.bottom() / TileSize; ++y)
            for (int x = region.left() / TileSize; x <= region.right() / TileSize; ++x) {
                const auto tile = m_data->tiles.value(m_data->key(x, y));
                if (tile) {
                    QMutexLocker locker(&tile->mutex);
                    append(qint64(tile->id));
                } else
                    append(0);
            }
    return key;
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

bool TiledImage::replace(const QImage &_image)
{
    if (_image.isNull())
        return false;
    TiledImage initial(_image.size(), _image.format(), Qt::transparent);
    if (initial.isNull())
        return false;
    const bool reuse = m_data && m_data->size == _image.size() &&
                       m_data->background.format() == _image.format() &&
                       m_data->background.colorSpace() == _image.colorSpace();
    auto       next  = reuse ? std::make_shared<Data>(*m_data) : initial.m_data;
    next->background.setColorSpace(_image.colorSpace());
    next->background.setDevicePixelRatio(_image.devicePixelRatio());
    next->background.setDotsPerMeterX(_image.dotsPerMeterX());
    next->background.setDotsPerMeterY(_image.dotsPerMeterY());
    const int pixelBytes = _image.depth() / 8;
    for (int row = 0; row < (_image.height() + TileSize - 1) / TileSize; ++row)
        for (int column = 0; column < next->columns(); ++column) {
            const auto   bounds   = next->rect(column, row);
            const int    key      = next->key(column, row);
            const auto   previous = next->tiles.value(key);
            const QImage pixels   = previous ? previous->image() : next->blank(bounds.size());
            bool         equal    = true;
            for (int y = 0; y < bounds.height(); ++y)
                if (std::memcmp(pixels.constScanLine(y),
                                _image.constScanLine(bounds.y() + y) + bounds.x() * pixelBytes,
                                size_t(bounds.width()) * pixelBytes) != 0) {
                    equal = false;
                    break;
                }
            if (!equal) {
                const QImage replacement = _image.copy(bounds);
                if (replacement.isNull())
                    return false;
                if (replacement == next->blank(bounds.size()))
                    next->tiles.remove(key);
                else
                    next->tiles.insert(key, std::make_shared<Tile>(replacement));
            }
        }
    m_data = std::move(next);
    return true;
}

QImage TiledImage::tileImage(int _column, int _row) const
{
    if (!m_data || _column < 0 || _row < 0 ||
        _column >= (m_data->size.width() + TileSize - 1) / TileSize ||
        _row >= (m_data->size.height() + TileSize - 1) / TileSize)
        return {};
    const auto tile = m_data->tiles.value(m_data->key(_column, _row));
    return tile ? tile->image() : m_data->background;
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

QImage TiledImage::scaledFast(const QSize &target) const
{
    if (!m_data || target.isEmpty())
        return {};
    if (target.width() > size().width() || target.height() > size().height())
        return region(QRect(QPoint{}, size()))
            .scaled(target, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    if (target == size())
        return region(QRect(QPoint{}, size()));
    QImage result = m_data->blank(target);
    if (result.isNull() || m_data->tiles.isEmpty())
        return result;
    // Qt's horizontal and vertical paths can use different rounding. Obtain
    // each axis in its own direction, in the native wide-pixel format when needed.
    const auto coordinates = [wide = format() == QImage::Format_RGBA64_Premultiplied](
                                 int source, int destination, bool vertical, bool otherChanges) {
        // Preserve Qt's one-axis versus two-axis scaler selection.
        const int fromOther = otherChanges ? 2 : 1;
        QImage    strip(vertical ? QSize(fromOther, source) : QSize(source, fromOther),
                        wide ? QImage::Format_RGBA64_Premultiplied
                             : QImage::Format_ARGB32_Premultiplied);
        if (strip.isNull())
            return QVector<int>{};
        for (int i = 0; i < source; ++i)
            for (int other = 0; other < fromOther; ++other) {
                auto     *row = strip.scanLine(vertical ? i : other);
                const int x   = vertical ? other : i;
                if (wide)
                    reinterpret_cast<QRgba64 *>(row)[x] =
                        QRgba64::fromRgba64(0, 0, ushort(i), 65535);
                else
                    reinterpret_cast<QRgb *>(row)[x] = 0xff000000u | quint32(i);
            }
        strip = strip.scaled(vertical ? QSize(1, destination) : QSize(destination, 1),
                             Qt::IgnoreAspectRatio, Qt::FastTransformation);
        if (strip.isNull())
            return QVector<int>{};
        QVector<int> values(destination);
        for (int i = 0; i < destination; ++i) {
            const auto *row = strip.constScanLine(vertical ? i : 0);
            const int   x   = vertical ? 0 : i;
            values[i]       = wide ? int(reinterpret_cast<const QRgba64 *>(row)[x].blue())
                                   : int(reinterpret_cast<const QRgb *>(row)[x] & 0x00ffffffu);
        }
        return values;
    };
    const auto xs =
        coordinates(size().width(), target.width(), false, size().height() != target.height());
    const auto ys =
        coordinates(size().height(), target.height(), true, size().width() != target.width());
    if (xs.size() != target.width() || ys.size() != target.height())
        return {};
    const int bytes = result.depth() / 8;
    for (int y = 0; y < target.height(); ++y) {
        uchar    *output = result.scanLine(y);
        const int row    = ys[y] / TileSize;
        for (int x = 0; x < target.width();) {
            const int column = xs[x] / TileSize;
            int       end    = x + 1;
            while (end < target.width() && xs[end] / TileSize == column)
                ++end;
            if (const auto tile = m_data->tiles.value(m_data->key(column, row))) {
                const QImage pixels = tile->image();
                const uchar *input  = pixels.constScanLine(ys[y] - row * TileSize);
                for (; x < end; ++x)
                    std::memcpy(output + x * bytes, input + (xs[x] - column * TileSize) * bytes,
                                size_t(bytes));
            } else
                x = end; // Uniform pixels were already filled by blank().
        }
    }
    return result;
}

bool TiledImage::paint(const QRect &_region, const std::function<void(QPainter &)> &_paint)
{
    return paintRegion(_region, _paint, true);
}

bool TiledImage::writeRegion(const QPoint &_origin, const QImage &_pixels, bool *_changed)
{
    if (_changed)
        *_changed = false;
    if (!m_data || _pixels.isNull() || _pixels.format() != format())
        return false;
    const QRect region = QRect(_origin, _pixels.size()).intersected(QRect(QPoint{}, size()));
    if (region.isEmpty())
        return true;
    std::shared_ptr<Data> next;
    const int             bytes = _pixels.depth() / 8;
    for (int y = region.top() / TileSize; y <= region.bottom() / TileSize; ++y)
        for (int x = region.left() / TileSize; x <= region.right() / TileSize; ++x) {
            const QRect  bounds = m_data->rect(x, y), overlap = region.intersected(bounds);
            const auto   previous = m_data->tiles.value(m_data->key(x, y));
            const QImage old      = previous ? previous->image() : QImage{};
            if (previous && old.isNull())
                return false;
            bool equal = true;
            for (int row = overlap.top(); row <= overlap.bottom() && equal; ++row) {
                const auto *input =
                    _pixels.constScanLine(row - _origin.y()) + (overlap.x() - _origin.x()) * bytes;
                if (previous)
                    equal = std::memcmp(old.constScanLine(row - bounds.y()) +
                                            (overlap.x() - bounds.x()) * bytes,
                                        input, size_t(overlap.width()) * bytes) == 0;
                else
                    for (int column = 0; column < overlap.width() && equal; ++column)
                        equal = std::memcmp(input + column * bytes, m_data->background.constBits(),
                                            size_t(bytes)) == 0;
            }
            if (equal)
                continue;
            if (!next)
                next = std::make_shared<Data>(*m_data);
            QImage image = previous ? old.copy() : next->blank(bounds.size());
            if (image.isNull())
                return false;
            for (int row = overlap.top(); row <= overlap.bottom(); ++row)
                std::memcpy(image.scanLine(row - bounds.y()) + (overlap.x() - bounds.x()) * bytes,
                            _pixels.constScanLine(row - _origin.y()) +
                                (overlap.x() - _origin.x()) * bytes,
                            size_t(overlap.width()) * bytes);
            next->tiles.insert(next->key(x, y), std::make_shared<Tile>(image));
        }
    if (next) {
        m_data = std::move(next);
        if (_changed)
            *_changed = true;
    }
    return true;
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
                const qreal ratio = tile->pixels.devicePixelRatio();
                painter.scale(1. / ratio, 1. / ratio);
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
