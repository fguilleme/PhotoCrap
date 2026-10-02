#pragma once

#include <QColorSpace>
#include <QImage>
#include <QSize>

namespace ImageUtils
{
inline QImage blank(const QSize &_size)
{
    QImage image(_size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}
} // namespace ImageUtils
