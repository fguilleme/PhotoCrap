#pragma once

#include <QImage>
#include <functional>

class PixelPlane;
class QIODevice;

namespace PhotoCrap::Storage
{
bool writeProjectPng(QIODevice &_output, const PixelPlane &_plane,
                     const std::function<bool()> &_keepGoing);

enum class ProjectPngRead { Failed, Unsupported, Loaded };
ProjectPngRead readProjectPng(QIODevice &_input, const QSize &_size, QImage::Format _format,
                              PixelPlane &_plane, const std::function<bool()> &_keepGoing);
}
