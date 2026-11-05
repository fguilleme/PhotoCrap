#include "WebpExport.h"

#include <QScopeGuard>
#include <cstring>
#include <webp/encode.h>

namespace PhotoCrap::Storage
{
bool writeWebpBands(QIODevice &_output, const QSize &_size, const ImageBandReader &_read,
                    const ImageWriteProgress &_progress, QString &_error)
{
    _error.clear();
    if (!_output.isWritable() || !_read || _size.isEmpty() || _size.width() > WEBP_MAX_DIMENSION ||
        _size.height() > WEBP_MAX_DIMENSION ||
        qint64(_size.width()) * _size.height() > 32LL * 1024 * 1024) {
        _error = QStringLiteral("Invalid WebP dimensions or output");
        return false;
    }
    WebPConfig  config;
    WebPPicture picture;
    if (!WebPConfigInit(&config) || !WebPPictureInit(&picture)) {
        _error = QStringLiteral("Unable to initialize WebP encoder");
        return false;
    }
    config.quality     = 95;
    picture.width      = _size.width();
    picture.height     = _size.height();
    picture.colorspace = WEBP_YUV420A;
    const auto release = qScopeGuard([&] { WebPPictureFree(&picture); });
    if (!WebPPictureAlloc(&picture)) {
        _error = QStringLiteral("Unable to allocate WebP encoder planes");
        return false;
    }
    const int width      = _size.width();
    const int bandHeight = qMin(256, 4 * 1024 * 1024 / (width * 8)) & ~1;
    bool      hasAlpha   = false;
    for (int top = 0; top < _size.height(); top += bandHeight) {
        if (_progress && !_progress(500 * top / _size.height(), 1000))
            return false;
        const QRect area(0, top, width, qMin(bandHeight, _size.height() - top));
        const auto  pixels = _read(area).convertToFormat(QImage::Format_RGBA8888);
        if (pixels.size() != area.size()) {
            _error = QStringLiteral("Unable to read a WebP image band");
            return false;
        }
        WebPPicture band;
        if (!WebPPictureInit(&band))
            return false;
        const auto freeBand = qScopeGuard([&] { WebPPictureFree(&band); });
        band.width          = width;
        band.height         = area.height();
        // Every band starts on an even row, so 4:2:0 chroma cells never straddle bands.
        if (!WebPPictureImportRGBA(&band, pixels.constBits(), int(pixels.bytesPerLine()))) {
            _error = QStringLiteral("Unable to convert a WebP image band");
            return false;
        }
        hasAlpha |= band.a != nullptr;
        for (int y = 0; y < band.height; ++y) {
            std::memcpy(picture.y + (top + y) * picture.y_stride, band.y + y * band.y_stride,
                        size_t(width));
            auto *alpha = picture.a + (top + y) * picture.a_stride;
            if (band.a)
                std::memcpy(alpha, band.a + y * band.a_stride, size_t(width));
            else
                std::memset(alpha, 255, size_t(width));
        }
        for (int y = 0; y < (band.height + 1) / 2; ++y) {
            const int    offset = (top / 2 + y) * picture.uv_stride;
            const size_t bytes  = size_t((width + 1) / 2);
            std::memcpy(picture.u + offset, band.u + y * band.uv_stride, bytes);
            std::memcpy(picture.v + offset, band.v + y * band.uv_stride, bytes);
        }
    }
    if (!hasAlpha) {
        picture.colorspace = WEBP_YUV420;
        picture.a          = nullptr;
    }
    picture.custom_ptr = &_output;
    picture.writer     = [](const uint8_t *_bytes, size_t _size, const WebPPicture *_picture) {
        auto &output = *static_cast<QIODevice *>(_picture->custom_ptr);
        while (_size) {
            const auto written =
                output.write(reinterpret_cast<const char *>(_bytes), qint64(_size));
            if (written <= 0)
                return 0;
            _bytes += written;
            _size -= size_t(written);
        }
        return 1;
    };
    picture.user_data     = const_cast<ImageWriteProgress *>(&_progress);
    picture.progress_hook = [](int _percent, const WebPPicture *_picture) {
        const auto &progress = *static_cast<const ImageWriteProgress *>(_picture->user_data);
        return int(!progress || progress(500 + _percent * 5, 1000));
    };
    if (WebPEncode(&config, &picture))
        return true;
    _error = QStringLiteral("WebP encoding failed (%1)").arg(int(picture.error_code));
    return false;
}
} // namespace PhotoCrap::Storage
