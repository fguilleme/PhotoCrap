#include "StreamingImageWriter.h"
#include <QPainter>
#include <QScopeGuard>
#include <QtEndian>
#include <array>
#include <cstring>
#include <zlib.h>

namespace PhotoCrap::Storage
{
namespace
{
bool writeAll(QIODevice &_output, const char *_data, qint64 _size)
{
    while (_size > 0) {
        const auto count = _output.write(_data, _size);
        if (count <= 0)
            return false;
        _data += count;
        _size -= count;
    }
    return true;
}
bool chunk(QIODevice &_output, const char *_type, const QByteArray &_bytes)
{
    std::array<char, 4> length{}, checksum{};
    qToBigEndian(quint32(_bytes.size()), length.data());
    auto crc = crc32(0, reinterpret_cast<const Bytef *>(_type), 4);
    crc      = crc32(crc, reinterpret_cast<const Bytef *>(_bytes.constData()), uInt(_bytes.size()));
    qToBigEndian(quint32(crc), checksum.data());
    return writeAll(_output, length.data(), 4) && writeAll(_output, _type, 4) &&
           writeAll(_output, _bytes.constData(), _bytes.size()) &&
           writeAll(_output, checksum.data(), 4);
}
bool png(QIODevice &_output, const QSize &_size, int _bits, int _bandHeight,
         const ImageBandReader &_read, const ImageWriteProgress &_progress)
{
    const auto signature = QByteArray::fromHex("89504e470d0a1a0a");
    QByteArray header(13, 0);
    qToBigEndian(quint32(_size.width()), header.data());
    qToBigEndian(quint32(_size.height()), header.data() + 4);
    header[8] = char(_bits);
    header[9] = 6; // RGBA, non-interlaced.
    if (!writeAll(_output, signature.constData(), 8) || !chunk(_output, "IHDR", header) ||
        !chunk(_output, "sRGB", QByteArray(1, 0)))
        return false;
    z_stream stream{};
    if (deflateInit(&stream, Z_DEFAULT_COMPRESSION) != Z_OK)
        return false;
    const auto release = qScopeGuard([&] { deflateEnd(&stream); });
    QByteArray output(64 * 1024, 0);
    const auto compress = [&](const QByteArray &_row, bool _finish) {
        stream.next_in  = reinterpret_cast<Bytef *>(const_cast<char *>(_row.constData()));
        stream.avail_in = uInt(_row.size());
        int result      = Z_OK;
        do {
            stream.next_out  = reinterpret_cast<Bytef *>(output.data());
            stream.avail_out = uInt(output.size());
            result           = deflate(&stream, _finish ? Z_FINISH : Z_NO_FLUSH);
            if (result != Z_OK && result != Z_STREAM_END)
                return false;
            const auto count = output.size() - stream.avail_out;
            if (count && !chunk(_output, "IDAT", output.first(count)))
                return false;
        } while (stream.avail_in || (_finish && result != Z_STREAM_END));
        return true;
    };
    const int  bytes = _bits == 16 ? 8 : 4;
    QByteArray raw(_size.width() * bytes, 0), row(raw.size() + 1, 0);
    row[0] = 1; // Sub filter, preserving bytes exactly before DEFLATE.
    for (int top = 0; top < _size.height(); top += _bandHeight) {
        const QRect  region(0, top, _size.width(), qMin(_bandHeight, _size.height() - top));
        const QImage band = _read(region).convertToFormat(_bits == 16 ? QImage::Format_RGBA64
                                                                      : QImage::Format_RGBA8888);
        if (band.size() != region.size())
            return false;
        for (int y = 0; y < band.height(); ++y) {
            if (_progress && !_progress(top + y, _size.height()))
                return false;
            if (_bits == 16) {
                const auto *source = reinterpret_cast<const QRgba64 *>(band.constScanLine(y));
                for (int x = 0; x < band.width(); ++x) {
                    qToBigEndian(source[x].red(), raw.data() + x * 8);
                    qToBigEndian(source[x].green(), raw.data() + x * 8 + 2);
                    qToBigEndian(source[x].blue(), raw.data() + x * 8 + 4);
                    qToBigEndian(source[x].alpha(), raw.data() + x * 8 + 6);
                }
            } else
                std::memcpy(raw.data(), band.constScanLine(y), size_t(raw.size()));
            for (qsizetype x = 0; x < raw.size(); ++x)
                row[x + 1] = char(uchar(raw[x]) - (x >= bytes ? uchar(raw[x - bytes]) : 0));
            if (!compress(row, false))
                return false;
        }
    }
    return compress({}, true) && chunk(_output, "IEND", {});
}
bool bmp(QIODevice &_output, const QSize &_size, int _bandHeight, const ImageBandReader &_read,
         const ImageWriteProgress &_progress)
{
    const int  stride = (_size.width() * 3 + 3) & ~3;
    QByteArray header(54, 0);
    header[0] = 'B';
    header[1] = 'M';
    qToLittleEndian(quint32(54 + stride * _size.height()), header.data() + 2);
    qToLittleEndian(quint32(54), header.data() + 10);
    qToLittleEndian(quint32(40), header.data() + 14);
    qToLittleEndian(quint32(_size.width()), header.data() + 18);
    qToLittleEndian(quint32(_size.height()), header.data() + 22);
    qToLittleEndian(quint16(1), header.data() + 26);
    qToLittleEndian(quint16(24), header.data() + 28);
    qToLittleEndian(quint32(stride * _size.height()), header.data() + 34);
    qToLittleEndian(quint32(3780), header.data() + 38);
    qToLittleEndian(quint32(3780), header.data() + 42);
    if (!writeAll(_output, header.constData(), header.size()))
        return false;
    QByteArray row(stride, 0);
    for (int end = _size.height(); end > 0;) {
        const int    top = qMax(0, end - _bandHeight);
        const QRect  region(0, top, _size.width(), end - top);
        const QImage source = _read(region);
        if (source.size() != region.size())
            return false;
        QImage white(source.size(), QImage::Format_RGB32);
        white.fill(Qt::white);
        QPainter painter(&white);
        painter.drawImage(0, 0, source);
        painter.end();
        for (int y = white.height() - 1; y >= 0; --y) {
            if (_progress && !_progress(_size.height() - top - 1 - y, _size.height()))
                return false;
            const auto *pixels = reinterpret_cast<const QRgb *>(white.constScanLine(y));
            for (int x = 0; x < white.width(); ++x) {
                row[x * 3]     = char(qBlue(pixels[x]));
                row[x * 3 + 1] = char(qGreen(pixels[x]));
                row[x * 3 + 2] = char(qRed(pixels[x]));
            }
            if (!writeAll(_output, row.constData(), row.size()))
                return false;
        }
        end = top;
    }
    return true;
}
} // namespace
bool writeImageBands(QIODevice &_output, const QSize &_size, int _bits, bool _png,
                     const ImageBandReader &_read, const ImageWriteProgress &_progress,
                     QString &_error)
{
    _error.clear();
    if (!_output.isWritable() || !_read || (_bits != 8 && _bits != 16) || _size.width() < 1 ||
        _size.height() < 1 || _size.width() > 16384 || _size.height() > 16384 ||
        qint64(_size.width()) * _size.height() > 32LL * 1024 * 1024) {
        _error = QStringLiteral("Invalid streaming image dimensions or output");
        return false;
    }
    const int  band   = qBound(1, 4 * 1024 * 1024 / (_size.width() * (_bits == 16 ? 8 : 4)), 256);
    const bool result = _png ? png(_output, _size, _bits, band, _read, _progress)
                             : bmp(_output, _size, band, _read, _progress);
    if (!result)
        _error = _output.errorString();
    return result;
}
} // namespace PhotoCrap::Storage
