#include "SurfaceGeometry.h"
#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QVector3D>

namespace PhotoCrap::Ai
{
namespace
{
QVector3D normalAt(const QImage &normals, int x, int y)
{
    const auto *rgb = normals.constScanLine(y) + 3 * x;
    return QVector3D(rgb[0] - 127.5f, rgb[1] - 127.5f, rgb[2] - 127.5f).normalized();
}
} // namespace

bool SurfaceGeometry::isValid() const
{
    return !disparity.isNull() && disparity.size() == normals.size() &&
           disparity.size() == validity.size() && disparity.format() == QImage::Format_Grayscale8 &&
           validity.format() == QImage::Format_Grayscale8 &&
           normals.format() == QImage::Format_RGB888;
}

SurfaceGeometry SurfaceGeometry::fromComfyPreview(const QByteArray &preview, const Input &source)
{
    const auto size = source.sentSize;
    if (size.isEmpty() || qMax(size.width(), size.height()) > 2048)
        return {};
    QBuffer buffer;
    buffer.setData(preview);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "png");
    if (reader.size() != QSize(size.width() * 3, size.height()))
        return {};
    const auto maps = reader.read();
    const auto image =
        QImage::fromData(source.image, "PNG").convertToFormat(QImage::Format_RGBA8888);
    if (maps.isNull() || image.size() != size)
        return {};

    SurfaceGeometry geometry{
        maps.copy(0, 0, size.width(), size.height()).convertToFormat(QImage::Format_Grayscale8),
        maps.copy(size.width(), 0, size.width(), size.height())
            .convertToFormat(QImage::Format_RGB888),
        maps.copy(size.width() * 2, 0, size.width(), size.height())
            .convertToFormat(QImage::Format_Grayscale8)};
    if (!geometry.isValid())
        return {};
    for (int y = 0; y < size.height(); ++y) {
        auto       *valid = geometry.validity.scanLine(y);
        const auto *rgba  = image.constScanLine(y);
        for (int x = 0; x < size.width(); ++x)
            valid[x] = valid[x] >= 128 && rgba[4 * x + 3] > 0 ? 255 : 0;
    }
    return geometry;
}

PixelPlane SurfaceGeometry::connectedMask(const QPoint &seed, const QSize &canvas,
                                          const Cancel &cancel) const
{
    if (!isValid() || canvas.isEmpty() || !QRect(QPoint{}, canvas).contains(seed))
        return {};
    const auto   size = disparity.size();
    const QPoint start(int(qint64(seed.x()) * size.width() / canvas.width()),
                       int(qint64(seed.y()) * size.height() / canvas.height()));
    if (!validity.constScanLine(start.y())[start.x()])
        return {};
    QImage selected(size, QImage::Format_Grayscale8);
    if (selected.isNull())
        return {};
    selected.fill(0);
    const auto direction = normalAt(normals, start.x(), start.y());
    // Limit adjacent disparity jumps and compare the orientation to the seed
    // so the flood does not progressively turn a corner onto another surface.
    constexpr int   maxDepthStep = 12;
    constexpr float minNormalDot = 0.78f;
    const QPoint    neighbors[]  = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    QList<QPoint>   pending{start};
    selected.scanLine(start.y())[start.x()] = 255;
    while (!pending.isEmpty()) {
        const QPoint point = pending.takeLast();
        if (cancel->load())
            return {};
        const int depth = disparity.constScanLine(point.y())[point.x()];
        for (const auto &offset : neighbors) {
            const auto next = point + offset;
            const int  x = next.x(), y = next.y();
            if (!selected.rect().contains(next) || selected.constScanLine(y)[x] ||
                !validity.constScanLine(y)[x] ||
                std::abs(depth - disparity.constScanLine(y)[x]) > maxDepthStep ||
                QVector3D::dotProduct(direction, normalAt(normals, x, y)) < minNormalDot)
                continue;
            selected.scanLine(y)[x] = 255;
            pending.append(next);
        }
    }

    QImage clear(1, 1, QImage::Format_Grayscale8);
    if (clear.isNull())
        return {};
    clear.fill(0);
    auto mask = PixelPlane::uniform(canvas, clear);
    for (int y = 0; y < canvas.height(); y += TiledImage::TileSize) {
        if (cancel->load())
            return {};
        QImage band(canvas.width(), qMin(TiledImage::TileSize, canvas.height() - y),
                    QImage::Format_Grayscale8);
        if (band.isNull())
            return {};
        QPainter painter(&band);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(QRect(QPoint(0, -y), canvas), selected);
        painter.end();
        if (!mask.writeRegion(QPoint(0, y), band, canvas))
            return {};
    }
    return mask.freeze() ? mask : PixelPlane{};
}
} // namespace PhotoCrap::Ai
