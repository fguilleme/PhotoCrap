#include "SurfaceGeometry.h"
#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QVector2D>
#include <QVector3D>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>
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

namespace
{
QVector3D sampleNormal(const QImage &normals, const QSize &canvas, const QPointF &point)
{
    if (normals.format() != QImage::Format_RGB888 || canvas.isEmpty() ||
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
    return ((1.f - v) * ((1.f - u) * normalAt(normals, x0, y0) + u * normalAt(normals, x1, y0)) +
            v * ((1.f - u) * normalAt(normals, x0, y1) + u * normalAt(normals, x1, y1)))
        .normalized();
}
} // namespace

QTransform surfaceBrushProjection(const QImage &normals, const QSize &canvas, const QPointF &center,
                                  const QTransform &mapping)
{
    bool       invertible = false;
    const auto point      = mapping.inverted(&invertible).map(center);
    if (!invertible)
        return {};
    const auto normal = sampleNormal(normals, canvas, point);
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

QTransform surfaceTextProjection(const QImage &normals, const QSize &canvas, const QPointF &anchor,
                                 const QTransform &mapping, qreal angle, qreal focalRatio)
{
    bool       invertible = false;
    const auto inverse    = mapping.inverted(&invertible);
    if (!invertible || canvas.isEmpty() || !std::isfinite(angle) || !std::isfinite(focalRatio))
        return {};
    const auto point = inverse.map(anchor);
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
        return {};
    const auto normal  = sampleNormal(normals, canvas, point);
    QVector3D  average = normal * 4.f;
    // Smooth small variations while avoiding a second face at a corner.
    const qreal radius = qMax(canvas.width(), canvas.height()) / 240.;
    for (int dy : {-1, 0, 1})
        for (int dx : {-1, 0, 1}) {
            const auto near =
                sampleNormal(normals, canvas, point + QPointF(dx * radius, dy * radius));
            if (QVector3D::dotProduct(normal, near) > .90f)
                average += near;
        }
    average.normalize();
    if (average.z() < 0.f)
        average = -average;
    QVector3D n(-average.x(), average.y(), qMax(.25f, average.z()));
    if (n.x() * n.x() + n.y() * n.y() < .0004f)
        n = {0.f, 0.f, 1.f};
    return inverse * planeTextProjection(canvas, point, n.normalized(), angle, focalRatio) *
           mapping;
}

QTransform planeTextProjection(const QSize &canvas, const QPointF &point, const QVector3D &normal,
                               qreal angle, qreal focalRatio)
{
    const QVector3D right   = QVector3D(normal.z(), 0.f, -normal.x()).normalized();
    const QVector3D down    = QVector3D::crossProduct(normal, right);
    const auto      radians = qDegreesToRadians(angle);
    const auto      c = float(std::cos(radians)), s = float(std::sin(radians));
    const auto      u = c * right + s * down;
    const auto      v = c * down - s * right;
    const QPointF   center(canvas.width() * .5, canvas.height() * .5);
    const auto      offset = point - center;
    const qreal focal = std::hypot(canvas.width(), canvas.height()) * qBound(.35, focalRatio, 3.);
    // Project a plane through the anchor. Its two depth slopes produce
    // converging lines and allow the lettering to rotate within that plane.
    const QTransform perspective(u.x(), u.y(), u.z() / focal, v.x(), v.y(), v.z() / focal,
                                 offset.x(), offset.y(), 1.);
    return QTransform::fromTranslate(-point.x(), -point.y()) * perspective *
           QTransform::fromTranslate(center.x(), center.y());
}

QVariantList SurfaceCrease::parameters() const
{
    return {edge.x1(),          edge.y1(),          edge.x2(),          edge.y2(),
            negativeNormal.x(), negativeNormal.y(), negativeNormal.z(), positiveNormal.x(),
            positiveNormal.y(), positiveNormal.z()};
}

std::optional<SurfaceCrease> SurfaceCrease::fromParameters(const QVariant &value)
{
    const auto values = value.toList();
    if (values.size() != 10)
        return {};
    qreal p[10];
    for (int i = 0; i < 10; ++i) {
        bool ok = false;
        p[i]    = values[i].toDouble(&ok);
        if (!ok || !std::isfinite(p[i]) || qAbs(p[i]) > (i < 4 ? 32768. : 1.))
            return {};
    }
    const SurfaceCrease crease{QLineF(p[0], p[1], p[2], p[3]), QVector3D(p[4], p[5], p[6]),
                               QVector3D(p[7], p[8], p[9])};
    if (crease.edge.length() < 1. || crease.negativeNormal.lengthSquared() < .5f ||
        crease.positiveNormal.lengthSquared() < .5f)
        return {};
    return crease;
}

std::optional<SurfaceHinge> surfaceTextHinge(const SurfaceCrease &crease, const QSize &canvas,
                                             qreal focalRatio)
{
    const auto cameraNormal = [](const QVector3D &normal) {
        return QVector3D(-normal.x(), normal.y(), normal.z()).normalized();
    };
    auto            first  = cameraNormal(crease.negativeNormal);
    auto            second = cameraNormal(crease.positiveNormal);
    const QPointF   center(canvas.width() * .5, canvas.height() * .5);
    const QPointF   origin = crease.edge.center();
    const QPointF   axis   = crease.edge.unitVector().p2() - crease.edge.p1();
    const QPointF   perpendicular(-axis.y(), axis.x());
    const qreal     diagonal = std::hypot(canvas.width(), canvas.height());
    const qreal     focal    = diagonal * qBound(.35, focalRatio, 3.);
    const QVector3D pivot(origin.x() - center.x(), origin.y() - center.y(), focal);
    const auto viewNormal = QVector3D(perpendicular.x(), perpendicular.y(),
                                      -QPointF::dotProduct(origin - center, perpendicular) / focal)
                                .normalized();
    // The 3D hinge lies in the viewing plane of the fitted image edge.
    const auto intersection = QVector3D::crossProduct(first, second);
    const auto hinge =
        (intersection - viewNormal * QVector3D::dotProduct(intersection, viewNormal)).normalized();
    if (hinge.isNull())
        return {};
    first                   = (first - hinge * QVector3D::dotProduct(first, hinge)).normalized();
    second                  = (second - hinge * QVector3D::dotProduct(second, hinge)).normalized();
    const auto acrossFirst  = QVector3D::crossProduct(first, hinge);
    const auto acrossSecond = QVector3D::crossProduct(second, hinge);
    const auto rotate       = [&](const QVector3D &v) {
        return hinge * QVector3D::dotProduct(v, hinge) +
               acrossSecond * QVector3D::dotProduct(v, acrossFirst) +
               second * QVector3D::dotProduct(v, first);
    };
    const qreal depth = QVector3D::dotProduct(first, pivot);
    if (depth < focal * .06 || QVector3D::dotProduct(second, pivot) < focal * .06)
        return {};
    const auto      offset = pivot - rotate(pivot);
    const QVector3D step(perpendicular.x(), perpendicular.y(), 0.);
    const auto  delta = rotate(step) - step + offset * (QVector3D::dotProduct(first, step) / depth);
    const qreal slope = delta.z() / focal;
    const auto  b     = delta.toPointF() + center * slope;
    const auto  tangent     = b - origin * slope;
    const qreal normalScale = 1. + QPointF::dotProduct(tangent, perpendicular);
    if (!std::isfinite(normalScale) || normalScale <= .06)
        return {};

    // H(p) = (p + b*s)/(1 + slope*s), where s is signed distance to the edge.
    // The full projective term preserves the vanishing points of the second face.
    const qreal d = -QPointF::dotProduct(origin, perpendicular);
    return SurfaceHinge{QTransform(1. + b.x() * perpendicular.x(), b.y() * perpendicular.x(),
                                   slope * perpendicular.x(), b.x() * perpendicular.y(),
                                   1. + b.y() * perpendicular.y(), slope * perpendicular.y(),
                                   b.x() * d, b.y() * d, 1. + slope * d),
                        first};
}

namespace
{
QPointF sampleTextGrid(const std::vector<QPointF> &grid, const QSize &steps, qreal col, qreal row)
{
    const int   gx = qMin(int(col), steps.width() - 1);
    const int   gy = qMin(int(row), steps.height() - 1);
    const qreal u = col - gx, v = row - gy;
    const int   stride = steps.width() + 1;
    const int   i      = gy * stride + gx;
    const auto  top    = grid[i] * (1. - u) + grid[i + 1] * u;
    const auto  bottom = grid[i + stride] * (1. - u) + grid[i + stride + 1] * u;
    return top * (1. - v) + bottom * v;
}

template <class Pixel, class Blend>
void sampleSurfaceText(QImage &projected, const QImage &text, const std::vector<QPointF> &grid,
                       const QSize &steps, Blend blend)
{
    const qreal stepX  = qreal(text.width() - 1) / steps.width();
    const qreal stepY  = qreal(text.height() - 1) / steps.height();
    for (int y = 0; y < text.height(); ++y) {
        auto       *out = reinterpret_cast<Pixel *>(projected.scanLine(y));
        const qreal row = stepY > 0. ? y / stepY : 0.;
        for (int x = 0; x < text.width(); ++x) {
            const qreal col    = stepX > 0. ? x / stepX : 0.;
            const auto  p      = sampleTextGrid(grid, steps, col, row);
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

QPointF SurfaceTextWarp::sourcePoint(const QPointF &point) const
{
    const qreal x   = qBound(0., point.x(), qreal(region.width() - 1));
    const qreal y   = qBound(0., point.y(), qreal(region.height() - 1));
    const qreal col = region.width() > 1 ? x * steps.width() / (region.width() - 1) : 0.;
    const qreal row =
        region.height() > 1 ? y * steps.height() / (region.height() - 1) : 0.;
    return sampleTextGrid(grid, steps, col, row) + point - QPointF(x, y);
}

QPointF SurfaceTextWarp::outputPoint(const QPointF &target, const QPointF &guess) const
{
    // Invert the bilinear sampling map used for the glyphs. Start from the
    // previous boundary sample to stay on the same side of a steep fold.
    QPointF point     = guess;
    QPointF best      = point;
    qreal   bestError = std::numeric_limits<qreal>::infinity();
    for (int iteration = 0; iteration < 16; ++iteration) {
        const auto  source   = sourcePoint(point);
        const auto  residual = source - target;
        const qreal error    = std::hypot(residual.x(), residual.y());
        if (error < bestError) {
            best      = point;
            bestError = error;
        }
        if (error < .05)
            break;
        const auto  dx   = sourcePoint(point + QPointF(.5, 0.)) - source;
        const auto  dy   = sourcePoint(point + QPointF(0., .5)) - source;
        const qreal det  = dx.x() * dy.y() - dy.x() * dx.y();
        QPointF     step = residual;
        if (qAbs(det) > .025)
            step = QPointF(dy.y() * residual.x() - dy.x() * residual.y(),
                           dx.x() * residual.y() - dx.y() * residual.x()) *
                   (.5 / det);
        const qreal length = std::hypot(step.x(), step.y());
        point -= step * qMin(1., 64. / qMax(1., length));
    }
    return best;
}

SurfaceTextWarp surfaceTextWarp(const QRect &region, const QPointF &anchor, const QImage &normals,
                                const QSize &canvas, const QTransform &mapping, qreal focalRatio,
                                qreal strength)
{
    if (region.isEmpty() || normals.isNull() || strength <= 0.)
        return {};
    const auto           origin = region.topLeft();
    const QSize          steps(qBound(1, (region.width() + 5) / 6, 192),
                               qBound(1, (region.height() + 5) / 6, 192));
    const int            stride = steps.width() + 1;
    const qreal          stepX  = qreal(region.width() - 1) / steps.width();
    const qreal          stepY  = qreal(region.height() - 1) / steps.height();
    const auto           unmap  = mapping.inverted();
    struct Differential {
        QTransform metric;
        QVector2D  depthSlope;
    };
    const auto differentialAt = [&](const QPointF &position, const Differential &fallback) {
        const auto point  = unmap.map(position);
        auto       normal = sampleNormal(normals, canvas, point);
        if (normal.isNull())
            return fallback;
        if (normal.z() < 0)
            normal = -normal;
        const auto across    = mapping.map(point + QPointF(normal.x(), -normal.y())) - position;
        const auto direction = QVector2D(across).normalized();
        // The square root of the surface metric gives the local increase in
        // distance along the slope, including the shear of a diagonal fold.
        const qreal stretch = 1. / qMax(.48, qreal(qAbs(normal.z()))) - 1.;
        const qreal xy      = stretch * direction.x() * direction.y();
        const auto slope = -direction * std::hypot(normal.x(), normal.y()) / qMax(.48f, normal.z());
        return Differential{QTransform(1. + stretch * direction.x() * direction.x(), xy, xy,
                                       1. + stretch * direction.y() * direction.y(), 0., 0.),
                            slope};
    };
    const auto             reference        = differentialAt(anchor, {});
    const auto             inverseReference = reference.metric.inverted();
    const auto             points           = stride * (steps.height() + 1);
    std::vector<QVector3D> horizontal(points), vertical(points);
    std::vector<QPointF>   grid(points);
    bool                   warped = false;
    for (int y = 0; y <= steps.height(); ++y)
        for (int x = 0; x <= steps.width(); ++x) {
            const auto position   = QPointF(origin) + QPointF(x * stepX + .5, y * stepY + .5);
            const auto local      = differentialAt(position, reference);
            const auto relative   = local.metric * inverseReference;
            const auto depthSlope = (local.depthSlope - reference.depthSlope) * float(strength);
            const int  i          = y * stride + x;
            horizontal[i]         = {float(qBound(-.30, (relative.m11() - 1.) * strength, .60)),
                                     float(qBound(-.26, relative.m12() * strength, .26)),
                                     qBound(-1.f, depthSlope.x(), 1.f)};
            vertical[i]           = {float(qBound(-.26, relative.m21() * strength, .26)),
                                     float(qBound(-.30, (relative.m22() - 1.) * strength, .60)),
                                     qBound(-1.f, depthSlope.y(), 1.f)};
            warped |= horizontal[i].lengthSquared() > 1e-5f || vertical[i].lengthSquared() > 1e-5f;
        }
    if (!warped)
        return {};

    const auto anchorInGrid = anchor - QPointF(origin) - QPointF(.5, .5);
    const auto gridPosition = [](qreal position, qreal step, int count) {
        return step > 0. ? qBound(0., position / step, qreal(count)) : 0.;
    };
    const qreal ax          = gridPosition(anchorInGrid.x(), stepX, steps.width());
    const qreal ay          = gridPosition(anchorInGrid.y(), stepY, steps.height());
    const auto  interpolate = [](const std::vector<QVector3D> &line, int start, int increment,
                                 int count, qreal at) {
        const int   i        = qMin(count - 1, int(at));
        const float fraction = float(at - i);
        return line[start + i * increment] * (1.f - fraction) +
               line[start + (i + 1) * increment] * fraction;
    };
    const auto integrate = [&](std::vector<QVector3D> &line, int start, int increment, int count,
                               qreal step, qreal at) {
        auto previous = line[start];
        line[start]   = {};
        for (int i = 1; i <= count; ++i) {
            const int next = start + i * increment;
            const auto slope = line[next];
            line[next]       = line[next - increment] + (previous + slope) * float(step * .5);
            previous         = slope;
        }
        const auto offset = interpolate(line, start, increment, count, at);
        for (int i = 0; i <= count; ++i)
            line[start + i * increment] -= offset;
    };
    for (int y = 0; y <= steps.height(); ++y)
        integrate(horizontal, y * stride, 1, steps.width(), stepX, ax);
    for (int x = 0; x <= steps.width(); ++x)
        integrate(vertical, x, stride, steps.height(), stepY, ay);

    const auto  center = mapping.map(QPointF(canvas.width() * .5, canvas.height() * .5)) -
                         QPointF(origin) - QPointF(.5, .5);
    const qreal focal  = qMax(
        1., QLineF(mapping.map(QPointF{}), mapping.map(QPointF(canvas.width(), canvas.height())))
                    .length() *
                focalRatio);
    // Normals can be noisy. Average the two paths from the anchor instead of
    // letting the warp depend on one traversal of the grid.
    for (int y = 0; y <= steps.height(); ++y) {
        const auto anchorColumn = interpolate(vertical, y * stride, 1, steps.width(), ax);
        for (int x = 0; x <= steps.width(); ++x) {
            const int     i         = y * stride + x;
            const auto    anchorRow = interpolate(horizontal, x, stride, steps.height(), ay);
            const QPointF position(x * stepX, y * stepY);
            const auto    delta = (horizontal[i] + vertical[i] + anchorRow + anchorColumn) * .5f;
            // A fold closer to the camera also changes scale along its ray.
            const qreal perspective = qBound(-.12, delta.z() / focal, .12);
            grid[i] = position + delta.toPointF() - (position - center) * perspective;
        }
    }

    return {region, steps, std::move(grid)};
}
QPointF SurfaceTextWarp::unmap(const QPointF &canvasPoint) const
{
    if (grid.empty())
        return canvasPoint;
    const QPointF origin = QPointF(region.topLeft()) + QPointF(.5, .5);
    return sourcePoint(canvasPoint - origin) + origin;
}

QPolygonF SurfaceTextWarp::outline(const QPolygonF &polygon, QPointF *bottomRight) const
{
    if (grid.empty())
        return {};
    QPolygonF     outline;
    const QPointF origin = QPointF(region.topLeft()) + QPointF(.5, .5);
    QPointF       previous, output;
    for (int edge = 0; edge < polygon.size(); ++edge) {
        const auto start   = polygon[edge] - origin;
        const auto end     = polygon[(edge + 1) % polygon.size()] - origin;
        const int  samples = int(qBound(1., std::ceil(QLineF(start, end).length() / 8.), 128.));
        for (int sample = 0; sample < samples; ++sample) {
            const auto target = start + (end - start) * (qreal(sample) / samples);
            const auto guess  = outline.isEmpty() ? target : output + target - previous;
            output            = outputPoint(target, guess);
            outline.append(output + origin);
            if (bottomRight && edge == 2 && sample == 0)
                *bottomRight = outline.back();
            previous = target;
        }
    }
    if (!outline.isEmpty())
        outline.append(outline.front());
    return outline;
}

QImage projectSurfaceText(const QImage &text, const QPoint &origin, const QPointF &anchor,
                          const QImage &normals, const QSize &canvas, const QTransform &mapping,
                          qreal focalRatio, qreal strength)
{
    const auto warp = surfaceTextWarp(QRect(origin, text.size()), anchor, normals, canvas, mapping,
                                      focalRatio, strength);
    if (warp.grid.empty())
        return text;
    const auto &grid  = warp.grid;
    const auto &steps = warp.steps;
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

std::optional<SurfaceCrease> SurfaceGeometry::textCrease(const QImage        &selected,
                                                         const QList<QPoint> &seeds,
                                                         const QSize &canvas, const QPoint &anchor,
                                                         const Cancel &cancel) const
{
    if (seeds.size() < 2 || !isValid() || selected.size() != normals.size() || canvas.isEmpty())
        return {};
    const qreal sx         = qreal(normals.width()) / canvas.width();
    const qreal sy         = qreal(normals.height()) / canvas.height();
    const auto  map        = [&](const QPoint &p) { return QPointF(p.x() * sx, p.y() * sy); };
    const auto  atAnchor   = sampleNormal(normals, canvas, anchor);
    const auto  similarity = [&](const QPoint &p) {
        return QVector3D::dotProduct(atAnchor, sampleNormal(normals, canvas, p));
    };
    const auto firstSeed  = std::max_element(seeds.cbegin(), seeds.cend(), [&](auto a, auto b) {
        return similarity(a) < similarity(b);
    });
    const auto first      = sampleNormal(normals, canvas, *firstSeed);
    const auto secondSeed = std::min_element(seeds.cbegin(), seeds.cend(), [&](auto a, auto b) {
        return QVector3D::dotProduct(first, sampleNormal(normals, canvas, a)) <
               QVector3D::dotProduct(first, sampleNormal(normals, canvas, b));
    });
    const auto second     = sampleNormal(normals, canvas, *secondSeed);
    if (first.z() < .15f || second.z() < .15f || QVector3D::dotProduct(first, second) > .94f)
        return {};

    const int  shortEdge = qMin(normals.width(), normals.height());
    const int  reach     = qBound(3, shortEdge / 85, 16);
    const int  stride    = qMax(1, shortEdge / 240);
    const auto delta     = second - first;
    const auto score     = [&](const QPoint &p) {
        return QVector3D::dotProduct(delta, normalAt(normals, p.x(), p.y()));
    };
    const auto supported = [&](const QPoint &p, const QPoint &step, float sign) {
        for (int i = 0; i <= reach; ++i) {
            const auto q = p + i * step;
            if (!selected.rect().contains(q) || !validity.constScanLine(q.y())[q.x()])
                return false;
            if (i && !continuousDepth(disparity, validity, q - step, q, step))
                return false;
            if (selected.constScanLine(q.y())[q.x()] &&
                sign * score(q) > delta.lengthSquared() * .16f)
                return true;
        }
        return false;
    };
    std::vector<QPointF> crossings;
    for (const auto step : {QPoint(1, 0), QPoint(0, 1)}) {
        const int advance = step.x() ? normals.width() : normals.height();
        const int lines   = step.x() ? normals.height() : normals.width();
        for (int line = 0; line < lines; line += stride) {
            if (cancel->load())
                return {};
            QPoint p      = step.x() ? QPoint(0, line) : QPoint(line, 0);
            float  before = score(p);
            for (int i = 1; i < advance; ++i, p += step) {
                const auto  q     = p + step;
                const float after = score(q);
                if ((before < 0) != (after < 0) && validity.constScanLine(p.y())[p.x()] &&
                    validity.constScanLine(q.y())[q.x()] &&
                    continuousDepth(disparity, validity, p, q, step)) {
                    const float sign = before < 0 ? -1.f : 1.f;
                    if (supported(p, -step, sign) && supported(q, step, -sign))
                        crossings.push_back(QPointF(p) + QPointF(.5, .5) +
                                            QPointF(step) * (before / (before - after)));
                }
                before = after;
            }
        }
    }
    if (crossings.size() < 8)
        return {};

    const qreal tolerance = qMax(1.5, shortEdge * .004);
    const auto  normal    = [](const QLineF &line) {
        const auto direction = (line.p2() - line.p1()) / line.length();
        return QPointF(-direction.y(), direction.x());
    };
    QLineF     best;
    size_t     bestCount     = 0;
    const auto skip          = qMax<size_t>(1, crossings.size() / 40);
    const auto supportStride = qMax<size_t>(1, crossings.size() / 2048);
    for (size_t i = 0; i < crossings.size(); i += skip) {
        if (cancel->load())
            return {};
        for (size_t j = i + skip; j < crossings.size(); j += skip) {
            const QLineF candidate(crossings[i], crossings[j]);
            if (candidate.length() < shortEdge * .08)
                continue;
            const auto n = normal(candidate);
            if (QPointF::dotProduct(n, map(*firstSeed) - candidate.p1()) *
                    QPointF::dotProduct(n, map(*secondSeed) - candidate.p1()) >=
                0.)
                continue;
            size_t count = 0;
            for (size_t k = 0; k < crossings.size(); k += supportStride)
                count += qAbs(QPointF::dotProduct(n, crossings[k] - candidate.p1())) < tolerance;
            if (count > bestCount) {
                best      = candidate;
                bestCount = count;
            }
        }
    }
    if (bestCount < 8 || bestCount * supportStride * 3 < crossings.size())
        return {};
    const auto n = normal(best);
    QPointF    center;
    size_t     inliers = 0;
    for (const auto &p : crossings)
        if (qAbs(QPointF::dotProduct(n, p - best.p1())) < tolerance) {
            center += p;
            ++inliers;
        }
    center /= qreal(inliers);
    qreal xx = 0., xy = 0., yy = 0.;
    for (const auto &p : crossings)
        if (qAbs(QPointF::dotProduct(n, p - best.p1())) < tolerance) {
            const auto d = p - center;
            xx += d.x() * d.x();
            xy += d.x() * d.y();
            yy += d.y() * d.y();
        }
    const qreal   angle = .5 * std::atan2(2. * xy, xx - yy);
    const QPointF direction(std::cos(angle), std::sin(angle));
    const QPointF perpendicular(-direction.y(), direction.x());
    const bool    firstPositive = QPointF::dotProduct(perpendicular, map(*firstSeed) - center) > 0.;
    const bool    anchorPositive = QPointF::dotProduct(perpendicular, map(anchor) - center) > 0.;
    const auto    extent         = direction * (shortEdge / 4.);
    const auto    unmap = [&](const QPointF &p) { return QPointF(p.x() / sx, p.y() / sy); };
    QLineF        edge(unmap(center - extent), unmap(center + extent));
    if (anchorPositive)
        edge = QLineF(edge.p2(), edge.p1());
    const bool anchorOnFirst = anchorPositive == firstPositive;
    return SurfaceCrease{edge, anchorOnFirst ? first : second, anchorOnFirst ? second : first};
}

void SurfaceGeometry::closeTextCrease(QImage &selected, const QSize &canvas,
                                      const SurfaceCrease &crease, const Cancel &cancel) const
{
    const auto    scale     = QTransform::fromScale(qreal(selected.width()) / canvas.width(),
                                                    qreal(selected.height()) / canvas.height());
    const auto    edge      = scale.map(crease.edge);
    const auto    direction = (edge.p2() - edge.p1()) / edge.length();
    const QPointF n(-direction.y(), direction.x());
    const int     reach    = qBound(3, qMin(selected.width(), selected.height()) / 85, 16);
    const auto    coverage = selected;
    for (int y = 0; y < selected.height(); ++y) {
        if (cancel->load())
            return;
        auto *out = selected.scanLine(y);
        for (int x = 0; x < selected.width(); ++x) {
            const QPoint p(x, y);
            if (out[x] || !validity.constScanLine(y)[x] ||
                qAbs(QPointF::dotProduct(n, QPointF(p) + QPointF(.5, .5) - edge.p1())) > reach)
                continue;
            const auto covered = [&](int side) {
                QPoint previous = p;
                for (int i = 1; i <= reach; ++i) {
                    const auto q = p + (n * (i * side)).toPoint();
                    if (!coverage.rect().contains(q) || !validity.constScanLine(q.y())[q.x()] ||
                        !continuousDepth(disparity, validity, previous, q, q - previous))
                        return false;
                    if (coverage.constScanLine(q.y())[q.x()])
                        return true;
                    previous = q;
                }
                return false;
            };
            if (covered(-1) && covered(1))
                out[x] = 255;
        }
    }
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
                                      const Cancel &cancel, const QImage &protection,
                                      const QImage &face)
{
    if (selected.isNull() || canvas.isEmpty() ||
        (!protection.isNull() && protection.size() != selected.size()) ||
        (!face.isNull() && face.size() != selected.size()))
        return {};
    auto coverage = selected;
    if (!protection.isNull() || !face.isNull()) {
        for (int y = 0; y < coverage.height(); ++y) {
            if (cancel->load())
                return {};
            auto       *out     = coverage.scanLine(y);
            const auto *blocked = protection.isNull() ? nullptr : protection.constScanLine(y);
            const auto *inside  = face.isNull() ? nullptr : face.constScanLine(y);
            for (int x = 0; x < coverage.width(); ++x)
                if ((blocked && blocked[x]) || (inside && !inside[x]))
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
