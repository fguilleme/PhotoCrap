#pragma once
#include "TiledImage.h"
#include <QColorSpace>
#include <atomic>

// Editable plane with immutable tile snapshots. Legacy whole-image operations
// may materialize a temporary QImage; freezing releases that temporary again.
class PixelPlane
{
public:
    PixelPlane() = default;
    PixelPlane(const QImage &image)
        : m_image(image), m_key(image.cacheKey()), m_readKey(image.cacheKey())
    {
    }
    bool isNull() const
    {
        return m_image.isNull() && m_tiles.isNull();
    }
    QSize size() const
    {
        return m_image.isNull() ? m_tiles.size() : m_image.size();
    }
    bool isUniform() const
    {
        return size() == QSize(1, 1) || (!m_dirty && !m_tiles.isNull() && m_tiles.tileCount() == 0);
    }
    QRect rect() const
    {
        return QRect(QPoint{}, size());
    }
    int width() const
    {
        return size().width();
    }
    int height() const
    {
        return size().height();
    }
    qint64 cacheKey() const
    {
        return m_dirty && m_image.cacheKey() != m_readKey ? m_image.cacheKey() : m_key;
    }
    QImage::Format format() const
    {
        return m_image.isNull() ? m_tiles.format() : m_image.format();
    }
    qint64 sizeInBytes() const
    {
        qint64 bytes = 0;
        for (auto b : storageBlocks())
            bytes += b;
        return bytes;
    }
    QColorSpace colorSpace() const
    {
        return m_image.isNull() ? m_tiles.colorSpace() : m_image.colorSpace();
    }
    QHash<qint64, qint64> storageBlocks() const
    {
        if (!m_dirty && !m_tiles.isNull())
            return m_tiles.storageBlocks();
        return m_image.isNull()
                   ? QHash<qint64, qint64>{}
                   : QHash<qint64, qint64>{{m_image.cacheKey(), m_image.sizeInBytes()}};
    }
    QHash<qint64, qint64> heapBlocks() const
    {
        auto blocks = m_tiles.heapBlocks();
        if (!m_image.isNull())
            blocks.insert(m_image.cacheKey(), m_image.sizeInBytes());
        return blocks;
    }
    operator QImage() const
    {
        return m_image.isNull() && !m_tiles.isNull() ? m_tiles.region(rect()) : m_image;
    }
    QImage &edit()
    {
        return write();
    }
    QImage copy(const QRect &r) const
    {
        if (!m_dirty && !m_tiles.isNull() && rect().contains(r))
            return m_tiles.region(r);
        return read().copy(r);
    }
    QImage copy() const
    {
        return read().copy();
    }
    QColor pixelColor(int x, int y) const
    {
        return !m_dirty && !m_tiles.isNull() ? m_tiles.region({x, y, 1, 1}).pixelColor(0, 0)
                                             : read().pixelColor(x, y);
    }
    const uchar *constScanLine(int y) const
    {
        return read().constScanLine(y);
    }
    QImage scaled(const QSize &s, Qt::AspectRatioMode a = Qt::IgnoreAspectRatio,
                  Qt::TransformationMode t = Qt::FastTransformation) const
    {
        if (!m_dirty && !m_tiles.isNull() && t == Qt::FastTransformation && !s.isEmpty()) {
            auto actual = size().scaled(s, a);
            actual.setWidth(qMax(1, actual.width()));
            actual.setHeight(qMax(1, actual.height()));
            return m_tiles.scaledFast(actual);
        }
        return QImage(*this).scaled(s, a, t);
    }
    QImage scaled(int w, int h, Qt::AspectRatioMode a = Qt::IgnoreAspectRatio,
                  Qt::TransformationMode t = Qt::FastTransformation) const
    {
        return scaled({w, h}, a, t);
    }
    QImage convertToFormat(QImage::Format f) const
    {
        return QImage(*this).convertToFormat(f);
    }
    bool convertTo(QImage::Format _format, const std::function<bool()> &_keepGoing = {})
    {
        if (format() == _format)
            return true;
        if (_keepGoing && !_keepGoing())
            return false;
        const auto native = [](QImage::Format _value) {
            return _value == QImage::Format_ARGB32_Premultiplied ||
                   _value == QImage::Format_RGBA64_Premultiplied ||
                   _value == QImage::Format_Grayscale8;
        };
        if (size() != QSize(1, 1) && native(format()) && native(_format)) {
            if (!freeze())
                return false;
            auto converted = m_tiles.convertedTo(_format, _keepGoing);
            if (converted.isNull())
                return false;
            m_key   = converted.backgroundPixel().cacheKey();
            m_tiles = std::move(converted);
            m_image = {};
            m_dirty = false;
            return true;
        }
        const QImage converted = QImage(*this).convertToFormat(_format);
        if (converted.isNull() || (_keepGoing && !_keepGoing()))
            return false;
        *this = PixelPlane(converted);
        return true;
    }
    void invertPixels()
    {
        write().invertPixels();
    }
    void setColorSpace(const QColorSpace &s)
    {
        if (colorSpace() != s)
            write().setColorSpace(s);
    }
    bool freeze() const
    {
        if (m_image.isNull())
            return true;
        if (m_image.size() == QSize(1, 1))
            return true;
        if (m_dirty || m_tiles.isNull()) {
            TiledImage next = m_tiles;
            if (!next.replace(m_image))
                return false;
            if (m_tiles.isNull() || next.storageBlocks() != m_tiles.storageBlocks() ||
                next.size() != m_tiles.size() || next.format() != m_tiles.format() ||
                next.colorSpace() != m_tiles.colorSpace())
                m_key = m_image.cacheKey();
            m_tiles = std::move(next);
            m_dirty = false;
        }
        m_image = {};
        return true;
    }
    bool shareStorage(const PixelPlane &other) const
    {
        if (cacheKey() != other.cacheKey() || other.m_tiles.isNull())
            return false;
        m_tiles = other.m_tiles;
        m_image = {};
        m_dirty = false;
        m_key   = other.m_key;
        return true;
    }
    QImage region(const QRect &r) const
    {
        if (size() == QSize(1, 1) && r != rect()) {
            const QImage pixel = tileImage(0, 0);
            if (pixel.isNull())
                return {};
            QImage image(r.size(), format());
            image.fill(pixel.pixelColor(0, 0));
            image.setColorSpace(pixel.colorSpace());
            image.setDevicePixelRatio(pixel.devicePixelRatio());
            image.setDotsPerMeterX(pixel.dotsPerMeterX());
            image.setDotsPerMeterY(pixel.dotsPerMeterY());
            return image;
        }
        return copy(r);
    }
    QImage tileImage(int column, int row) const
    {
        if (column < 0 || row < 0)
            return {};
        if (size() == QSize(1, 1))
            return m_image.isNull() ? m_tiles.tileImage(0, 0) : m_image;
        return freeze() ? m_tiles.tileImage(column, row) : QImage{};
    }
    QByteArray regionKey(const QRect &r) const
    {
        if (isNull())
            return "null";
        if (size() == QSize(1, 1)) {
            const QImage pixel = tileImage(0, 0);
            return QByteArray("uniform:") + QByteArray::number(int(format())) + ':' +
                   QByteArray(reinterpret_cast<const char *>(pixel.constBits()), pixel.depth() / 8);
        }
        if (!m_dirty && !m_tiles.isNull())
            return m_tiles.regionKey(r);
        return QByteArray::number(cacheKey()) + ':' + QByteArray::number(r.x()) + ':' +
               QByteArray::number(r.y()) + ':' + QByteArray::number(r.width()) + ':' +
               QByteArray::number(r.height());
    }
    const TiledImage &tiles() const
    {
        freeze();
        return m_tiles;
    }
    bool offload(ScratchImageCache &_cache, const std::function<bool()> &_continue = {}) const
    {
        return freeze() && m_tiles.offload(_cache, _continue);
    }
    bool writeRegion(const QPoint &origin, const QImage &pixels, const QSize &documentSize)
    {
        TiledImage next;
        if (size() == QSize(1, 1)) {
            next = TiledImage(documentSize, tileImage(0, 0));
        } else {
            if (!freeze())
                return false;
            next = m_tiles;
        }
        bool changed = false;
        if (next.isNull() || !next.writeRegion(origin, pixels, &changed))
            return false;
        if (!changed)
            return true;
        m_tiles = std::move(next);
        static std::atomic<qint64> nextVersion{1};
        m_key   = -nextVersion.fetch_add(1);
        m_dirty = false;
        m_image = {};
        return true;
    }

private:
    const QImage &read() const
    {
        if (m_image.isNull() && !m_tiles.isNull()) {
            m_image   = m_tiles.region(rect());
            m_readKey = m_image.cacheKey();
        }
        return m_image;
    }
    QImage &write()
    {
        read();
        m_dirty = true;
        return m_image;
    }
    mutable QImage     m_image;
    mutable TiledImage m_tiles;
    mutable qint64     m_key = 0, m_readKey = 0;
    mutable bool       m_dirty = false;
};
