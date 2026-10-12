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
