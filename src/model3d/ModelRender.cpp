#include "ImageUtils.h"
#include "Model3DPrivate.h"

#include <QColorSpace>
#include <QQuaternion>
#include <QThreadPool>
#include <QtConcurrent>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace PhotoCrap::Model3D
{
namespace
{
constexpr int       MaxBandPixels     = 1024 * 1024;
constexpr int       PreviewBandPixels = 64 * 1024;
constexpr qsizetype GeometryBatchSize = 16 * 1024;

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

float inverseDepth(const QVector3D &_position)
{
    return 1.f / (CameraDistance - _position.z());
}

QPointF project(const QVector3D &_position, float _inverseDepth, float _scale = 1.f)
{
    const float perspective = CameraDistance * _inverseDepth * _scale;
    return {_position.x() * perspective, -_position.y() * perspective};
}

struct Bounds {
    QPointF minimum{std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    QPointF maximum{-minimum.x(), -minimum.y()};

    void add(const QPointF &_point)
    {
        minimum.setX(qMin(minimum.x(), _point.x()));
        minimum.setY(qMin(minimum.y(), _point.y()));
        maximum.setX(qMax(maximum.x(), _point.x()));
        maximum.setY(qMax(maximum.y(), _point.y()));
    }

    QRectF rect() const
    {
        return {minimum, maximum};
    }

    void include(const Bounds &_other)
    {
        add(_other.minimum);
        add(_other.maximum);
    }
};

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
    float     inverseZ;
};
RenderView renderView(const QVariantMap &parameters)
{
    const auto p = parameters["pose3d"].toMap(), l = parameters["light3d"].toMap();
    const auto rotation =
        QQuaternion::fromEulerAngles(p["pitch"].toFloat(), p["yaw"].toFloat(), p["roll"].toFloat());
    const float  azimuth   = qDegreesToRadians(l["azimuth"].toFloat());
    const float  elevation = qDegreesToRadians(l["elevation"].toFloat());
    const QColor lightColor(l["color"].toString());
    return {rotation,
            placement(p),
            {std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
             std::cos(azimuth) * std::cos(elevation)},
            {linear(lightColor.red()), linear(lightColor.green()), linear(lightColor.blue())},
            p["scale"].toFloat() / 100.f,
            l["ambient"].toFloat() / 100.f,
            l["intensity"].toFloat() / 100.f};
}
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

QVariantMap fittedPose(const Asset &_asset, const QRectF &_bounds, const Continue &_continue)
{
    Bounds bounds;
    for (const auto &primitive : _asset.primitives) {
        if (_continue && !_continue())
            return {};
        for (const auto &vertex : primitive.vertices)
            bounds.add(project(vertex.position, inverseDepth(vertex.position)));
    }
    const auto projected = bounds.rect();
    if (projected.isEmpty() || _bounds.isEmpty())
        return {};
    const double unit   = qMin(1'000'000., qMax(_bounds.width(), _bounds.height()) /
                                               qMax(projected.width(), projected.height()));
    const auto   center = _bounds.center() - projected.center() * unit;
    auto         result = pose(_bounds.size().toSize());
    result["x"]         = center.x();
    result["y"]         = center.y();
    result["basis"]     = QVariantList{unit, 0., 0., unit};
    return result;
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

namespace
{
QThreadPool &modelPreviewPool()
{
    struct Pool : QThreadPool {
        Pool()
        {
            setMaxThreadCount(qBound(1, QThread::idealThreadCount() - 1, 8));
        }
    };
    // The document preview itself already runs on Qt's global pool.
    static Pool pool;
    return pool;
}

PixelPlane renderCpu(const Asset &_asset, const QVariantMap &_parameters, const QSize &_canvas,
                     QImage::Format _format, const Continue &_continue, bool _preview)
{
    const bool deep = _format == QImage::Format_RGBA64_Premultiplied;
    if (!deep && _format != QImage::Format_ARGB32_Premultiplied)
        return {};
    auto parameters = _parameters;
    if (!normalize(parameters) || _canvas.isEmpty() || (_continue && !_continue()))
        return {};
    const auto  view = renderView(parameters);
    const auto &map  = view.placement;
    // The common camera looks along -Z. Each layer is rendered into its own
    // transparent region and then participates in the document's 2D stack.
    Bounds                      bounds;
    QVector<QVector<Projected>> projected;
    projected.reserve(_asset.primitives.size());
    struct ProjectionBatch {
        const Vertex *source;
        Projected    *destination;
        qsizetype     size;
        Bounds        bounds;
    };
    QVector<ProjectionBatch> projectionBatches;
    for (const auto &primitive : _asset.primitives) {
        projected.append(QVector<Projected>(primitive.vertices.size()));
        auto *destination = projected.last().data();
        for (qsizetype first = 0; first < primitive.vertices.size(); first += GeometryBatchSize)
            projectionBatches.append({primitive.vertices.constData() + first,
                                      destination + first,
                                      qMin(GeometryBatchSize, primitive.vertices.size() - first),
                                      {}});
    }
    const auto projectBatch = [&](ProjectionBatch &batch) {
        for (qsizetype i = 0; i < batch.size; ++i) {
            const auto &vertex   = batch.source[i];
            const auto  position = view.rotation.rotatedVector(vertex.position);
            const float inverseZ = inverseDepth(position);
            const auto  pixel    = map.map(project(position, inverseZ, view.scale));
            batch.bounds.add(pixel);
            batch.destination[i] = {pixel, position, view.rotation.rotatedVector(vertex.normal),
                                    inverseZ};
        }
    };
    if (_preview && projectionBatches.size() > 1)
        QtConcurrent::blockingMap(&modelPreviewPool(), projectionBatches, projectBatch);
    else
        for (auto &batch : projectionBatches) {
            if (_continue && !_continue())
                return {};
            projectBatch(batch);
        }
    for (const auto &batch : std::as_const(projectionBatches))
        bounds.include(batch.bounds);
    const auto nativeBounds = bounds.rect()
                                  .adjusted(-1, -1, 1, 1)
                                  .intersected(QRectF(QPointF{}, QSizeF(_canvas)))
                                  .toAlignedRect();
    auto       plane        = PixelPlane::uniform(_canvas, ImageUtils::blank({1, 1}, _format));
    if (nativeBounds.isEmpty())
        return plane;
    // Bin by rows. Each band uses document pixels and has a bounded color and depth buffer.
    // Keep triangle order for equal-depth faces.
    const int width = nativeBounds.width(), height = nativeBounds.height();
    // Keep final-render memory bounded. Smaller preview bands give cores
    // independent rows to draw even when the whole proxy fits in one band.
    const int   rowsPerBand = qMax(1, (_preview ? PreviewBandPixels : MaxBandPixels) / width);
    const QRect area(0, 0, width, height);
    struct Triangle {
        const Projected *vertices;
        const Primitive *primitive;
        const Face      *indices;
        QRect            region;
        float            signedArea;
        bool             front;
    };
    struct TriangleBatch {
        const Primitive  *primitive;
        const Projected  *vertices;
        const Face       *faces;
        qsizetype         size;
        QVector<Triangle> triangles;
        qint64            samples = 0;
    };
    QVector<TriangleBatch>             batches;
    QVector<QVector<const Triangle *>> bands((height + rowsPerBand - 1) / rowsPerBand);
    const bool                         mirrored    = map.determinant() < 0.;
    const qint64                       sampleLimit = qMax(160'000'000LL, 16LL * width * height);
    for (qsizetype i = 0; i < _asset.primitives.size(); ++i) {
        const auto &primitive  = _asset.primitives[i];
        auto       &projection = projected[i];
        for (auto &v : projection)
            v.pixel -= nativeBounds.topLeft();
        for (qsizetype first = 0; first < primitive.triangles.size(); first += GeometryBatchSize)
            batches.append({&primitive,
                            projection.constData(),
                            primitive.triangles.constData() + first,
                            qMin(GeometryBatchSize, primitive.triangles.size() - first),
                            {},
                            0});
    }
    const auto prepareTriangles = [&](TriangleBatch &batch) {
        const auto *vertices = batch.vertices;
        for (qsizetype i = 0; i < batch.size; ++i) {
            if (i % 128 == 0 && _continue && !_continue())
                return;
            const auto &indices = batch.faces[i];
            const auto &a = vertices[indices[0]], &b = vertices[indices[1]],
                       &c          = vertices[indices[2]];
            const float signedArea = edge(a.pixel, b.pixel, c.pixel);
            const bool  front      = (signedArea < 0) != mirrored;
            if (qAbs(signedArea) < 1e-7f || (!front && !batch.primitive->material.doubleSided))
                continue;
            const QRect region = QRectF(QPointF(std::min({a.pixel.x(), b.pixel.x(), c.pixel.x()}),
                                                std::min({a.pixel.y(), b.pixel.y(), c.pixel.y()})),
                                        QPointF(std::max({a.pixel.x(), b.pixel.x(), c.pixel.x()}),
                                                std::max({a.pixel.y(), b.pixel.y(), c.pixel.y()})))
                                     .toAlignedRect()
                                     .intersected(area);
            if (region.isEmpty())
                continue;
            batch.samples += qint64(region.width()) * region.height();
            if (batch.samples > sampleLimit)
                return;
            batch.triangles.append(
                {vertices, batch.primitive, &indices, region, signedArea, front});
        }
    };
    qint64     samples      = 0;
    const auto binTriangles = [&](const TriangleBatch &batch) {
        samples += batch.samples;
        if (samples > sampleLimit || (_continue && !_continue()))
            return false;
        for (const auto &triangle : batch.triangles)
            for (int band = triangle.region.top() / rowsPerBand;
                 band <= triangle.region.bottom() / rowsPerBand; ++band)
                bands[band].append(&triangle);
        return true;
    };
    if (_preview && batches.size() > 1) {
        QtConcurrent::blockingMap(&modelPreviewPool(), batches, prepareTriangles);
        for (const auto &batch : std::as_const(batches))
            if (!binTriangles(batch))
                return {};
    } else {
        for (auto &batch : batches) {
            prepareTriangles(batch);
            if (!binTriangles(batch))
                return {};
        }
    }
    const auto &bins       = std::as_const(bands);
    const auto  renderBand = [&](int band) -> QImage {
        const int   top  = band * rowsPerBand;
        const int   rows = qMin(rowsPerBand, height - top);
        const QRect bandArea(0, top, width, rows);
        auto        image = ImageUtils::blank(bandArea.size(), _format);
        if (image.isNull())
            return {};
        QVector<float> depth(qsizetype(width) * rows, 0.f);
        int            drawn = 0;
        for (const auto *entry : bins[band]) {
            if (++drawn % 128 == 0 && _continue && !_continue())
                return {};
            const auto &triangle = *entry;
            const auto [i, j, k] = *triangle.indices;
            const auto *vertices = triangle.vertices;
            const auto &a = vertices[i], &b = vertices[j], &c = vertices[k];
            const auto &primitive = *triangle.primitive;
            const auto &material  = primitive.material;
            const auto *source    = primitive.vertices.constData();
            const auto *colors =
                primitive.vertexColors.isEmpty() ? nullptr : primitive.vertexColors.constData();
            const auto region = triangle.region.intersected(bandArea);
            const auto flat =
                QVector3D::crossProduct(b.position - a.position, c.position - a.position)
                    .normalized();
            const bool textured = !material.texture.isNull();
            for (int y = region.top(); y <= region.bottom(); ++y) {
                auto *pixels = image.scanLine(y - top);
                auto *depths = depth.data() + qsizetype(y - top) * width;
                for (int x = region.left(); x <= region.right(); ++x) {
                    const QPointF center(x + .5, y + .5);
                    float         u = edge(b.pixel, c.pixel, center) / triangle.signedArea;
                    float         v = edge(c.pixel, a.pixel, center) / triangle.signedArea;
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
                        const auto uv = u * source[i].uv + v * source[j].uv + w * source[k].uv;
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
                        colors ? material.color * (u * colors[i] + v * colors[j] + w * colors[k])
                               : material.color;
                    if (material.alphaMask &&
                        qAlpha(texel) / 255.f * base.w() < material.alphaCutoff)
                        continue;
                    const auto  smooth = u * a.normal + v * b.normal + w * c.normal;
                    const auto  normal = (smooth.isNull() ? flat : smooth.normalized()) *
                                         (triangle.front ? 1.f : -1.f);
                    const float diffuse =
                        material.unlit
                            ? 0.f
                            : view.intensity *
                                  qMax(0.f, QVector3D::dotProduct(normal, view.lightDirection));
                    const float ambient = material.unlit ? 1.f : view.ambient;
                    const float red     = sRgb(base.x() * linear(qRed(texel)) *
                                               (ambient + diffuse * view.lightColor.x()));
                    const float green   = sRgb(base.y() * linear(qGreen(texel)) *
                                               (ambient + diffuse * view.lightColor.y()));
                    const float blue    = sRgb(base.z() * linear(qBlue(texel)) *
                                               (ambient + diffuse * view.lightColor.z()));
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
        return image;
    };
    const auto storeBand = [&](int band, const QImage &image) {
        return !image.isNull() &&
               plane.writeRegion(nativeBounds.topLeft() + QPoint(0, band * rowsPerBand), image,
                                 _canvas);
    };
    if (_preview && bands.size() > 1) {
        QVector<int> activeBands;
        for (int band = 0; band < bins.size(); ++band)
            if (!bins[band].isEmpty())
                activeBands.append(band);
        const auto images = QtConcurrent::blockingMapped<QList<QImage>>(&modelPreviewPool(),
                                                                        activeBands, renderBand);
        for (qsizetype i = 0; i < activeBands.size(); ++i)
            if (!storeBand(activeBands.at(i), images.at(i)))
                return {};
    } else {
        for (int band = 0; band < bins.size(); ++band) {
            if (_continue && !_continue())
                return {};
            if (!bins[band].isEmpty() && !storeBand(band, renderBand(band)))
                return {};
        }
    }
    return _continue && !_continue() ? PixelPlane{} : plane;
}
} // namespace

PixelPlane render(const Asset &_asset, const QVariantMap &_parameters, const QSize &_canvas,
                  QImage::Format _format, const Continue &_continue)
{
    return renderCpu(_asset, _parameters, _canvas, _format, _continue, false);
}

PixelPlane renderSoftwarePreview(const Asset &_asset, const QVariantMap &_parameters,
                                 const QSize &_canvas, QImage::Format _format)
{
    return renderCpu(_asset, _parameters, _canvas, _format, {}, true);
}

PixelPlane renderPreview(const std::shared_ptr<const Asset> &_asset, const QVariantMap &_parameters,
                         const QSize &_canvas, QImage::Format _format)
{
#ifdef Q_OS_MACOS
    auto parameters = _parameters;
    if (_format == QImage::Format_ARGB32_Premultiplied && !_canvas.isEmpty() &&
        normalize(parameters)) {
        auto image = renderMetalPreview(_asset, renderView(parameters), _canvas);
        if (!image.isNull())
            return image;
    }
#endif
    return renderSoftwarePreview(*_asset, _parameters, _canvas, _format);
}
} // namespace PhotoCrap::Model3D
