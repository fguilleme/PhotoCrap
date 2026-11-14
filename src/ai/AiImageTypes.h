#pragma once
#include "storage/PixelPlane.h"
#include <QByteArray>
#include <QRect>
#include <QString>
#include <atomic>
#include <memory>

namespace PhotoCrap::Ai
{
struct Input {
    QByteArray image, mask;
    QSize      canvas, sentSize;
    QRect      bounds;
    QImage     coverage;
    QImage     localImage;
    QString    layerId, error;
    quint64    session  = 0;
    quint64    contentId = 0;
    bool       selected = false, originalSource = false;
    bool       maskEnabled = true;
    qreal      maskDensity = 100., maskFeather = 0.;
};
// Preserve the provider's original bytes instead of serializing their enlarged
// raster. Immutable snapshots share this source; a pixel edit invalidates it.
struct ImageSource {
    QByteArray encoded;
    QSize      canvas;
    QRect      bounds;
    int        bits      = 8;
    qint64     pixelKey  = 0;
    qint64     memoryKey = QImage(1, 1, QImage::Format_ARGB32).cacheKey();
    bool       matches(const PixelPlane &_plane, const QSize &_canvas, int _bits) const
    {
        return !encoded.isEmpty() && canvas == _canvas && bits == _bits &&
               _plane.size() == canvas && _plane.cacheKey() == pixelKey &&
               _plane.format() == (_bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                               : QImage::Format_ARGB32_Premultiplied);
    }
};
struct Result {
    PixelPlane                         image, mask;
    QString                            error;
    int                                bits = 8;
    std::shared_ptr<const ImageSource> original;
};
using Cancel = std::shared_ptr<std::atomic_bool>;
Result prepareResult(const Input &_input, const QByteArray &_bytes, int _bits,
                     const Cancel &_cancel, bool nativeSize = false);
bool   validSize(const QSize &_size);
} // namespace PhotoCrap::Ai
