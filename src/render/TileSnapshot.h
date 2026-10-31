#pragma once
#include <QImage>
#include <QRect>

namespace PhotoCrap::Render
{
// Reuse an independent snapshot (and its cache key) when the source rectangle
// still has identical pixels. Never retain a reference to the full source.
QImage tileSnapshot(const QImage &_source, const QRect &_region, const QImage &_previous = {});
} // namespace PhotoCrap::Render
