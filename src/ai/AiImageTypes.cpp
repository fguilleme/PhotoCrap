#include "AiImageTypes.h"
#include "ImageUtils.h"
#include <QBuffer>
#include <QImageReader>
#include <QPainter>

namespace PhotoCrap::Ai
{
bool validSize(const QSize &_size)
{
    return _size.width() > 0 && _size.height() > 0 && _size.width() <= 16384 &&
           _size.height() <= 16384 && qint64(_size.width()) * _size.height() <= 32LL * 1024 * 1024;
}
Result prepareResult(const Input &_input, const QByteArray &_bytes, int _bits,
                     const Cancel &_cancel)
{
    Result result;
    result.bits = _bits;
    if (_cancel->load())
        return result;
    if (!validSize(_input.canvas) || _input.bounds.isEmpty() ||
        !QRect(QPoint{}, _input.canvas).contains(_input.bounds) ||
        (_input.selected && (_input.coverage.size() != _input.canvas ||
                             _input.coverage.format() != QImage::Format_Grayscale8))) {
        result.error = QObject::tr("Impossible de préparer le masque du calque IA.");
        return result;
    }
    QBuffer buffer;
    buffer.setData(_bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    if (_bytes.size() > 100LL * 1024 * 1024 ||
        !QList<QByteArray>{"png", "jpeg", "jpg", "webp"}.contains(reader.format()) ||
        !validSize(reader.size())) {
        result.error = QObject::tr("L’IA a renvoyé une image trop grande ou illisible.");
        return result;
    }
    auto image = reader.read();
    if (image.isNull()) {
        result.error = QObject::tr("Impossible de lire l’image renvoyée par l’IA.");
        return result;
    }
    image.setDevicePixelRatio(1.);
    if (image.colorSpace().isValid())
        image.convertToColorSpace(QColorSpace::SRgb);
    if (_cancel->load())
        return result;
    const auto format =
        _bits == 16 ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied;
    result.image = PixelPlane::uniform(_input.canvas, ImageUtils::blank({1, 1}, format));
    // Keep upscaling regional: an AI thumbnail must not require another full
    // native-resolution document buffer before becoming editable tiles.
    for (int y = _input.bounds.top(); y <= _input.bounds.bottom(); y += TiledImage::TileSize) {
        if (_cancel->load())
            return {};
        const QRect part(_input.bounds.x(), y, _input.bounds.width(),
                         qMin(TiledImage::TileSize, _input.bounds.bottom() - y + 1));
        auto        band = ImageUtils::blank(part.size(), format);
        if (band.isNull()) {
            result.error = QObject::tr("Mémoire insuffisante pour préparer le calque IA.");
            return result;
        }
        QPainter painter(&band);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.translate(-part.topLeft());
        painter.drawImage(_input.bounds, image);
        painter.end();
        if (!result.image.writeRegion(part.topLeft(), band, _input.canvas)) {
            result.error = QObject::tr("Mémoire insuffisante pour préparer le calque IA.");
            return result;
        }
    }
    if (!result.image.freeze()) {
        result.error = QObject::tr("Mémoire insuffisante pour préparer le calque IA.");
        return result;
    }
    if (_input.selected) {
        result.mask = _input.coverage;
        if (!result.mask.freeze())
            result.error = QObject::tr("Impossible de préparer le masque du calque IA.");
    }
    if (result.error.isEmpty()) {
        auto original      = std::make_shared<ImageSource>();
        original->encoded  = _bytes;
        original->canvas   = _input.canvas;
        original->bounds   = _input.bounds;
        original->bits     = _bits;
        original->pixelKey = result.image.cacheKey();
        result.original    = std::move(original);
    }
    return result;
}
} // namespace PhotoCrap::Ai
