#include "BinaryProject.h"
#include <QCborMap>
#include <QColorSpace>
#include <QCryptographicHash>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <zstd.h>

namespace PhotoCrap::Storage
{
namespace
{
constexpr qint64 MaxFile = 8LL * 1024 * 1024 * 1024;
bool             append(QIODevice &file, const QByteArray &bytes)
{
    return !bytes.isEmpty() && file.pos() >= 0 && file.pos() <= MaxFile - bytes.size() &&
           file.write(bytes) == bytes.size();
}
QByteArray fetch(QIODevice &file, qint64 offset, qint64 count, const Continue &progress)
{
    if (offset < 0 || count <= 0 || offset > file.size() || count > file.size() - offset ||
        !file.seek(offset))
        return {};
    QByteArray result;
    result.reserve(count);
    while (count > 0) {
        if (progress && !progress())
            return {};
        const auto amount = std::min<qint64>(count, 16384);
        auto       part   = file.read(amount);
        if (part.size() != amount)
            return {};
        result += part;
        count -= amount;
    }
    return result;
}
QImage::Format nativeFormat(int bpp)
{
    return bpp == 1   ? QImage::Format_Grayscale8
           : bpp == 4 ? QImage::Format_RGBA8888_Premultiplied
                      : QImage::Format_RGBA64_Premultiplied;
}
QByteArray pixels(QImage image, int bpp)
{
    image = image.convertToFormat(nativeFormat(bpp));
    QByteArray raw(image.width() * image.height() * bpp, Qt::Uninitialized);
    for (int y = 0; y < image.height(); ++y) {
        char *out = raw.data() + y * image.width() * bpp;
        std::memcpy(out, image.constScanLine(y), image.width() * bpp);
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
        if (bpp == 8)
            for (int x = 0; x < image.width() * 4; ++x) {
                const auto value = reinterpret_cast<const quint16 *>(image.constScanLine(y))[x];
                qToLittleEndian(value, out + x * 2);
            }
#endif
    }
    return raw;
}
QImage image(const QByteArray &raw, int w, int h, int bpp, QImage::Format format)
{
    QImage result(w, h, nativeFormat(bpp));
    if (result.isNull() || raw.size() != qint64(w) * h * bpp)
        return {};
    for (int y = 0; y < h; ++y) {
        auto *out = result.scanLine(y);
        std::memcpy(out, raw.constData() + y * w * bpp, w * bpp);
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
        if (bpp == 8)
            for (int x = 0; x < w * 4; ++x)
                reinterpret_cast<quint16 *>(out)[x] =
                    qFromLittleEndian<quint16>(raw.constData() + (y * w * 4 + x) * 2);
#endif
    }
    result = result.convertToFormat(format);
    if (bpp != 1)
        result.setColorSpace(QColorSpace::SRgb);
    return result;
}
// Reversible horizontal byte deltas followed by channel/byte shuffling. In
// 16-bit images the high and low bytes have independent entropy streams.
int paeth(int left, int up, int corner)
{
    const int estimate = left + up - corner;
    const int a = std::abs(estimate - left), b = std::abs(estimate - up),
              c = std::abs(estimate - corner);
    return a <= b && a <= c ? left : b <= c ? up : corner;
}
void colorTransform(QByteArray &raw, int bpp, bool inverse)
{
    if (bpp == 1)
        return;
    auto *data = reinterpret_cast<uchar *>(raw.data());
    for (qsizetype i = 0; i < raw.size(); i += bpp) {
        if (bpp == 4) {
            data[i]     = uchar(inverse ? data[i] + data[i + 1] : data[i] - data[i + 1]);
            data[i + 2] = uchar(inverse ? data[i + 2] + data[i + 1] : data[i + 2] - data[i + 1]);
        } else {
            const auto green = qFromLittleEndian<quint16>(data + i + 2);
            const auto red   = qFromLittleEndian<quint16>(data + i),
                       blue  = qFromLittleEndian<quint16>(data + i + 4);
            qToLittleEndian<quint16>(quint16(inverse ? red + green : red - green), data + i);
            qToLittleEndian<quint16>(quint16(inverse ? blue + green : blue - green), data + i + 4);
        }
    }
}
QByteArray predict(const QByteArray &raw, int w, int h, int bpp, bool inverse, int mode)
{
    QByteArray result(raw.size(), Qt::Uninitialized);
    QByteArray transformed;
    if (!inverse && mode == 2) {
        transformed = raw;
        colorTransform(transformed, bpp, false);
    }
    const auto *src =
        reinterpret_cast<const uchar *>((transformed.isEmpty() ? raw : transformed).constData());
    auto *dst = reinterpret_cast<uchar *>(result.data());
    for (int lane = 0; lane < bpp; ++lane)
        for (int y = 0; y < h; ++y) {
            uchar previous = 0;
            for (int x = 0; x < w; ++x) {
                const int   interleaved = (y * w + x) * bpp + lane;
                const int   planar      = lane * w * h + y * w + x;
                const auto *history     = inverse ? dst : src;
                const int   up          = y ? history[interleaved - w * bpp] : 0;
                const int   corner      = x && y ? history[interleaved - w * bpp - bpp] : 0;
                const int   prediction  = mode == 2 ? paeth(previous, up, corner) : previous;
                if (inverse) {
                    previous         = uchar(prediction + src[planar]);
                    dst[interleaved] = previous;
                } else {
                    dst[planar] = uchar(src[interleaved] - prediction);
                    previous    = src[interleaved];
                }
            }
        }
    if (inverse && mode == 2)
        colorTransform(result, bpp, true);
    return result;
}
} // namespace
QByteArray packProjectBytes(const QByteArray &bytes)
{
    std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context(ZSTD_createCCtx(), ZSTD_freeCCtx);
    if (!context)
        return {};
    ZSTD_CCtx_setParameter(context.get(), ZSTD_c_compressionLevel, 3);
    ZSTD_CCtx_setParameter(context.get(), ZSTD_c_checksumFlag, 1);
    QByteArray result(qsizetype(ZSTD_compressBound(bytes.size())), Qt::Uninitialized);
    const auto count = ZSTD_compress2(context.get(), result.data(), result.size(),
                                      bytes.constData(), bytes.size());
    if (ZSTD_isError(count))
        return {};
    result.resize(qsizetype(count));
    return result;
}
QByteArray unpackProjectBytes(const QByteArray &bytes, qint64 size)
{
    if (size <= 0 || size > 32 * 1024 * 1024 ||
        ZSTD_getFrameContentSize(bytes.constData(), bytes.size()) != quint64(size))
        return {};
    std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> context(ZSTD_createDCtx(), ZSTD_freeDCtx);
    if (!context)
        return {};
    ZSTD_DCtx_setParameter(context.get(), ZSTD_d_windowLogMax, 23);
    QByteArray result(size, Qt::Uninitialized);
    const auto count = ZSTD_decompressDCtx(context.get(), result.data(), result.size(),
                                           bytes.constData(), bytes.size());
    return !ZSTD_isError(count) && count == size_t(size) ? result : QByteArray{};
}
BinaryProjectWriter::BinaryProjectWriter(QIODevice &file, QByteArray preview, Continue progress)
    : m_file(file), m_continue(std::move(progress))
{
    m_header.preview = preview.size();
    m_ok             = preview.size() <= qint64(BinaryHeader::MaxPreview) && file.pos() == 0 &&
           append(file, QByteArray(64, '\0')) && (preview.isEmpty() || append(file, preview));
}
int BinaryProjectWriter::block(const QByteArray &raw, int w, int h, int bpp)
{
    if (!m_ok || raw.isEmpty() || (m_continue && !m_continue())) {
        m_ok = false;
        return -1;
    }
    const auto digest = QCryptographicHash::hash(raw, QCryptographicHash::Sha256);
    const auto key    = QByteArray::number(w) + ':' + QByteArray::number(h) + ':' +
                     QByteArray::number(bpp) + ':' + digest;
    if (auto found = m_hashes.constFind(key); found != m_hashes.cend())
        return *found;
    QByteArray packed;
    if (bpp)
        packed = packProjectBytes(predict(raw, w, h, bpp, false, 2));
    const bool  compressed = !packed.isEmpty() && packed.size() < raw.size();
    const auto &bytes      = compressed ? packed : raw;
    const auto  offset     = m_file.pos();
    if (!append(m_file, bytes)) {
        m_ok = false;
        return -1;
    }
    const int id = int(m_blocks.size());
    m_blocks.append(QCborArray{offset, qint64(bytes.size()), qint64(raw.size()), compressed ? 1 : 0,
                               compressed ? 2 : 0, w, h, bpp, digest});
    m_hashes.insert(key, id);
    return id;
}
int BinaryProjectWriter::original(const QByteArray &bytes)
{
    if (bytes.isEmpty() || bytes.size() > 100 * 1024 * 1024) {
        m_ok = false;
        return -1;
    }
    const int id   = int(m_planes.size());
    const int data = block(bytes, 0, 0, 0);
    m_planes.append(QCborArray{0, 0, 0, QByteArray{}, data, QCborArray{}});
    return data < 0 ? -1 : id;
}
int BinaryProjectWriter::plane(const PixelPlane &plane)
{
    if (plane.isNull())
        return -1;
    if (!plane.freeze()) {
        m_ok = false;
        return -1;
    }
    if (auto found = m_versions.constFind(plane.cacheKey()); found != m_versions.cend())
        return *found;
    const int  bpp = plane.format() == QImage::Format_Grayscale8                 ? 1
                     : QImage::toPixelFormat(plane.format()).bitsPerPixel() > 32 ? 8
                                                                                 : 4;
    const auto background =
        plane.isUniform() ? plane.uniformPixel() : plane.tiles().backgroundPixel();
    const QByteArray uniform = pixels(background, bpp);
    if (uniform.size() != bpp) {
        m_ok = false;
        return -1;
    }
    QCborArray tiles;
    if (!plane.isUniform())
        for (int y = 0; y < plane.height(); y += 256)
            for (int x = 0; x < plane.width(); x += 256) {
                if (m_continue && !m_continue()) {
                    m_ok = false;
                    return -1;
                }
                const QRect rect(x, y, std::min(256, plane.width() - x),
                                 std::min(256, plane.height() - y));
                const auto  tile = plane.tileImage(x / 256, y / 256);
                if (tile.size() == QSize(1, 1) && pixels(tile, bpp) == uniform) {
                    tiles.append(-1);
                    continue;
                }
                const auto raw  = pixels(plane.copy(rect), bpp);
                const int  data = block(raw, rect.width(), rect.height(), bpp);
                if (data < 0)
                    return -1;
                tiles.append(data);
            }
    if (plane.isUniform() && uniform.size() != bpp) {
        m_ok = false;
        return -1;
    }
    const QCborArray descriptor{plane.width(),
                                plane.height(),
                                bpp,
                                uniform,
                                tiles,
                                QCborArray{background.devicePixelRatio(),
                                           background.dotsPerMeterX(), background.dotsPerMeterY()}};
    const auto       hash =
        QCryptographicHash::hash(QCborValue(descriptor).toCbor(), QCryptographicHash::Sha256);
    if (auto found = m_planeHashes.constFind(hash); found != m_planeHashes.cend()) {
        m_versions.insert(plane.cacheKey(), *found);
        return *found;
    }
    const int id = int(m_planes.size());
    m_planes.append(descriptor);
    m_planeHashes.insert(hash, id);
    m_versions.insert(plane.cacheKey(), id);
    return id;
}
bool BinaryProjectWriter::finish(QJsonObject metadata)
{
    if (!m_ok || (m_continue && !m_continue()))
        return false;
    QCborMap manifest;
    manifest.insert(QStringLiteral("document"), QCborValue::fromJsonValue(metadata));
    manifest.insert(QStringLiteral("planes"), m_planes);
    manifest.insert(QStringLiteral("blocks"), m_blocks);
    const auto raw = QCborValue(manifest).toCbor();
    if (raw.isEmpty() || raw.size() > 32 * 1024 * 1024)
        return false;
    const auto packed = packProjectBytes(raw);
    if (packed.isEmpty() || packed.size() > 32 * 1024 * 1024)
        return false;
    m_header.index    = m_file.pos();
    m_header.unpacked = raw.size();
    m_header.packed   = packed.size();
    if (!append(m_file, packed))
        return false;
    m_header.length   = m_file.pos();
    const auto header = m_header.bytes();
    return m_file.seek(0) && m_file.write(header.data(), header.size()) == qint64(header.size()) &&
           m_file.seek(m_header.length);
}
bool BinaryProjectReader::read(QIODevice &file, Continue progress)
{
    m_file     = nullptr;
    m_continue = std::move(progress);
    m_decoded.clear();
    m_planes          = {};
    m_blocks          = {};
    metadata          = {};
    const auto header = fetch(file, 0, 64, m_continue);
    if (header.size() != 64 || !m_header.read(header.constData()) ||
        m_header.length != quint64(file.size()))
        return false;
    const auto       packed = fetch(file, m_header.index, m_header.packed, m_continue);
    const auto       raw    = unpackProjectBytes(packed, m_header.unpacked);
    QCborParserError error;
    const auto       value = QCborValue::fromCbor(raw, &error);
    if (raw.isEmpty() || error.error != QCborError::NoError || !value.isMap())
        return false;
    const auto manifest = value.toMap();
    if (!manifest.value(QStringLiteral("document")).isMap() ||
        !manifest.value(QStringLiteral("planes")).isArray() ||
        !manifest.value(QStringLiteral("blocks")).isArray())
        return false;
    metadata = manifest.value(QStringLiteral("document")).toJsonValue().toObject();
    m_planes = manifest.value(QStringLiteral("planes")).toArray();
    m_blocks = manifest.value(QStringLiteral("blocks")).toArray();
    if (m_planes.size() > 1536 || m_blocks.size() > 262144)
        return false;
    quint64 end = 64 + m_header.preview;
    for (const auto &value : m_blocks) {
        if (m_continue && !m_continue())
            return false;
        const auto b = value.toArray();
        if (!value.isArray() || b.size() != 9)
            return false;
        for (int i = 0; i < 8; ++i)
            if (!b[i].isInteger())
                return false;
        const qint64 offset = b[0].toInteger(), size = b[1].toInteger(), count = b[2].toInteger();
        const qint64 codec = b[3].toInteger(), predictor = b[4].toInteger(), w = b[5].toInteger(),
                     h = b[6].toInteger(), bpp = b[7].toInteger();
        if (offset < 0 || quint64(offset) != end || size <= 0 || count <= 0 ||
            quint64(size) > m_header.index - end || !b[8].isByteArray() ||
            b[8].toByteArray().size() != 32)
            return false;
        if (bpp == 0) {
            if (w || h || codec || predictor || size != count || size > 100 * 1024 * 1024)
                return false;
        } else if ((bpp != 1 && bpp != 4 && bpp != 8) || w < 1 || w > 256 || h < 1 || h > 256 ||
                   count != qint64(w) * h * bpp || (codec != 0 && codec != 1) ||
                   (codec == 0 ? predictor != 0 : (predictor != 1 && predictor != 2)) ||
                   (codec == 0 && size != count) || size > 512 * 1024)
            return false;
        end += size;
    }
    if (end != m_header.index)
        return false;
    for (const auto &value : m_planes) {
        if (!value.isArray())
            return false;
        const auto p = value.toArray();
        if (p.size() != 6 || !p[0].isInteger() || !p[1].isInteger() || !p[2].isInteger() ||
            !p[3].isByteArray())
            return false;
        const qint64 w = p[0].toInteger(), h = p[1].toInteger(), bpp = p[2].toInteger();
        if (bpp == 0) {
            if (w || h || !p[4].isInteger() || p[4].toInteger() < 0 ||
                p[4].toInteger() >= m_blocks.size())
                return false;
            continue;
        }
        if ((bpp != 1 && bpp != 4 && bpp != 8) || w < 1 || h < 1 || w > 32768 || h > 32768 ||
            w * h > 32LL * 1024 * 1024)
            return false;
        if (!p[5].isArray())
            return false;
        const auto metrics = p[5].toArray();
        if (metrics.size() != 3 || (!metrics[0].isDouble() && !metrics[0].isInteger()) ||
            !std::isfinite(metrics[0].toDouble()) || metrics[0].toDouble() < .01 ||
            metrics[0].toDouble() > 100 || !metrics[1].isInteger() || !metrics[2].isInteger() ||
            metrics[1].toInteger() < 0 || metrics[1].toInteger() > 1000000000 ||
            metrics[2].toInteger() < 0 || metrics[2].toInteger() > 1000000000)
            return false;
        const auto uniform = p[3].toByteArray();
        if (uniform.size() != bpp || !p[4].isArray())
            return false;
        const auto tiles = p[4].toArray();
        if (!tiles.isEmpty() && tiles.size() != ((w + 255) / 256) * ((h + 255) / 256))
            return false;
        for (const auto &tile : tiles)
            if (!tile.isInteger() || tile.toInteger() < -1 || tile.toInteger() >= m_blocks.size())
                return false;
    }
    m_file = &file;
    return true;
}
bool BinaryProjectReader::isOriginal(int id) const
{
    return id >= 0 && id < m_planes.size() && m_planes[id].toArray()[2].toInteger() == 0;
}
QByteArray BinaryProjectReader::block(int id, int w, int h, int bpp)
{
    if (!m_file || id < 0 || id >= m_blocks.size())
        return {};
    const auto b = m_blocks[id].toArray();
    if (b[5].toInteger() != w || b[6].toInteger() != h || b[7].toInteger() != bpp)
        return {};
    auto raw = fetch(*m_file, b[0].toInteger(), b[1].toInteger(), m_continue);
    if (b[3].toInteger() == 1) {
        raw = unpackProjectBytes(raw, b[2].toInteger());
        if (raw.isEmpty())
            return {};
        raw = predict(raw, w, h, bpp, true, b[4].toInteger());
    }
    if (raw.size() != b[2].toInteger() ||
        QCryptographicHash::hash(raw, QCryptographicHash::Sha256) != b[8].toByteArray())
        return {};
    return raw;
}
QByteArray BinaryProjectReader::original(int id)
{
    return isOriginal(id) ? block(m_planes[id].toArray()[4].toInteger(), 0, 0, 0) : QByteArray{};
}
PixelPlane BinaryProjectReader::plane(int id, QSize expected, QImage::Format format)
{
    if (!m_file || id < 0 || id >= m_planes.size() || isOriginal(id))
        return {};
    const auto  p = m_planes[id].toArray();
    const QSize size(p[0].toInteger(), p[1].toInteger());
    const int   bpp       = p[2].toInteger();
    const auto  uniform   = p[3].toByteArray();
    const int   requested = format == QImage::Format_Grayscale8                 ? 1
                            : QImage::toPixelFormat(format).bitsPerPixel() > 32 ? 8
                                                                                : 4;
    const auto  ids       = p[4].toArray();
    if (bpp != requested || (size != expected && (!ids.isEmpty() || expected != QSize(1, 1))))
        return {};
    const QString key = QString::number(id) + ':' + QString::number(expected.width()) + ':' +
                        QString::number(expected.height()) + ':' + QString::number(format);
    if (m_decoded.contains(key))
        return m_decoded.value(key);
    const auto metrics        = p[5].toArray();
    const auto restoreMetrics = [&](QImage &value) {
        value.setDevicePixelRatio(metrics[0].toDouble());
        value.setDotsPerMeterX(metrics[1].toInteger());
        value.setDotsPerMeterY(metrics[2].toInteger());
    };
    auto background = image(uniform, 1, 1, bpp, format);
    restoreMetrics(background);
    PixelPlane result;
    if (ids.isEmpty())
        result = expected == QSize(1, 1) ? PixelPlane(background)
                                         : PixelPlane::uniform(expected, background);
    else {
        TiledImage tiles(expected, background);
        int        next = 0;
        for (int y = 0; y < expected.height(); y += 256)
            for (int x = 0; x < expected.width(); x += 256) {
                const int w  = std::min(256, expected.width() - x),
                          h  = std::min(256, expected.height() - y);
                const int id = ids[next++].toInteger();
                if (id < 0)
                    continue;
                const auto raw = block(id, w, h, bpp);
                if (raw.isEmpty())
                    return {};
                auto tile = image(raw, w, h, bpp, format);
                restoreMetrics(tile);
                if (tile.isNull() || !tiles.writeRegion({x, y}, tile, nullptr, false))
                    return {};
            }
        result = PixelPlane::fromTiles(std::move(tiles));
    }
    if (!result.isNull())
        m_decoded.insert(key, result);
    return result;
}
} // namespace PhotoCrap::Storage
