#pragma once
#include <QImage>

namespace PhotoCrap
{
// Match Qt's 8-bit alpha quantization without converting native color planes.
QImage alphaCoverage(const QImage &_source, qreal _opacity);
} // namespace PhotoCrap
