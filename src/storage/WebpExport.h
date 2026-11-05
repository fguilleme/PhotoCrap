#pragma once

#include "StreamingImageWriter.h"

namespace PhotoCrap::Storage
{
bool writeWebpBands(QIODevice &_output, const QSize &_size, const ImageBandReader &_read,
                    const ImageWriteProgress &_progress, QString &_error);
}
