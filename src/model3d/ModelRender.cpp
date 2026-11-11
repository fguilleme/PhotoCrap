#include "ImageUtils.h"
#include "Model3DPrivate.h"

#include <QColorSpace>
#include <QQuaternion>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>

namespace PhotoCrap::Model3D
{
namespace
{
constexpr int    MaxRasterSide = 1600;
constexpr qint64 MaxSamples    = 160'000'000;

bool number(QVariantMap &_map, const QString &_key, double _min, double _max)
{
    bool       ok    = false;
    const auto value = _map.value(_key).toDouble(&ok);
    if (!ok || !std::isfinite(value) || value < _min || value > _max)
        return false;
    _map[_key] = value;
    return true;
}

QTransform placement(const QVariantMap &_pose)
{
    const auto basis = _pose.value("basis").toList();
    return {basis[0].toDouble(), basis[1].toDouble(),         basis[2].toDouble(),
            basis[3].toDouble(), _pose.value("x").toDouble(), _pose.value("y").toDouble()};
}

float edge(const QPointF &_a, const QPointF &_b, const QPointF &_point)
{
    return float((_b.x() - _a.x()) * (_point.y() - _a.y()) -
                 (_b.y() - _a.y()) * (_point.x() - _a.x()));
}

float wrapped(float _value, Material::Wrap _mode)
{
    if (_mode == Material::Wrap::clamp)
        return qBound(0.f, _value, 1.f);
    const float tile     = std::floor(_value);
    const float fraction = _value - tile;
    return _mode == Material::Wrap::mirror && int(tile) % 2 != 0 ? 1.f - fraction : fraction;
}

float linear(int _byte)
{
    static const auto table = [] {
        std::array<float, 256> values{};
        for (int i = 0; i < 256; ++i) {
            const float s = i / 255.f;
            values[i]     = s <= .04045f ? s / 12.92f : std::pow((s + .055f) / 1.055f, 2.4f);
        }
        return values;
    }();
    return table[_byte];
}

float sRgb(float _value)
{
    const float l = qBound(0.f, _value, 1.f);
    return l <= .0031308f ? 12.92f * l : 1.055f * std::pow(l, 1.f / 2.4f) - .055f;
}

struct Projected {
    QPointF   pixel;
    QVector3D position, normal;
    QVector2D uv;
    QVector4D color;
    float     inverseZ;
};
struct Lighting {
    QVector3D direction, color;
    float     ambient, intensity;
};
} // namespace

QVariantMap pose(const QSize &_canvas)
{
    const double unit = qMin(_canvas.width(), _canvas.height()) * .72;
    return {{"x", _canvas.width() / 2.},
            {"y", _canvas.height() / 2.},
            {"scale", 100.},
            {"pitch", 0.},
            {"yaw", 0.},
            {"roll", 0.},
            {"basis", QVariantList{unit, 0., 0., unit}}};
}

QVariantMap light()
{
    return {{"azimuth", -35.},
            {"elevation", 35.},
            {"intensity", 80.},
            {"ambient", 45.},
            {"color", QStringLiteral("#ffffff")}};
}

bool normalize(QVariantMap &_parameters)
{
    auto p = _parameters.value("pose3d").toMap();
    auto l = _parameters.value("light3d").toMap();
    if (!number(p, "x", -1000000., 1000000.) || !number(p, "y", -1000000., 1000000.) ||
        !number(p, "scale", 1., 1000.) || !number(p, "pitch", -180., 180.) ||
        !number(p, "yaw", -180., 180.) || !number(p, "roll", -180., 180.) ||
        !number(l, "azimuth", -180., 180.) || !number(l, "elevation", -90., 90.) ||
        !number(l, "intensity", 0., 200.) || !number(l, "ambient", 0., 100.))
        return false;
    const auto basis = p.value("basis").toList();
    if (basis.size() != 4)
        return false;
    for (const auto &entry : basis) {
        bool         ok    = false;
        const double value = entry.toDouble(&ok);
        if (!ok || !std::isfinite(value) || qAbs(value) > 1000000.)
            return false;
    }
    if (!placement(p).isInvertible())
        return false;
    const QColor color(l.value("color").toString());
    if (!color.isValid() || color.alpha() != 255)
        return false;
    l["color"]             = color.name();
    _parameters["pose3d"]  = p;
    _parameters["light3d"] = l;
    return true;
}

bool transformPlacement(QVariantMap &_parameters, const QTransform &_transform)
{
    if (!_transform.isAffine())
        return false;
    auto p = _parameters.value("pose3d").toMap();
    if (p.value("basis").toList().size() != 4)
        return false;
    const auto mapped     = placement(p) * _transform;
    p["x"]                = mapped.dx();
    p["y"]                = mapped.dy();
    p["basis"]            = QVariantList{mapped.m11(), mapped.m12(), mapped.m21(), mapped.m22()};
    _parameters["pose3d"] = p;
    return normalize(_parameters);
}

PixelPlane render(const Asset &_asset, const QVariantMap &_parameters, const QSize &_canvas,
                  QImage::Format _format, const Continue &_continue)
{
    const bool deep = _format == QImage::Format_RGBA64_Premultiplied;
    if (!deep && _format != QImage::Format_ARGB32_Premultiplied)
        return {};
    auto parameters = _parameters;
    if (!normalize(parameters) || _canvas.isEmpty() || (_continue && !_continue()))
        return {};
    const auto p = parameters["pose3d"].toMap(), l = parameters["light3d"].toMap();
    const auto rotation =
        QQuaternion::fromEulerAngles(p["pitch"].toFloat(), p["yaw"].toFloat(), p["roll"].toFloat());
    const auto     map       = placement(p);
    const float    scale     = p["scale"].toFloat() / 100.f;
    const float    azimuth   = qDegreesToRadians(l["azimuth"].toFloat());
    const float    elevation = qDegreesToRadians(l["elevation"].toFloat());
    const QColor   lightColor(l["color"].toString());
    const Lighting lighting{
        {std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
         std::cos(azimuth) * std::cos(elevation)},
        {linear(lightColor.red()), linear(lightColor.green()), linear(lightColor.blue())},
        l["ambient"].toFloat() / 100.f,
        l["intensity"].toFloat() / 100.f};
    // The common camera looks along -Z. Each layer is rendered into its own
    // transparent region and then participates in the document's 2D stack.
    QPointF minimum(std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
    QPointF maximum(-minimum.x(), -minimum.y());
    QVector<QVector<Projected>> projected;
    projected.reserve(_asset.primitives.size());
    for (const auto &primitive : _asset.primitives) {
        QVector<Projected> vertices;
        vertices.reserve(primitive.vertices.size());
        for (const auto &vertex : primitive.vertices) {
            const auto  position    = rotation.rotatedVector(vertex.position);
            const float inverseZ    = 1.f / (3.f - position.z());
            const float perspective = 3.f * inverseZ * scale;
            const auto  pixel =
                map.map(QPointF(position.x() * perspective, -position.y() * perspective));
            minimum.setX(qMin(minimum.x(), pixel.x()));
            minimum.setY(qMin(minimum.y(), pixel.y()));
            maximum.setX(qMax(maximum.x(), pixel.x()));
            maximum.setY(qMax(maximum.y(), pixel.y()));
            vertices.append(Projected{pixel, position, rotation.rotatedVector(vertex.normal),
                                      vertex.uv, vertex.color, inverseZ});
        }
        projected.append(std::move(vertices));
    }
    const auto nativeBounds = QRectF(minimum, maximum)
                                  .adjusted(-1, -1, 1, 1)
                                  .intersected(QRectF(QPointF{}, QSizeF(_canvas)))
                                  .toAlignedRect();
    auto plane = PixelPlane::uniform(_canvas, ImageUtils::blank({1, 1}, _format));
    if (nativeBounds.isEmpty())
        return plane;
    const double ratio =
        qMin(1., double(MaxRasterSide) / qMax(nativeBounds.width(), nativeBounds.height()));
    const QSize  target(qMax(1, qRound(nativeBounds.width() * ratio)),
                       qMax(1, qRound(nativeBounds.height() * ratio)));
    const double sx = double(target.width()) / nativeBounds.width();
    const double sy = double(target.height()) / nativeBounds.height();
    const QRect  area(QPoint{}, target);
    auto         image = ImageUtils::blank(target, _format);
    if (image.isNull())
        return {};
    QVector<float> depth(qsizetype(target.width()) * target.height(), 0.f);
    qint64         samples = 0, triangles = 0;
    const bool     mirrored = map.determinant() < 0.;
    for (qsizetype i = 0; i < _asset.primitives.size(); ++i) {
        const auto &primitive = _asset.primitives[i];
        const auto &material  = primitive.material;
        auto &      vertices  = projected[i];
        for (auto &v : vertices)
            v.pixel = {(v.pixel.x() - nativeBounds.x()) * sx,
                       (v.pixel.y() - nativeBounds.y()) * sy};
        for (const auto &indices : primitive.triangles) {
            if (++triangles % 128 == 0 && _continue && !_continue())
                return {};
            const auto &a = vertices[indices[0]], &b = vertices[indices[1]],
                       &c          = vertices[indices[2]];
            const float signedArea = edge(a.pixel, b.pixel, c.pixel);
            const bool  front      = (signedArea < 0) != mirrored;
            if (qAbs(signedArea) < 1e-7f || (!front && !material.doubleSided))
                continue;
            const QRect region = QRectF(QPointF(std::min({a.pixel.x(), b.pixel.x(), c.pixel.x()}),
                                                std::min({a.pixel.y(), b.pixel.y(), c.pixel.y()})),
                                        QPointF(std::max({a.pixel.x(), b.pixel.x(), c.pixel.x()}),
                                                std::max({a.pixel.y(), b.pixel.y(), c.pixel.y()})))
                                     .toAlignedRect()
                                     .intersected(area);
            samples += qint64(region.width()) * region.height();
            if (samples > MaxSamples)
                return {};
            const auto flat =
                QVector3D::crossProduct(b.position - a.position, c.position - a.position)
                    .normalized();
            const bool textured = !material.texture.isNull();
            for (int y = region.top(); y <= region.bottom(); ++y) {
                auto *pixels = image.scanLine(y);
                auto *depths = depth.data() + y * target.width();
                for (int x = region.left(); x <= region.right(); ++x) {
                    const QPointF center(x + .5, y + .5);
                    float         u = edge(b.pixel, c.pixel, center) / signedArea;
                    float         v = edge(c.pixel, a.pixel, center) / signedArea;
                    float         w = 1.f - u - v;
                    if (u < -1e-6f || v < -1e-6f || w < -1e-6f)
                        continue;
                    u *= a.inverseZ;
                    v *= b.inverseZ;
                    w *= c.inverseZ;
                    const float inverseZ = u + v + w;
                    if (inverseZ <= depths[x])
                        continue;
                    u /= inverseZ;
                    v /= inverseZ;
                    w /= inverseZ;
                    QRgb texel = qRgb(255, 255, 255);
                    if (textured) {
                        const auto uv = u * a.uv + v * b.uv + w * c.uv;
                        const int  tx =
                            qMin(material.texture.width() - 1,
                                 int(wrapped(uv.x(), material.u) * material.texture.width()));
                        const int ty =
                            qMin(material.texture.height() - 1,
                                 int(wrapped(uv.y(), material.v) * material.texture.height()));
                        texel =
                            reinterpret_cast<const QRgb *>(material.texture.constScanLine(ty))[tx];
                    }
                    const auto base =
                        primitive.vertexColors
                            ? material.color * (u * a.color + v * b.color + w * c.color)
                            : material.color;
                    if (material.alphaMask &&
                        qAlpha(texel) / 255.f * base.w() < material.alphaCutoff)
                        continue;
                    const auto smooth = u * a.normal + v * b.normal + w * c.normal;
                    const auto normal =
                        (smooth.isNull() ? flat : smooth.normalized()) * (front ? 1.f : -1.f);
                    const float diffuse =
                        material.unlit
                            ? 0.f
                            : lighting.intensity *
                                  qMax(0.f, QVector3D::dotProduct(normal, lighting.direction));
                    const float ambient = material.unlit ? 1.f : lighting.ambient;
                    const float red     = sRgb(base.x() * linear(qRed(texel)) *
                                           (ambient + diffuse * lighting.color.x()));
                    const float green   = sRgb(base.y() * linear(qGreen(texel)) *
                                             (ambient + diffuse * lighting.color.y()));
                    const float blue    = sRgb(base.z() * linear(qBlue(texel)) *
                                            (ambient + diffuse * lighting.color.z()));
                    if (deep)
                        reinterpret_cast<QRgba64 *>(pixels)[x] =
                            QRgba64::fromRgba64(qRound(65535.f * red), qRound(65535.f * green),
                                                qRound(65535.f * blue), 65535);
                    else
                        reinterpret_cast<QRgb *>(pixels)[x] =
                            qRgb(qRound(255.f * red), qRound(255.f * green), qRound(255.f * blue));
                    depths[x] = inverseZ;
                }
            }
        }
    }
    if (_continue && !_continue())
        return {};
    if (image.size() != nativeBounds.size())
        image = image.scaled(nativeBounds.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (image.isNull() || !plane.writeRegion(nativeBounds.topLeft(), image, _canvas))
        return {};
    return plane;
}
} // namespace PhotoCrap::Model3D
