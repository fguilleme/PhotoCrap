#include "SurfaceGeometry.h"
#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QVector2D>
#include <QVector3D>
#include <cmath>
#include <new>
#include <vector>

namespace PhotoCrap::Ai
{
namespace
{
QVector3D normalAt(const QImage &normals, int x, int y)
{
    const auto *rgb = normals.constScanLine(y) + 3 * x;
    const QVector3D normal(rgb[0] - 127.5f, rgb[1] - 127.5f, rgb[2] - 127.5f);
    return normal.lengthSquared() <= 3.f ? QVector3D{} : normal.normalized();
}

bool continuousDepth(const QImage &disparity, const QImage &validity, const QPoint &point,
                     const QPoint &next, const QPoint &offset)
{
    const auto depthAt = [&](const QPoint &p) { return disparity.constScanLine(p.y())[p.x()]; };
    const int  step    = depthAt(next) - depthAt(point);
    // Ignore small variations in the quantized map. Larger gradients must
    // continue on one side of the edge, as on a sloping surface or at a fold.
    if (std::abs(step) <= 4)
        return true;
    if (std::abs(step) > 24)
        return false;
    const auto similarSlope = [&](const QPoint &p, int depth, int sign) {
        return disparity.rect().contains(p) && validity.constScanLine(p.y())[p.x()] &&
               std::abs((depthAt(p) - depth) * sign - step) <= qMax(2, std::abs(step) / 3);
    };
    return similarSlope(point - offset, depthAt(point), -1) ||
           similarSlope(next + offset, depthAt(next), 1);
}
} // namespace

QTransform surfaceBrushProjection(const QImage &normals, const QSize &canvas, const QPointF &center,
                                  const QTransform &mapping)
{
    bool       invertible = false;
    const auto point      = mapping.inverted(&invertible).map(center);
    if (normals.format() != QImage::Format_RGB888 || canvas.isEmpty() || !invertible ||
        !QRectF(QPointF{}, QSizeF(canvas)).contains(point))
        return {};
    // Interpolate the encoded normals in document coordinates so the dab's
    // axis follows the fabric smoothly even when the map is much smaller.
    const qreal x =
        qBound(0., point.x() * normals.width() / canvas.width() - .5, qreal(normals.width() - 1));
    const qreal y  = qBound(0., point.y() * normals.height() / canvas.height() - .5,
                            qreal(normals.height() - 1));
    const int   x0 = int(x), y0 = int(y);
    const int   x1 = qMin(x0 + 1, normals.width() - 1), y1 = qMin(y0 + 1, normals.height() - 1);
    const float u = float(x - x0), v = float(y - y0);
    const auto  normal =
        ((1.f - v) * ((1.f - u) * normalAt(normals, x0, y0) + u * normalAt(normals, x1, y0)) +
         v * ((1.f - u) * normalAt(normals, x0, y1) + u * normalAt(normals, x1, y1)))
            .normalized();
    if (normal.x() * normal.x() + normal.y() * normal.y() < 0.0004f)
        return {};
    const QPointF   offset(-normal.y(), -normal.x());
    const QVector2D tangent(mapping.map(point + offset) - center);
    if (tangent.isNull())
        return {};
    // The projected tangent disk contracts by |N.z| along the projected normal.
    // Keep a narrow footprint at grazing angles to avoid gaps between dabs.
    const auto direction = tangent.normalized();
    const auto squeeze   = 1. - qBound(.25, qreal(std::abs(normal.z())), 1.);
    const auto xx        = squeeze * direction.x() * direction.x();
    const auto yy        = squeeze * direction.y() * direction.y();
    const auto xy        = squeeze * direction.x() * direction.y();
    return {1. - yy, xy, xy, 1. - xx, 0., 0.};
}

namespace
{
template <class Pixel, class Blend>
void sampleSurfaceText(QImage &projected, const QImage &text, const std::vector<QPointF> &grid,
                       const QSize &steps, Blend blend)
{
    const int   stride = steps.width() + 1;
    const qreal stepX  = qreal(text.width() - 1) / steps.width();
    const qreal stepY  = qreal(text.height() - 1) / steps.height();
    for (int y = 0; y < text.height(); ++y) {
        auto       *out = reinterpret_cast<Pixel *>(projected.scanLine(y));
        const qreal row = stepY > 0. ? y / stepY : 0.;
        const int   gy  = qMin(int(row), steps.height() - 1);
        const qreal v   = row - gy;
        for (int x = 0; x < text.width(); ++x) {
            const qreal col    = stepX > 0. ? x / stepX : 0.;
            const int   gx     = qMin(int(col), steps.width() - 1);
            const qreal u      = col - gx;
            const auto  i      = gy * stride + gx;
            const auto  top    = grid[i] * (1. - u) + grid[i + 1] * u;
            const auto  bottom = grid[i + stride] * (1. - u) + grid[i + stride + 1] * u;
            const auto  p      = top * (1. - v) + bottom * v;
            if (p.x() <= -1. || p.y() <= -1. || p.x() >= text.width() || p.y() >= text.height())
                continue;
            const int  sx = int(std::floor(p.x())), sy = int(std::floor(p.y()));
            const auto sample = [&](int px, int py) -> Pixel {
                if (px < 0 || py < 0 || px >= text.width() || py >= text.height())
                    return {};
                return reinterpret_cast<const Pixel *>(text.constScanLine(py))[px];
            };
            out[x] = blend(sample(sx, sy), sample(sx + 1, sy), sample(sx, sy + 1),
                           sample(sx + 1, sy + 1), p.x() - sx, p.y() - sy);
        }
    }
}
} // namespace

QImage projectSurfaceText(const QImage &text, const QPoint &origin, const QPointF &anchor,
                          const QImage &normals, const QSize &canvas, const QTransform &mapping)
{
    if (text.isNull() || normals.isNull())
        return text;
    const QSize          steps(qBound(1, (text.width() + 5) / 6, 192),
                               qBound(1, (text.height() + 5) / 6, 192));
    const int            stride = steps.width() + 1;
    const qreal          stepX  = qreal(text.width() - 1) / steps.width();
    const qreal          stepY  = qreal(text.height() - 1) / steps.height();
    std::vector<QPointF> stretch(stride * (steps.height() + 1));
    std::vector<QPointF> grid(stretch.size());
    bool                 flat = true;
    for (int y = 0; y <= steps.height(); ++y)
        for (int x = 0; x <= steps.width(); ++x) {
            const auto projection = surfaceBrushProjection(
                normals, canvas, QPointF(origin) + QPointF(x * stepX + .5, y * stepY + .5),
                mapping);
            // Convert image distances to approximate distances along the cloth.
            // Cap the stretch to keep lettering legible on grazing folds.
            constexpr qreal minimumScale = .45;
            const qreal     sx = qMax(minimumScale, std::hypot(projection.m11(), projection.m12()));
            const qreal     sy = qMax(minimumScale, std::hypot(projection.m21(), projection.m22()));
            flat &= 1. - sx < .001 && 1. - sy < .001;
            stretch[y * stride + x] = {1. / sx, 1. / sy};
        }
    if (flat)
        return text;
    // Integrate each row and column through the folds. Different distances on
    // adjacent rows and columns deform the entire word, including its baseline.
    const auto integrate = [&](int start, int increment, int count, int axis, qreal step,
                               qreal at) {
        const auto value = [axis](const QPointF &p) { return axis == 0 ? p.x() : p.y(); };
        const auto coord = [axis](QPointF &p) -> qreal & { return axis == 0 ? p.rx() : p.ry(); };
        for (int i = 1; i <= count; ++i) {
            const int next = start + i * increment;
            const int prev = next - increment;
            coord(grid[next]) =
                value(grid[prev]) + (value(stretch[prev]) + value(stretch[next])) * step * .5;
        }
        at                   = qBound(0., at, step * count);
        const qreal t        = step > 0. ? at / step : 0.;
        const int   i        = qMin(count - 1, int(t));
        const qreal fraction = t - i;
        const qreal offset   = at - value(grid[start + i * increment]) * (1. - fraction) -
                               value(grid[start + (i + 1) * increment]) * fraction;
        for (int k = 0; k <= count; ++k)
            coord(grid[start + k * increment]) += offset;
    };
    const QPointF localAnchor = anchor - QPointF(origin) - QPointF(.5, .5);
    for (int y = 0; y <= steps.height(); ++y)
        integrate(y * stride, 1, steps.width(), 0, stepX, localAnchor.x());
    for (int x = 0; x <= steps.width(); ++x)
        integrate(x, stride, steps.height(), 1, stepY, localAnchor.y());

    QImage projected(text.size(), text.format());
    if (projected.isNull())
        return {};
    projected.fill(Qt::transparent);
    const auto channel = [](int a, int b, int c, int d, qreal u, qreal v) {
        return qRound((a * (1. - u) + b * u) * (1. - v) + (c * (1. - u) + d * u) * v);
    };
    if (text.format() == QImage::Format_RGBA64_Premultiplied) {
        sampleSurfaceText<QRgba64>(
            projected, text, grid, steps,
            [&](QRgba64 a, QRgba64 b, QRgba64 c, QRgba64 d, qreal u, qreal v) {
                return QRgba64::fromRgba64(
                    channel(a.red(), b.red(), c.red(), d.red(), u, v),
                    channel(a.green(), b.green(), c.green(), d.green(), u, v),
                    channel(a.blue(), b.blue(), c.blue(), d.blue(), u, v),
                    channel(a.alpha(), b.alpha(), c.alpha(), d.alpha(), u, v));
            });
    } else {
        sampleSurfaceText<QRgb>(
            projected, text, grid, steps, [&](QRgb a, QRgb b, QRgb c, QRgb d, qreal u, qreal v) {
                return qRgba(channel(qRed(a), qRed(b), qRed(c), qRed(d), u, v),
                             channel(qGreen(a), qGreen(b), qGreen(c), qGreen(d), u, v),
                             channel(qBlue(a), qBlue(b), qBlue(c), qBlue(d), u, v),
                             channel(qAlpha(a), qAlpha(b), qAlpha(c), qAlpha(d), u, v));
            });
    }
    return projected;
}

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

QImage SurfaceGeometry::regionAt(const QPoint &seed, const QSize &canvas, bool followRelief,
                                 const Cancel &cancel) const
{
    if (!isValid() || canvas.isEmpty() || !QRect(QPoint{}, canvas).contains(seed))
        return {};
    const auto size = disparity.size();
    const QPoint start(int((2LL * seed.x() + 1) * size.width() / (2LL * canvas.width())),
                       int((2LL * seed.y() + 1) * size.height() / (2LL * canvas.height())));
    if (!validity.constScanLine(start.y())[start.x()])
        return {};
    QImage region(size, QImage::Format_Grayscale8);
    if (region.isNull())
        throw std::bad_alloc();
    region.fill(0);
    const auto       seedNormal   = normalAt(normals, start.x(), start.y());
    const float      minNormalDot = followRelief ? 0.5f : 0.78f;
    constexpr QPoint neighbors[]  = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    QList<QPoint>    pending{start};
    region.scanLine(start.y())[start.x()] = 255;
    while (!pending.isEmpty()) {
        const QPoint point = pending.takeLast();
        if (cancel->load())
            return {};
        const auto direction = followRelief ? normalAt(normals, point.x(), point.y()) : seedNormal;
        for (const auto &offset : neighbors) {
            const auto next = point + offset;
            const int  x = next.x(), y = next.y();
            if (!region.rect().contains(next) || region.constScanLine(y)[x] ||
                !validity.constScanLine(y)[x] ||
                !continuousDepth(disparity, validity, point, next, offset) ||
                QVector3D::dotProduct(direction, normalAt(normals, x, y)) < minNormalDot)
                continue;
            region.scanLine(y)[x] = 255;
            pending.append(next);
        }
    }

    return region;
}

QImage restrictSurfaceRegion(QImage region, const QImage &boundary, const Cancel &cancel)
{
    if (region.isNull() || boundary.format() != QImage::Format_Grayscale8 ||
        region.size() != boundary.size())
        return region;
    qsizetype pixels = 0, agreement = 0;
    for (int y = 0; y < region.height(); ++y) {
        if (cancel->load())
            return {};
        const auto *selected = region.constScanLine(y);
        const auto *inside   = boundary.constScanLine(y);
        for (int x = 0; x < region.width(); ++x) {
            pixels += selected[x] != 0;
            agreement += selected[x] && inside[x];
        }
    }
    // A prompt over a stain or a shadow can return just that detail. Only
    // use the image boundary if it describes most of the same surface.
    if (agreement * 4 < pixels * 3 || agreement == pixels)
        return region;
    for (int y = 0; y < region.height(); ++y) {
        if (cancel->load())
            return {};
        auto       *selected = region.scanLine(y);
        const auto *inside   = boundary.constScanLine(y);
        for (int x = 0; x < region.width(); ++x)
            selected[x] = inside[x] ? selected[x] : 0;
    }
    return region;
}

QImage combineSurfaceRegions(QImage region, const QImage &previous, bool subtract,
                             const Cancel &cancel)
{
    if (region.isNull() || (!previous.isNull() && (previous.size() != region.size() ||
                                                   previous.format() != QImage::Format_Grayscale8)))
        return {};
    if (previous.isNull())
        return subtract || cancel->load() ? QImage{} : region;
    bool any = false;
    for (int y = 0; y < region.height(); ++y) {
        if (cancel->load())
            return {};
        const auto *old = previous.constScanLine(y);
        auto       *row = region.scanLine(y);
        for (int x = 0; x < region.width(); ++x) {
            const bool wasSelected = old[x] != 0;
            const bool selected    = subtract ? wasSelected && !row[x] : wasSelected || row[x];
            row[x]                 = selected ? 255 : 0;
            any |= selected;
        }
    }
    return any ? region : QImage{};
}

SurfaceSelection surfaceSelection(const QImage &mask, const QImage &protection,
                                  const Cancel &cancel)
{
    SurfaceSelection selection;
    if (mask.isNull() && protection.isNull())
        return selection;
    selection.mask    = mask;
    selection.preview = QImage(mask.isNull() ? protection.size() : mask.size(),
                               QImage::Format_ARGB32_Premultiplied);
    if (selection.preview.isNull())
        throw std::bad_alloc();
    const auto green = qPremultiply(qRgba(123, 234, 169, 112));
    const auto red   = qPremultiply(qRgba(250, 115, 110, 125));
    for (int y = 0; y < selection.preview.height(); ++y) {
        if (cancel->load())
            return {};
        const auto *surface = mask.isNull() ? nullptr : mask.constScanLine(y);
        const auto *object  = protection.isNull() ? nullptr : protection.constScanLine(y);
        auto       *out     = reinterpret_cast<QRgb *>(selection.preview.scanLine(y));
        for (int x = 0; x < selection.preview.width(); ++x)
            out[x] = object && object[x] ? red : surface && surface[x] ? green : 0;
    }
    return selection;
}

PixelPlane SurfaceGeometry::paintMask(const QImage &selected, const QSize &canvas,
                                      const Cancel &cancel, const QImage &protection)
{
    if (selected.isNull() || canvas.isEmpty() ||
        (!protection.isNull() && protection.size() != selected.size()))
        return {};
    auto coverage = selected;
    if (!protection.isNull()) {
        for (int y = 0; y < coverage.height(); ++y) {
            if (cancel->load())
                return {};
            auto       *out     = coverage.scanLine(y);
            const auto *blocked = protection.constScanLine(y);
            for (int x = 0; x < coverage.width(); ++x)
                if (blocked[x])
                    out[x] = 0;
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
        painter.drawImage(QRect(QPoint(0, -y), canvas), coverage);
        painter.end();
        if (!mask.writeRegion(QPoint(0, y), band, canvas))
            return {};
    }
    return mask.freeze() ? mask : PixelPlane{};
}
} // namespace PhotoCrap::Ai
