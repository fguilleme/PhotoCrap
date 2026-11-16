#include "Model3DPrivate.h"

#include <QColor>
#include <QtEndian>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace PhotoCrap::Model3D
{
namespace
{
constexpr float MaxRadius = .75f;
constexpr int   CellCount = PaintGridEdge * PaintGridEdge * PaintGridEdge;
constexpr int   DabBytes  = 12 * 4;

int cellCoordinate(float position)
{
    return qBound(0, int((position + .5f) * PaintGridEdge), PaintGridEdge - 1);
}

int cellIndex(int x, int y, int z)
{
    return (z * PaintGridEdge + y) * PaintGridEdge + x;
}

void visitCells(const PaintDab &dab, auto visit)
{
    const auto p = dab.centerRadius;
    const auto r = p.w();
    for (int z = cellCoordinate(p.z() - r); z <= cellCoordinate(p.z() + r); ++z)
        for (int y = cellCoordinate(p.y() - r); y <= cellCoordinate(p.y() + r); ++y)
            for (int x = cellCoordinate(p.x() - r); x <= cellCoordinate(p.x() + r); ++x)
                visit(cellIndex(x, y, z));
}

struct Ray {
    QVector3D origin, direction;
};

struct Box {
    QVector3D lower{1, 1, 1}, upper{-1, -1, -1};

    void include(const QVector3D &p)
    {
        for (int axis = 0; axis < 3; ++axis) {
            lower[axis] = qMin(lower[axis], p[axis]);
            upper[axis] = qMax(upper[axis], p[axis]);
        }
    }

    bool intersects(const Ray &ray, float limit) const
    {
        float near = 0.f, far = limit;
        for (int axis = 0; axis < 3; ++axis) {
            const float origin = ray.origin[axis], slope = ray.direction[axis];
            if (std::abs(slope) < 1e-8f) {
                if (origin < lower[axis] - 1e-6f || origin > upper[axis] + 1e-6f)
                    return false;
                continue;
            }
            const float a = (lower[axis] - origin - 1e-6f) / slope;
            const float b = (upper[axis] - origin + 1e-6f) / slope;
            near          = qMax(near, qMin(a, b));
            far           = qMin(far, qMax(a, b));
            if (near > far)
                return false;
        }
        return true;
    }
};

struct TriangleBatch {
    const Primitive *primitive;
    const Face      *faces;
    int              count;
    Box              box;
};

struct Hit {
    float     distance = std::numeric_limits<float>::infinity();
    QVector3D normal;
};

void intersect(const TriangleBatch &batch, const Ray &ray, Hit &hit)
{
    if (!batch.box.intersects(ray, hit.distance))
        return;
    const auto &primitive = *batch.primitive;
    for (int i = 0; i < batch.count; ++i) {
        const auto [ia, ib, ic] = batch.faces[i];
        const auto &a = primitive.vertices[ia], &b = primitive.vertices[ib],
                   &c  = primitive.vertices[ic];
        const auto  ab = b.position - a.position, ac = c.position - a.position;
        const auto  p   = QVector3D::crossProduct(ray.direction, ac);
        const float det = QVector3D::dotProduct(ab, p);
        if (std::abs(det) < 1e-10f || (det < 0.f && !primitive.material.doubleSided))
            continue;
        const auto  relative = ray.origin - a.position;
        const float u        = QVector3D::dotProduct(relative, p) / det;
        if (u < -1e-6f || u > 1.f + 1e-6f)
            continue;
        const auto  q = QVector3D::crossProduct(relative, ab);
        const float v = QVector3D::dotProduct(ray.direction, q) / det;
        if (v < -1e-6f || u + v > 1.f + 1e-6f)
            continue;
        const float distance = QVector3D::dotProduct(ac, q) / det;
        if (distance <= 0.f || distance >= hit.distance)
            continue;
        const auto &material = primitive.material;
        if (material.alphaMask) {
            const float w     = 1.f - u - v;
            const auto  texel = materialTexel(material, w * a.uv + u * b.uv + v * c.uv);
            float       alpha = material.color.w() * qAlpha(texel) / 255.f;
            if (!primitive.vertexColors.isEmpty()) {
                const auto &colors = primitive.vertexColors;
                alpha *= w * colors[ia].w() + u * colors[ib].w() + v * colors[ic].w();
            }
            if (alpha < material.alphaCutoff)
                continue;
        }
        // QVector3D::normalized treats small, valid triangles as zero.
        const auto  normal = QVector3D::crossProduct(ab, ac);
        const float length = normal.length();
        if (length > 0.f) {
            hit.distance = distance;
            hit.normal   = normal * ((det < 0.f ? -1.f : 1.f) / length);
        }
    }
}
} // namespace

struct PaintPicking {
    struct Node {
        Box box;
        int first, count, right;
    };

    QList<TriangleBatch> batches;
    QList<Node>          nodes;

    int build(int first, int count)
    {
        Box bounds;
        for (int i = first; i < first + count; ++i) {
            bounds.include(batches.at(i).box.lower);
            bounds.include(batches.at(i).box.upper);
        }
        const int index = int(nodes.size());
        nodes.append({bounds, first, count, -1});
        if (count <= 4)
            return index;
        const auto extent = bounds.upper - bounds.lower;
        int        axis   = extent.x() > extent.y() ? 0 : 1;
        if (extent.z() > extent[axis])
            axis = 2;
        auto      start  = batches.begin() + first;
        const int middle = count / 2;
        std::nth_element(start, start + middle, start + count,
                         [axis](const auto &a, const auto &b) {
                             return a.box.lower[axis] + a.box.upper[axis] <
                                    b.box.lower[axis] + b.box.upper[axis];
                         });
        build(first, middle);
        const int right    = build(first + middle, count - middle);
        nodes[index].right = right;
        return index;
    }

    void cast(int index, const Ray &ray, Hit &hit) const
    {
        const auto &node = nodes.at(index);
        if (!node.box.intersects(ray, hit.distance))
            return;
        if (node.right >= 0) {
            cast(index + 1, ray, hit);
            cast(node.right, ray, hit);
        } else {
            for (int i = node.first; i < node.first + node.count; ++i)
                intersect(batches.at(i), ray, hit);
        }
    }
};

std::shared_ptr<const PaintPicking> paintPicking(const Asset &asset)
{
    auto                picking   = std::make_shared<PaintPicking>();
    constexpr qsizetype BatchSize = 32;
    for (const auto &primitive : asset.primitives)
        for (qsizetype first = 0; first < primitive.triangles.size(); first += BatchSize) {
            auto &batch = picking->batches.emplaceBack(
                &primitive, primitive.triangles.constData() + first,
                int(qMin(BatchSize, primitive.triangles.size() - first)), Box{});
            for (int i = 0; i < batch.count; ++i)
                for (const auto vertex : batch.faces[i])
                    batch.box.include(primitive.vertices.at(vertex).position);
        }
    if (!picking->batches.isEmpty())
        picking->build(0, int(picking->batches.size()));
    return picking;
}

bool paintDab(const PaintPicking &picking, const QVariantMap &parameters, const QPointF &point,
              qreal radius, qreal hardness, const QColor &color, qreal opacity, PaintDab &dab)
{
    if (picking.nodes.isEmpty())
        return false;
    const auto view     = renderView(parameters);
    const auto inv      = view.placement.inverted().map(point) / (view.scale * CameraDistance);
    const auto rotation = view.rotation.conjugated();
    const Ray  ray{rotation.rotatedVector({0, 0, CameraDistance}),
                   rotation.rotatedVector({float(inv.x()), -float(inv.y()), -1.f})};
    Hit        hit;
    picking.cast(0, ray, hit);
    if (!std::isfinite(hit.distance))
        return false;
    const float pixelsPerUnit = float(std::sqrt(std::abs(view.placement.determinant()))) *
                                (view.scale * CameraDistance / hit.distance);
    dab = {QVector4D(ray.origin + hit.distance * ray.direction,
                     qBound(1e-6f, float(radius) / pixelsPerUnit, MaxRadius)),
           QVector4D(hit.normal, float(hardness)),
           {linear(color.red()), linear(color.green()), linear(color.blue()), float(opacity)}};
    return true;
}

PaintGrid::PaintGrid(const Paint &paint)
{
    if (paint.isEmpty())
        return;
    offsets.resize(CellCount + 1);
    for (const auto &dab : paint)
        visitCells(dab, [&](int cell) { ++offsets[cell + 1]; });
    for (int i = 1; i <= CellCount; ++i)
        offsets[i] += offsets[i - 1];
    indices.resize(offsets.constLast());
    auto next = offsets;
    for (quint32 i = 0; i < quint32(paint.size()); ++i)
        visitCells(paint.at(i), [&](int cell) { indices[next[cell]++] = i; });
}

QVector3D paintedColor(const Paint &paint, const PaintGrid &grid, const QVector3D &position,
                       const QVector3D &normal, QVector3D color)
{
    const int cell = cellIndex(cellCoordinate(position.x()), cellCoordinate(position.y()),
                               cellCoordinate(position.z()));
    for (auto i = grid.offsets.at(cell); i < grid.offsets.at(cell + 1); ++i) {
        const auto &dab         = paint.at(grid.indices.at(i));
        const auto  delta       = (position - dab.centerRadius.toVector3D()) / dab.centerRadius.w();
        const auto  brushNormal = dab.normalHardness.toVector3D();
        const float depth       = QVector3D::dotProduct(delta, brushNormal);
        const float distance    = std::sqrt(delta.lengthSquared() + 12.f * depth * depth);
        if (distance >= 1.f || QVector3D::dotProduct(normal, brushNormal) < .30f)
            continue;
        const float alpha = dab.colorOpacity.w() *
                            qMin(1.f, (1.f - distance) / qMax(.015f, 1.f - dab.normalHardness.w()));
        color += alpha * (dab.colorOpacity.toVector3D() - color);
    }
    return color;
}

QByteArray encodePaint(const Paint &paint)
{
    QByteArray bytes(paint.size() * DabBytes, Qt::Uninitialized);
    auto      *out = reinterpret_cast<uchar *>(bytes.data());
    for (const auto &dab : paint)
        for (const auto &vector : {dab.centerRadius, dab.normalHardness, dab.colorOpacity})
            for (int axis = 0; axis < 4; ++axis, out += 4)
                qToLittleEndian(std::bit_cast<quint32>(vector[axis]), out);
    return bytes;
}

bool decodePaint(const QByteArray &bytes, Paint &paint)
{
    if (bytes.isEmpty() || bytes.size() > MaxPaintBytes || bytes.size() % DabBytes)
        return false;
    const auto *in = reinterpret_cast<const uchar *>(bytes.constData());
    Paint       decoded;
    decoded.reserve(bytes.size() / DabBytes);
    for (qsizetype i = 0; i < bytes.size() / DabBytes; ++i) {
        PaintDab dab;
        for (auto *vector : {&dab.centerRadius, &dab.normalHardness, &dab.colorOpacity})
            for (int axis = 0; axis < 4; ++axis, in += 4) {
                (*vector)[axis] = std::bit_cast<float>(qFromLittleEndian<quint32>(in));
                if (!std::isfinite((*vector)[axis]))
                    return false;
            }
        for (int axis = 0; axis < 3; ++axis)
            if (std::abs(dab.centerRadius[axis]) > .501f)
                return false;
        const float normalLength = dab.normalHardness.toVector3D().lengthSquared();
        if (dab.centerRadius.w() < 1e-6f || dab.centerRadius.w() > MaxRadius ||
            normalLength < .99f || normalLength > 1.01f || dab.normalHardness.w() < 0.f ||
            dab.normalHardness.w() > 1.f)
            return false;
        for (int axis = 0; axis < 4; ++axis)
            if (dab.colorOpacity[axis] < 0.f || dab.colorOpacity[axis] > 1.f)
                return false;
        decoded.append(dab);
    }
    paint = std::move(decoded);
    return true;
}
} // namespace PhotoCrap::Model3D
