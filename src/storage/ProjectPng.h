#pragma once

#include <functional>

class PixelPlane;
class QIODevice;

namespace PhotoCrap::Storage
{
bool writeProjectPng(QIODevice &_output, const PixelPlane &_plane,
                     const std::function<bool()> &_keepGoing);
}
