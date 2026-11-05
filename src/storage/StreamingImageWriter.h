#pragma once
#include <QIODevice>
#include <QImage>
#include <functional>

namespace PhotoCrap::Storage
{
using ImageBandReader    = std::function<QImage(const QRect &)>;
using ImageWriteProgress = std::function<bool(int, int)>;
// Native, lossless PNG and white-composited BMP. The reader is called for at
// most 4 MiB of native pixels at a time; no complete output image is retained.
bool writeImageBands(QIODevice &_output, const QSize &_size, int _bits, bool _png,
                     const ImageBandReader &_read, const ImageWriteProgress &_progress,
                     QString &_error);
bool writeJpegBands(QIODevice &_output, const QSize &_size, const ImageBandReader &_read,
                    const ImageWriteProgress &_progress, QString &_error);
} // namespace PhotoCrap::Storage
