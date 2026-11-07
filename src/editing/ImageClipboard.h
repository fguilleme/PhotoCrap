#pragma once
#include <QImage>
#include <QPoint>

namespace PhotoCrap::Clipboard
{
struct Pixels {
    QImage image;
    QPoint origin;
    QSize  canvas;
    bool   positioned = false;
};
bool    hasImage();
Pixels  read();
void    write(const Pixels &_pixels);
quint64 revision();
} // namespace PhotoCrap::Clipboard
