#include "TiledImage.h"
#include "ImageSampling.h"
#include "ScratchImageCache.h"

#include <QCache>
#include <QColorSpace>
#include <QCryptographicHash>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <utility>

namespace
{
std::atomic<quint64> nextTileId{1};

QByteArray uniformRegionKey(const QImage &_pixel, const QSize &_size)
{
    // Backgrounds recur across planes and render regions. Bound the per-thread
    // metadata independently of the number of compact layers or snapshots.
    thread_local QCache<QByteArray, QByteArray> cache(256);
    QByteArray                                  key;
    for (int value : {int(_pixel.format()), _size.width(), _size.height()})
        key.append(reinterpret_cast<const char *>(&value), sizeof(value));
    const int bytes = _pixel.depth() / 8;
    key.append(reinterpret_cast<const char *>(_pixel.constBits()), bytes);
    if (const auto cached = cache.object(key))
        return *cached;
    QByteArray row(_size.width() * bytes, Qt::Uninitialized);
    for (int x = 0; x < _size.width(); ++x)
        std::memcpy(row.data() + x * bytes, _pixel.constBits(), size_t(bytes));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (int y = 0; y < _size.height(); ++y)
        hash.addData(row);
    const auto result = hash.result();
    cache.insert(key, new QByteArray(result));
    return result;
}

bool isUniformRegion(const QImage &_image, const QRect &_bounds, const uchar *_pixel)
{
    const int   bytes = _image.depth() / 8;
    const int   x     = _bounds.x() * bytes;
    const auto *first = _image.constScanLine(_bounds.top()) + x;
    if (_pixel && std::memcmp(first, _pixel, size_t(bytes)) != 0)
        return false;
    // Verify the first row by doubling its known uniform prefix, then compare
    // whole rows. This ignores scanline padding and keeps native pixel bits.
    for (int prefix = 1; prefix < _bounds.width(); prefix *= 2)
        if (std::memcmp(first, first + prefix * bytes,
                        size_t(qMin(prefix, _bounds.width() - prefix)) * bytes) != 0)
            return false;
    for (int y = _bounds.top() + 1; y <= _bounds.bottom(); ++y)
        if (std::memcmp(first, _image.constScanLine(y) + x, size_t(_bounds.width()) * bytes) != 0)
            return false;
    return true;
}

// Only uniform tiles can be represented by the shared background. Choose the
// native pixel that saves the most allocated bytes, including partial edge tiles.
QImage importedBackground(const QImage &_image)
{
    const int              bytes = _image.depth() / 8;
    QHash<quint64, qint64> weights;
    qint64                 bestWeight = 0;
    QPoint                 best;
    bool                   found = false;
    for (int top = 0; top < _image.height(); top += TiledImage::TileSize)
        for (int left = 0; left < _image.width(); left += TiledImage::TileSize) {
            const int   width   = qMin(TiledImage::TileSize, _image.width() - left);
            const int   height  = qMin(TiledImage::TileSize, _image.height() - top);
            if (!isUniformRegion(_image, {left, top, width, height}, nullptr))
                continue;
            quint64 pixel = 0;
            std::memcpy(&pixel, _image.constScanLine(top) + left * bytes, size_t(bytes));
            const auto weight = (weights[pixel] += qint64((width * bytes + 3) & ~3) * height);
            if (weight > bestWeight) {
                bestWeight = weight;
                best       = {left, top};
                found      = true;
            }
        }
    return found ? _image.copy(QRect(best, QSize(1, 1))) : QImage{};
}

bool opaquePixels(const QImage &image)
{
    if (image.isNull())
        return false;
    if (!image.hasAlphaChannel())
        return true;
    for (int y = 0; y < image.height(); ++y) {
        if (image.format() == QImage::Format_ARGB32_Premultiplied) {
            const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x)
                if (qAlpha(row[x]) != 255)
                    return false;
        } else if (image.format() == QImage::Format_RGBA64_Premultiplied) {
            const auto *row = reinterpret_cast<const QRgba64 *>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x)
                if (row[x].alpha() != 65535)
                    return false;
        } else
            return false;
    }
    return true;
}
struct Tile {
    mutable QMutex                     mutex;
    QImage                             pixels;
    std::shared_ptr<Tile>              parent;
    QRect                              patchRect;
    int                                depth      = 0;
    quint64                            id         = nextTileId.fetch_add(1);
    bool                               diskBacked = false;
    bool                               opaque     = false;
    mutable QHash<quint32, QByteArray> partialKeys;
    explicit Tile(QImage _pixels) : pixels(std::move(_pixels)), opaque(opaquePixels(pixels))
    {
    }
    QImage image() const
    {
        std::shared_ptr<Tile> base;
        QImage                patch;
        QRect                 rect;
        {
            QMutexLocker locker(&mutex);
            if (!parent)
                return pixels;
            base  = parent;
            patch = pixels;
            rect  = patchRect;
        }
        // The first write detaches the base. Each newer delta can reuse that image.
        QImage result = base->image();
        if (!result.isDetached())
            result = result.copy();
        if (result.isNull())
            return {};
        const int bytes = result.depth() / 8;
        for (int y = 0; y < rect.height(); ++y)
            std::memcpy(result.scanLine(rect.y() + y) + rect.x() * bytes, patch.constScanLine(y),
                        size_t(rect.width()) * bytes);
        return result;
    }
    bool copyRegion(const QRect &_region, QImage &_output, const QPoint &_destination) const
    {
        std::shared_ptr<Tile> base;
        QImage                source;
        QRect                 sourceRect;
        {
            QMutexLocker locker(&mutex);
            source     = pixels;
            base       = parent;
            sourceRect = base ? patchRect : pixels.rect();
        }
        if (base && !sourceRect.contains(_region) &&
            !base->copyRegion(_region, _output, _destination))
            return false;
        if (source.isNull())
            return false;
        const QRect overlap = _region.intersected(sourceRect);
        const int   bytes   = source.depth() / 8;
        const auto  offset  = _destination + overlap.topLeft() - _region.topLeft();
        for (int row = 0; row < overlap.height(); ++row)
            std::memcpy(_output.scanLine(offset.y() + row) + offset.x() * bytes,
                        source.constScanLine(overlap.y() - sourceRect.y() + row) +
                            (overlap.x() - sourceRect.x()) * bytes,
                        size_t(overlap.width()) * bytes);
        return true;
    }
    void blocks(QHash<qint64, qint64> &result, bool heap) const
    {
        std::shared_ptr<Tile> base;
        {
            QMutexLocker locker(&mutex);
            const qint64 key = -qint64(id);
            if (result.contains(key))
                return;
            if (!heap || !diskBacked)
                result.insert(key, pixels.sizeInBytes());
            base = parent;
        }
        if (base)
            base->blocks(result, heap);
    }
    QByteArray partialKey(const QRect &_region) const
    {
        const quint32 key = (quint32(_region.x()) << 24) | (quint32(_region.y()) << 16) |
                            (quint32(_region.width() - 1) << 8) | quint32(_region.height() - 1);
        QImage         source;
        QImage::Format patchFormat = QImage::Format_Invalid;
        {
            QMutexLocker locker(&mutex);
            if (auto found = partialKeys.constFind(key); found != partialKeys.cend())
                return found.value();
            if (parent)
                patchFormat = pixels.format();
            else
                source = pixels;
        }
        QRect bounds = _region;
        if (patchFormat != QImage::Format_Invalid) {
            source = QImage(_region.size(), patchFormat);
            if (source.isNull() || !copyRegion(_region, source, {}))
                return {};
            bounds = source.rect();
        }
        if (source.isNull())
            return {};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        const int          bytes = source.depth() / 8;
        for (int y = bounds.top(); y <= bounds.bottom(); ++y)
            hash.addData(QByteArrayView(
                reinterpret_cast<const char *>(source.constScanLine(y) + bounds.x() * bytes),
                bounds.width() * bytes));
        const auto   result = hash.result();
        QMutexLocker locker(&mutex);
        if (partialKeys.size() >= 8)
            partialKeys.clear();
        partialKeys.insert(key, result);
        return result;
    }
};
// Small native changes retain a bounded predecessor chain and only their true
// changed rectangle. No reconstructed full image is kept as a hidden cache.
std::shared_ptr<Tile> changedTile(const QImage &image, const std::shared_ptr<Tile> &previous,
                                  const QImage &old, const QRect &dirty, bool preserve = true)
{
    auto full = [&] { return std::make_shared<Tile>(image); };
    if (previous && !preserve) {
        std::shared_ptr<Tile> parent;
        bool                  disk;
        {
            QMutexLocker lock(&previous->mutex);
            parent = previous->parent;
            disk   = previous->diskBacked;
        }
        // Fold disposable gesture versions into the last retained predecessor.
        if (parent)
            return changedTile(image, parent, parent->image(), image.rect());
        if (!disk)
            return full();
    }
    if (!previous || old.size() != image.size() || old.format() != image.format())
        return full();
    int depth;
    {
        QMutexLocker lock(&previous->mutex);
        depth = previous->depth;
    }
    if (depth >= 8)
        return full();
    const int   bytes = image.depth() / 8;
    const QRect area  = dirty.intersected(image.rect());
    int         left = image.width(), right = -1, top = image.height(), bottom = -1;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        const auto *a = image.constScanLine(y), *b = old.constScanLine(y);
        if (!std::memcmp(a + area.x() * bytes, b + area.x() * bytes, size_t(area.width()) * bytes))
            continue;
        top    = qMin(top, y);
        bottom = y;
        for (int x = area.left(); x <= area.right(); ++x)
            if (std::memcmp(a + x * bytes, b + x * bytes, size_t(bytes))) {
                left  = qMin(left, x);
                right = qMax(right, x);
            }
    }
    if (right < left)
        return previous;
    const QRect patch(left, top, right - left + 1, bottom - top + 1);
    if (qint64(patch.width()) * patch.height() * 4 > qint64(image.width()) * image.height())
        return full();
    QImage pixels = image.copy(patch);
    if (pixels.isNull())
        return full();
    auto result = std::make_shared<Tile>(std::move(pixels));
    bool parentOpaque;
    {
        QMutexLocker lock(&previous->mutex);
        parentOpaque = previous->opaque;
    }
    result->opaque    = result->opaque && (parentOpaque || opaquePixels(image));
    result->parent    = previous;
    result->patchRect = patch;
    result->depth     = depth + 1;
    return result;
}
} // namespace

