#pragma once

#include <QColorSpace>
#include <QImage>
#include <QSize>
#include <QThread>
#include <QThreadPool>
#include <QtConcurrent>
#include <utility>

namespace ImageUtils
{
// A single stored pixel represents a uniform plane at document size. Expand
// it only when a tool needs writable pixels; snapshots keep the compact value.
inline bool compact(const QImage &_image)
{
    return !_image.isNull() && _image.size() == QSize(1, 1);
}

inline QImage uniformMask(int _value)
{
    QImage mask(1, 1, QImage::Format_Grayscale8);
    mask.fill(qBound(0, _value, 255));
    return mask;
}

inline QImage expandedPlane(const QImage &_image, const QSize &_size)
{
    if (!compact(_image) || _image.size() == _size)
        return _image;
    QImage image(_size, _image.format());
    if (_image.format() == QImage::Format_Grayscale8)
        image.fill(_image.constScanLine(0)[0]);
    else
        image.fill(_image.pixelColor(0, 0));
    image.setColorSpace(_image.colorSpace());
    return image;
}

inline bool is16Bit(const QImage &_image)
{
    return _image.depth() > 32 || _image.format() == QImage::Format_Grayscale16;
}

inline QImage::Format workingFormat(const QImage &_image)
{
    return is16Bit(_image) ? QImage::Format_RGBA64_Premultiplied
                           : QImage::Format_ARGB32_Premultiplied;
}

inline QImage blank(const QSize   &_size,
                    QImage::Format _format = QImage::Format_ARGB32_Premultiplied)
{
    QImage image(_size, _format);
    image.fill(Qt::transparent);
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}

template <typename Range, typename Function>
void blockingMap(Range &_range, Function _function)
{
    QThreadPool *pool = QThreadPool::globalInstance();
    if (pool->contains(QThread::currentThread())) {
        for (auto &value : _range)
            _function(value);
        return;
    }
    QtConcurrent::blockingMap(_range, std::move(_function));
}
} // namespace ImageUtils
