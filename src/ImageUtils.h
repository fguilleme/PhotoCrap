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
inline QImage blank(const QSize &_size)
{
    QImage image(_size, QImage::Format_ARGB32_Premultiplied);
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