struct TiledImage::Data {
    QSize                             size;
    QImage                            background;
    QHash<int, std::shared_ptr<Tile>> tiles;
    qsizetype                         nonOpaqueTiles = 0;

    // In-place painting removes a tile from this count before changing its opacity.
    std::shared_ptr<Tile> takeTile(int _key)
    {
        auto tile = tiles.take(_key);
        if (tile && !tile->opaque)
            --nonOpaqueTiles;
        return tile;
    }
    void setTile(int _key, std::shared_ptr<Tile> _tile)
    {
        const auto previous = tiles.constFind(_key);
        if (previous != tiles.cend() && !previous.value()->opaque)
            --nonOpaqueTiles;
        if (_tile) {
            nonOpaqueTiles += !_tile->opaque;
            tiles.insert(_key, std::move(_tile));
        } else
            tiles.remove(_key);
    }
    int                               columns() const
    {
        return (size.width() + TileSize - 1) / TileSize;
    }
    bool hasEveryTile() const
    {
        return tiles.size() == columns() * ((size.height() + TileSize - 1) / TileSize);
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
    QImage createImage(const QSize &_size) const
    {
        QImage image(_size, background.format());
        if (image.isNull())
            return {};
        image.setColorSpace(background.colorSpace());
        image.setDevicePixelRatio(background.devicePixelRatio());
        image.setDotsPerMeterX(background.dotsPerMeterX());
        image.setDotsPerMeterY(background.dotsPerMeterY());
        return image;
    }
    QImage blank(const QSize &_size) const
    {
        auto image = createImage(_size);
        if (image.isNull())
            return {};
        if (background.format() == QImage::Format_Grayscale8)
            image.fill(background.constScanLine(0)[0]);
        else if (background.format() == QImage::Format_ARGB32_Premultiplied) {
            quint32 pixel = 0;
            std::memcpy(&pixel, background.constBits(), sizeof(pixel));
            image.fill(pixel);
        } else {
            quint64 pixel = 0;
            std::memcpy(&pixel, background.constBits(), sizeof(pixel));
            for (int y = 0; y < image.height(); ++y)
                std::fill_n(reinterpret_cast<quint64 *>(image.scanLine(y)), image.width(), pixel);
        }
        return image;
    }
    QImage readBuffer(const QSize &_size) const
    {
        // Dense planes overwrite every output pixel. Only skip initialization
        // when there is also no scanline padding left over.
        const bool padding =
            background.format() == QImage::Format_Grayscale8 && _size.width() % 4 != 0;
        return hasEveryTile() && !padding ? createImage(_size) : blank(_size);
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
        for (const auto &tile : std::as_const(m_data->tiles))
            tile->blocks(result, false);
    return result;
}
QHash<qint64, qint64> TiledImage::heapBlocks() const
{
    QHash<qint64, qint64> result;
    if (m_data)
        for (const auto &tile : std::as_const(m_data->tiles))
            tile->blocks(result, true);
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
                const auto tile    = m_data->tiles.value(m_data->key(x, y));
                const auto bounds  = m_data->rect(x, y);
                const auto overlap = region.intersected(bounds);
                if (overlap != bounds) {
                    // A neighbouring GPU region often needs just a border of
                    // this storage tile. Its other pixels must not invalidate it.
                    append(-1);
                    key += tile ? tile->partialKey(overlap.translated(-bounds.topLeft()))
                                : uniformRegionKey(m_data->background, overlap.size());
                } else if (tile) {
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
    for (auto value : storageBlocks())
        bytes += value;
    return bytes;
}
qint64 TiledImage::heapBytes() const
{
    qint64 bytes = 0;
    for (auto value : heapBlocks())
        bytes += value;
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
    if (!reuse) {
        const auto background = importedBackground(_image);
        if (!background.isNull())
            initial = TiledImage(_image.size(), background);
    }
    auto next = reuse ? std::make_shared<Data>(*m_data) : initial.m_data;
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
            const QImage pixels   = previous ? previous->image() : QImage{};
            if (previous) {
                if (pixels.isNull())
                    return false;
                bool equal = true;
                for (int y = 0; y < bounds.height() && equal; ++y)
                    equal =
                        std::memcmp(pixels.constScanLine(y),
                                    _image.constScanLine(bounds.y() + y) + bounds.x() * pixelBytes,
                                    size_t(bounds.width()) * pixelBytes) == 0;
                if (equal)
                    continue;
            }
            if (isUniformRegion(_image, bounds, next->background.constBits())) {
                if (previous)
                    next->setTile(key, {});
                continue;
            }
            const QImage replacement = _image.copy(bounds);
            if (replacement.isNull())
                return false;
            next->setTile(key, changedTile(replacement, previous, pixels, replacement.rect()));
        }
    m_data = std::move(next);
    return true;
}

TiledImage TiledImage::fromBands(const QSize &_size, QImage::Format _format,
                                 const std::function<QImage(const QRect &)> &_read)
{
    TiledImage result(_size, _format, Qt::transparent);
    if (result.isNull() || !_read)
        return {};
    auto &data = *result.m_data;
    struct Uniform {
        int     key;
        quint64 value;
        QImage  pixel;
    };
    QVector<Uniform>       uniforms;
    QHash<quint64, qint64> weights;
    qint64                 bestWeight = 0;
    quint64                bestValue  = 0;
    const int              bytes      = data.background.depth() / 8;
    for (int top = 0; top < _size.height(); top += TileSize) {
        const QRect  area(0, top, _size.width(), qMin(TileSize, _size.height() - top));
        const QImage band = _read(area);
        if (band.size() != area.size() || band.format() != _format)
            return {};
        if (top == 0) {
            data.background = band.copy(0, 0, 1, 1);
            if (data.background.isNull())
                return {};
            data.background.fill(Qt::transparent);
        }
        for (int column = 0; column < data.columns(); ++column) {
            const int   key     = data.key(column, top / TileSize);
            const QRect bounds  = data.rect(column, top / TileSize).translated(0, -top);
            const bool  uniform = isUniformRegion(band, bounds, nullptr);
            const auto  pixels = band.copy(uniform ? QRect(bounds.topLeft(), QSize(1, 1)) : bounds);
            if (pixels.isNull())
                return {};
            if (!uniform) {
                data.setTile(key, std::make_shared<Tile>(pixels));
                continue;
            }
            quint64 value = 0;
            std::memcpy(&value, pixels.constBits(), size_t(bytes));
            const auto weight =
                (weights[value] += qint64((bounds.width() * bytes + 3) & ~3) * bounds.height());
            if (weight > bestWeight) {
                bestWeight      = weight;
                bestValue       = value;
                data.background = pixels;
            }
            uniforms.append({key, value, pixels});
        }
    }
    // Keep the same dominant compact background as a whole-image import,
    // without allocating uniform tiles before that background is known.
    Data minority;
    for (const auto &tile : uniforms)
        if (tile.value != bestValue) {
            minority.background = tile.pixel;
            const auto pixels   = minority.blank(
                data.rect(tile.key % data.columns(), tile.key / data.columns()).size());
            if (pixels.isNull())
                return {};
            data.setTile(tile.key, std::make_shared<Tile>(pixels));
        }
    return result;
}

TiledImage TiledImage::convertedTo(QImage::Format               _format,
                                   const std::function<bool()> &_continue) const
{
    if (!m_data || (_continue && !_continue()))
        return {};
    if (_format == format())
        return *this;
    if (_format != QImage::Format_ARGB32_Premultiplied &&
        _format != QImage::Format_RGBA64_Premultiplied && _format != QImage::Format_Grayscale8)
        return {};
    const auto background = m_data->background.convertToFormat(_format);
    if (background.isNull())
        return {};
    TiledImage converted(size(), background);
    if (converted.isNull())
        return {};
    for (auto it = m_data->tiles.cbegin(); it != m_data->tiles.cend(); ++it) {
        if (_continue && !_continue())
            return {};
        const QImage pixels = it.value()->image().convertToFormat(_format);
        if (pixels.isNull())
            return {};
        if (!isUniformRegion(pixels, pixels.rect(), background.constBits()))
            converted.m_data->setTile(it.key(), std::make_shared<Tile>(pixels));
    }
    if (_continue && !_continue())
        return {};
    return converted;
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
    QImage result = m_data->readBuffer(region.size());
    if (result.isNull())
        return {};
    for (int row = region.top() / TileSize; row <= region.bottom() / TileSize; ++row)
        for (int column = region.left() / TileSize; column <= region.right() / TileSize; ++column) {
            const auto tile = m_data->tiles.value(m_data->key(column, row));
            if (!tile)
                continue;
            const QRect bounds  = m_data->rect(column, row);
            const QRect overlap = bounds.intersected(region);
            if (!tile->copyRegion(overlap.translated(-bounds.topLeft()), result,
                                  overlap.topLeft() - region.topLeft()))
                return {};
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
    QImage result = m_data->readBuffer(target);
    if (result.isNull() || m_data->tiles.isEmpty())
        return result;
    using PhotoCrap::ImageSampling::nearestAxis;
    const bool wide = format() == QImage::Format_RGBA64_Premultiplied;
    const auto xs   = nearestAxis(size().width(), target.width(), false,
                                  size().height() != target.height(), wide);
    const auto ys =
        nearestAxis(size().height(), target.height(), true, size().width() != target.width(), wide);
    if (xs.size() != target.width() || ys.size() != target.height())
        return {};
    struct TileRun {
        int index, first, end;
    };
    const auto tileRuns = [](const QVector<int> &_samples) {
        QVector<TileRun> runs;
        for (int first = 0; first < _samples.size();) {
            const int index = _samples[first] / TileSize;
            int       end   = first + 1;
            while (end < _samples.size() && _samples[end] / TileSize == index)
                ++end;
            runs.append({index, first, end});
            first = end;
        }
        return runs;
    };
    const auto columns = tileRuns(xs);
    const int  bytes   = result.depth() / 8;
    // Visit each sampled tile once, even if its native pixels are stored as deltas.
    for (const auto &row : tileRuns(ys))
        for (const auto &column : columns) {
            const auto tile = m_data->tiles.value(m_data->key(column.index, row.index));
            if (!tile)
                continue;
            const QImage pixels = tile->image();
            if (pixels.isNull())
                return {};
            for (int y = row.first; y < row.end; ++y) {
                auto       *output = result.scanLine(y);
                const auto *input  = pixels.constScanLine(ys[y] - row.index * TileSize);
                for (int x = column.first; x < column.end; ++x)
                    std::memcpy(output + x * bytes,
                                input + (xs[x] - column.index * TileSize) * bytes, size_t(bytes));
            }
        }
    return result;
}

bool TiledImage::paint(const QRect &_region, const std::function<void(QPainter &)> &_paint)
{
    return paintRegion(_region, _paint, true);
}

bool TiledImage::knownOpaque() const
{
    return m_data && m_data->background.devicePixelRatio() == 1. && m_data->nonOpaqueTiles == 0 &&
           (m_data->background.pixelColor(0, 0).alphaF() == 1. || m_data->hasEveryTile());
}

bool TiledImage::sharedStorage() const
{
    return m_data && (m_data.use_count() > 1 || !m_data->tiles.isDetached());
}

bool TiledImage::writeRegion(const QPoint &_origin, const QImage &_pixels, bool *_changed,
                             bool _preserveSnapshots)
{
    if (_changed)
        *_changed = false;
    if (!m_data || _pixels.isNull() || _pixels.format() != format())
        return false;
    const QRect region = QRect(_origin, _pixels.size()).intersected(QRect(QPoint{}, size()));
    if (region.isEmpty())
        return true;
    const bool            shared = _preserveSnapshots && sharedStorage();
    std::shared_ptr<Data> next;
    const int             bytes = _pixels.depth() / 8;
    for (int y = region.top() / TileSize; y <= region.bottom() / TileSize; ++y)
        for (int x = region.left() / TileSize; x <= region.right() / TileSize; ++x) {
            const QRect  bounds = m_data->rect(x, y), overlap = region.intersected(bounds);
            const auto   previous = m_data->tiles.value(m_data->key(x, y));
            const bool   preserve = shared || (previous && previous.use_count() > 2);
            const QImage old      = previous ? previous->image() : QImage{};
            if (previous && old.isNull())
                return false;
            const QRect source = overlap.translated(-_origin);
            bool equal = true;
            if (previous) {
                for (int row = 0; row < overlap.height() && equal; ++row)
                    equal =
                        std::memcmp(old.constScanLine(overlap.y() - bounds.y() + row) +
                                        (overlap.x() - bounds.x()) * bytes,
                                    _pixels.constScanLine(source.y() + row) + source.x() * bytes,
                                    size_t(overlap.width()) * bytes) == 0;
            } else
                equal = isUniformRegion(_pixels, source, m_data->background.constBits());
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
            if (isUniformRegion(image, image.rect(), next->background.constBits()))
                next->setTile(next->key(x, y), {});
            else
                next->setTile(next->key(x, y),
                              changedTile(image, previous, old,
                                          overlap.translated(-bounds.topLeft()), preserve));
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
    const bool shared  = m_data.use_count() > 1 || !m_data->tiles.isDetached();
    const bool inPlace = !_preserve && m_data.use_count() == 1;
    auto       next    = inPlace ? m_data : std::make_shared<Data>(*m_data);
    for (int row = region.top() / TileSize; row <= region.bottom() / TileSize; ++row)
        for (int column = region.left() / TileSize; column <= region.right() / TileSize; ++column) {
            const int             key    = next->key(column, row);
            const QRect           bounds = next->rect(column, row);
            auto                  tile   = next->takeTile(key);
            std::shared_ptr<Tile> previous;
            QImage                old;
            bool                  preserve = shared;
            if (!tile)
                tile = std::make_shared<Tile>(next->blank(bounds.size()));
            else if (!inPlace || tile.use_count() != 1) {
                preserve = shared || tile.use_count() > 2;
                previous = tile;
                old      = tile->image();
                tile     = std::make_shared<Tile>(old);
            } else if (tile->parent)
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
                tile->opaque     = opaquePixels(tile->pixels);
                tile->partialKeys.clear();
            }
            if (previous)
                tile =
                    changedTile(tile->image(), previous, old,
                                region.intersected(bounds).translated(-bounds.topLeft()), preserve);
            next->setTile(key, std::move(tile));
        }
    m_data = std::move(next);
    return true;
}

bool TiledImage::offload(ScratchImageCache &_cache, const std::function<bool()> &_continue) const
{
    if (!m_data)
        return true;
    bool complete = true;
    for (const auto &tile : std::as_const(m_data->tiles)) {
        if (_continue && !_continue())
            return false;
        QImage  image;
        QString key;
        {
            QMutexLocker locker(&tile->mutex);
            if (tile->diskBacked)
                continue;
            key = QStringLiteral("tile/%1").arg(tile->id);
        }
        image = tile->image();
        if (image.isNull()) {
            complete = false;
            continue;
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
            tile->parent.reset();
            tile->patchRect = {};
            tile->depth     = 0;
        }
    }
    return complete;
}
