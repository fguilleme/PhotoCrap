#include "RelightFilter.h"
#include "ImagePixels.h"
#include "Layer.h"
#include <QVector3D>
#include <cmath>
#include <numeric>

namespace PhotoCrap::Filters
{
namespace
{
struct Lamp {
    QVector3D position, color;
    float     radius;
};
} // namespace
QImage relight(const QImage &source, const Layer &layer, const QPoint &origin)
{
    const auto &map    = layer.surfaceNormals;
    const QSize canvas = layer.surfaceNormalCanvas;
    if (map.format() != QImage::Format_RGBA8888 || canvas.isEmpty() || source.isNull())
        return source;
    const float   unit = qMax(canvas.width(), canvas.height());
    const float   w = canvas.width() / unit, h = canvas.height() / unit;
    QVector<Lamp> lamps;
    for (const auto &item : layer.parameters.value("lights").toList().mid(0, 16)) {
        const auto params = item.toMap();
        const auto number = [&](const char *key, float fallback, float low, float high) {
            return qBound(low, params.value(key, fallback).toFloat(), high) / 100.f;
        };
        const QColor c(params.value("color").toString());
        const float  power = number("intensity", 100, 0, 400);
        if (!c.isValid() || power <= 0)
            continue;
        lamps.append({{w * number("x", 50, -100, 200), -h * number("y", 50, -100, 200),
                       number("z", 65, -100, 200)},
                      QVector3D(c.redF(), c.greenF(), c.blueF()) * power,
                      .025f + .65f * number("softness", 45, 0, 100)});
    }
    const double ambient = qBound(0., layer.parameters.value("ambient", 100).toDouble(), 100.) / 100.;
    if (lamps.isEmpty() && ambient == 1.)
        return source;

    // Evaluate irradiance on the geometry grid, then interpolate it at document resolution.
    QVector<QVector3D> illumination(map.width() * map.height());
    auto              *grid = illumination.data();
    QVector<int>       rows(map.height());
    std::iota(rows.begin(), rows.end(), 0);
    ImageUtils::blockingMap(rows, [&](int y) {
        const auto *row = map.constScanLine(y);
        for (int x = 0; x < map.width(); ++x) {
            const auto *p = row + 4 * x;
            if (!p[3])
                continue;
            const QVector3D n = QVector3D(p[0] - 127.5f, p[1] - 127.5f, p[2] - 127.5f).normalized();
            const QVector3D pos(w * (x + .5f) / map.width(), -h * (y + .5f) / map.height(),
                                ((p[3] - 1) / 254.f - .5f) * .5f);
            QVector3D       sum;
            for (const auto &lamp : lamps) {
                const auto  delta   = lamp.position - pos;
                const float squared = delta.lengthSquared(), r2 = lamp.radius * lamp.radius;
                const float incidence =
                    QVector3D::dotProduct(n, delta) / std::sqrt(squared + .0001f);
                // A finite emitter softens the terminator and broadens the falloff.
                const float spread  = lamp.radius / std::sqrt(squared + r2);
                const float diffuse = qBound(0.f, (incidence + spread) / (1 + spread), 1.f);
                sum += lamp.color * (diffuse * (.22f + r2) / (squared + r2 + .04f));
            }
            grid[y * map.width() + x] = sum;
        }
    });
    const QTransform toSurface = layer.surfaceNormalTransform.inverted();
    auto             render    = [&]<typename Pixel>() {
        using P     = ImageUtils::PixelAccess<Pixel>;
        QImage out  = source.convertToFormat(P::format);
        auto  *data = out.bits();
        if (!data)
            return QImage{};
        const auto stride = out.bytesPerLine();
        rows.resize(out.height());
        std::iota(rows.begin(), rows.end(), 0);
        ImageUtils::blockingMap(rows, [&](int y) {
            auto *row = reinterpret_cast<Pixel *>(data + y * stride);
            for (int x = 0; x < out.width(); ++x) {
                const QPointF pt = toSurface.map(QPointF(origin.x() + x + .5, origin.y() + y + .5));
                if (pt.x() < 0 || pt.y() < 0 || pt.x() >= canvas.width() ||
                    pt.y() >= canvas.height())
                    continue;
                const float u =
                    qBound(0., pt.x() * map.width() / canvas.width() - .5, double(map.width() - 1));
                const float v = qBound(0., pt.y() * map.height() / canvas.height() - .5,
                                                      double(map.height() - 1));
                const int   i = int(u), j = int(v), k = qMin(i + 1, map.width() - 1),
                          l     = qMin(j + 1, map.height() - 1);
                const auto line = [&](int yy) {
                    const auto *g = grid + yy * map.width();
                    return g[i] * (1 - (u - i)) + g[k] * (u - i);
                };
                const auto light   = line(j) * (1 - (v - j)) + line(l) * (v - j);
                const auto channel = [ambient](double c, float e) {
                    return c * (ambient + e) / (1 + c * e / 255.);
                };
                const auto p = row[x];
                row[x] = P::rgba(channel(P::red(p), light.x()), channel(P::green(p), light.y()),
                                                channel(P::blue(p), light.z()), P::alpha(p));
            }
        });
        return out.convertToFormat(P::premultiplied);
    };
    return ImageUtils::is16Bit(source) ? render.template operator()<QRgba64>()
                                       : render.template operator()<QRgb>();
}
} // namespace PhotoCrap::Filters
