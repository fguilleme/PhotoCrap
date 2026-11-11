#include "LayerTransform.h"

#include "ImagePixels.h"
#include "ImageUtils.h"
#include "masks/VectorMask.h"
#include "model3d/Model3D.h"
#include "selection/SelectionCoverage.h"
#include "shapes/ShapeLayer.h"
#include "text/TextLayer.h"

#include <QColorSpace>
#include <QLineF>
#include <QPainter>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>

namespace PhotoCrap::Transform
{
namespace
{
constexpr int MaxGridLines = 13;

qreal cross(const QPointF &_a, const QPointF &_b)
{
    return _a.x() * _b.y() - _a.y() * _b.x();
}

bool finite(const QPointF &_point)
{
    return std::isfinite(_point.x()) && std::isfinite(_point.y()) && qAbs(_point.x()) <= 1000000. &&
           qAbs(_point.y()) <= 1000000.;
}

int interval(const QVector<qreal> &_axis, qreal _position)
{
    return qBound(
        0, int(std::upper_bound(_axis.cbegin(), _axis.cend(), _position) - _axis.cbegin()) - 1,
        int(_axis.size()) - 2);
}

QPointF hermite(const QPointF &_a, const QPointF &_b, const QPointF &_da, const QPointF &_db,
                qreal _t)
{
    const qreal t2 = _t * _t, t3 = t2 * _t;
    return _a * (2 * t3 - 3 * t2 + 1) + _da * (t3 - 2 * t2 + _t) + _b * (-2 * t3 + 3 * t2) +
           _db * (t3 - t2);
}

struct Triangle {
    std::array<QPointF, 3> source, target;
};

QVector<Triangle> meshTriangles(const Mapping &_mapping, const QTransform &_matrix)
{
    QVector<Triangle> triangles;
    const auto &      mesh = _mapping.mesh;
    for (int y = 0; y + 1 < mesh.rows.size(); ++y) {
        for (int x = 0; x + 1 < mesh.columns.size(); ++x) {
            constexpr int                                                 divisions = 8;
            std::array<std::array<QPointF, divisions + 1>, divisions + 1> source, target;
            for (int j = 0; j <= divisions; ++j)
                for (int i = 0; i <= divisions; ++i) {
                    const qreal u =
                        std::lerp(mesh.columns[x], mesh.columns[x + 1], i / qreal(divisions));
                    const qreal v = std::lerp(mesh.rows[y], mesh.rows[y + 1], j / qreal(divisions));
                    source[j][i]  = _mapping.sourcePoint({u, v});
                    target[j][i]  = _matrix.map(_mapping.sourcePoint(mesh.sample(u, v)));
                }
            for (int j = 0; j < divisions; ++j)
                for (int i = 0; i < divisions; ++i) {
                    triangles.append(
                        Triangle{{source[j][i], source[j][i + 1], source[j + 1][i + 1]},
                                 {target[j][i], target[j][i + 1], target[j + 1][i + 1]}});
                    triangles.append(
                        Triangle{{source[j][i], source[j + 1][i + 1], source[j + 1][i]},
                                 {target[j][i], target[j + 1][i + 1], target[j + 1][i]}});
                }
        }
    }
    return triangles;
}

template <typename Pixel>
void drawMesh(QImage &_result, const QRect &_bounds, const QImage &_input, const QPoint &_origin,
              const QVector<Triangle> &_triangles)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    for (const auto &triangle : _triangles) {
        const QPointF a = triangle.target[0], ab = triangle.target[1] - a,
                      ac        = triangle.target[2] - a;
        const qreal determinant = cross(ab, ac);
        if (qAbs(determinant) < 1e-10 || !finite(a) || !finite(triangle.target[1]) ||
            !finite(triangle.target[2]))
            continue;
        const QRect region = QPolygonF{a, triangle.target[1], triangle.target[2]}
                                 .boundingRect()
                                 .toAlignedRect()
                                 .intersected(_bounds);
        const QPointF u    = triangle.source[1] - triangle.source[0],
                      v    = triangle.source[2] - triangle.source[0],
                      base = triangle.source[0] - QPointF(_origin) - QPointF(.5, .5);
        for (int y = region.top(); y <= region.bottom(); ++y) {
            auto *out = reinterpret_cast<Pixel *>(_result.scanLine(y - _bounds.y()));
            for (int x = region.left(); x <= region.right(); ++x) {
                const auto  delta = QPointF(x + .5, y + .5) - a;
                const qreal b = cross(delta, ac) / determinant, c = cross(ab, delta) / determinant;
                if (b < -1e-8 || c < -1e-8 || b + c > 1. + 1e-8)
                    continue;
                const auto  position = base + b * u + c * v;
                const auto  sx       = qBound(0., position.x(), qreal(_input.width() - 1));
                const auto  sy       = qBound(0., position.y(), qreal(_input.height() - 1));
                const int   ix = int(sx), iy = int(sy);
                const int   nx    = qMin(ix + 1, _input.width() - 1);
                const auto *upper = reinterpret_cast<const Pixel *>(_input.constScanLine(iy));
                const auto *lower = reinterpret_cast<const Pixel *>(
                    _input.constScanLine(qMin(iy + 1, _input.height() - 1)));
                const auto channel = [&](auto _get) {
                    return std::lerp(std::lerp(_get(upper[ix]), _get(upper[nx]), sx - ix),
                                     std::lerp(_get(lower[ix]), _get(lower[nx]), sx - ix), sy - iy);
                };
                out[x - _bounds.x()] = P::rgba(channel(P::red), channel(P::green), channel(P::blue),
                                               channel(P::alpha));
            }
        }
    }
}

QImage render(const QImage &_input, const QPoint &_origin, const QRect &_bounds,
              const Mapping &_mapping)
{
    QTransform matrix;
    if (!_mapping.matrix(matrix))
        return {};
    auto output = ImageUtils::blank(_bounds.size(), _input.format());
    if (output.isNull())
        return {};
    if (!_mapping.mesh.empty()) {
        const auto triangles = meshTriangles(_mapping, matrix);
        if (ImageUtils::is16Bit(output))
            drawMesh<QRgba64>(output, _bounds, _input, _origin, triangles);
        else
            drawMesh<QRgb>(output, _bounds, _input, _origin, triangles);
    } else {
        QPainter painter(&output);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setTransform(QTransform::fromTranslate(_origin.x(), _origin.y()) * matrix *
                             QTransform::fromTranslate(-_bounds.x(), -_bounds.y()));
        painter.drawImage(QPoint{}, _input);
    }
    output.setColorSpace(_input.colorSpace());
    return output;
}

QImage toNative(const QImage &_image, QImage::Format _format)
{
    if (_format != QImage::Format_Grayscale8)
        return _image.convertToFormat(_format);
    QImage result(_image.size(), _format);
    if (result.isNull())
        return {};
    for (int y = 0; y < result.height(); ++y) {
        const auto *in  = reinterpret_cast<const QRgb *>(_image.constScanLine(y));
        auto *      out = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x)
            out[x] = qRed(in[x]); // Premultiplied intensity, composited over black.
    }
    return result;
}

bool transformPlane(PixelPlane &_plane, const QSize &_size, const Mapping &_mapping,
                    const QImage &_selection, bool _copy)
{
    if (_plane.isNull())
        return false;
    const QRect  canvas(QPoint{}, _size);
    const QRect  source      = _mapping.source.toAlignedRect().intersected(canvas);
    const QRect  destination = _mapping.destination(canvas);
    const auto   native      = _plane.format();
    const auto   format = native == QImage::Format_Grayscale8 ? QImage::Format_ARGB32_Premultiplied
                                                              : ImageUtils::workingFormat(_plane);
    const QImage original = _plane.region(source).convertToFormat(format);
    if (original.isNull())
        return false;
    auto selected = original;
    if (!_selection.isNull())
        SelectionCoverage::attenuate(selected, _selection, source.topLeft());
    PixelPlane result;
    if (!_selection.isNull() && !_copy) {
        result           = _plane;
        auto   remainder = original;
        QImage coverage(source.size(), format);
        if (coverage.isNull())
            return false;
        coverage.fill(Qt::white);
        SelectionCoverage::attenuate(coverage, _selection, source.topLeft());
        QPainter painter(&remainder);
        painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        painter.drawImage(QPoint{}, coverage);
        painter.end();
        if (!result.writeRegion(source.topLeft(), toNative(remainder, native), _size))
            return false;
    } else {
        auto blank = toNative(ImageUtils::blank(QSize(1, 1), format), native);
        if (blank.isNull())
            return false;
        blank.setColorSpace(original.colorSpace());
        result = PixelPlane::uniform(_size, blank);
    }
    if (!destination.isEmpty()) {
        auto transformed = render(selected, source.topLeft(), destination, _mapping);
        if (transformed.isNull())
            return false;
        if (!_selection.isNull() && !_copy) {
            auto existing = result.region(destination).convertToFormat(format);
            if (existing.isNull())
                return false;
            QPainter painter(&existing);
            painter.drawImage(QPoint{}, transformed);
            painter.end();
            transformed = std::move(existing);
        }
        if (!result.writeRegion(destination.topLeft(), toNative(transformed, native), _size))
            return false;
    }
    _plane = std::move(result);
    return true;
}
} // namespace

void Mesh::reset(int _columns, int _rows)
{
    columns.clear();
    rows.clear();
    corners.clear();
    tangents.clear();
    points.clear();
    _columns = qBound(1, _columns, MaxGridLines - 1);
    _rows    = qBound(1, _rows, MaxGridLines - 1);
    for (int i = 0; i <= _columns; ++i)
        columns.append(qreal(i) / _columns);
    for (int j = 0; j <= _rows; ++j)
        rows.append(qreal(j) / _rows);
    for (qreal y : rows)
        for (qreal x : columns)
            points.append({x, y});
}

bool Mesh::hasHandle(int _point, int _direction) const
{
    if (_point < 0 || _point >= points.size() || _direction < 0 || _direction >= 4)
        return false;
    const int x = _point % columns.size(), y = _point / columns.size();
    switch (_direction) {
    case 0:
        return x > 0;
    case 1:
        return x + 1 < columns.size();
    case 2:
        return y > 0;
    default:
        return y + 1 < rows.size();
    }
}

QPointF Mesh::derivative(int _point, int _direction) const
{
    const auto found = tangents.constFind(_point * 4 + _direction);
    if (found != tangents.cend())
        return found.value();
    const bool  vertical   = _direction >= 2;
    const auto &axis       = vertical ? rows : columns;
    const int   stride     = vertical ? int(columns.size()) : 1;
    const int   coordinate = vertical ? _point / columns.size() : _point % columns.size();
    const int   offset     = _point - coordinate * stride;
    const bool  positive   = _direction % 2;
    const bool  corner     = corners.contains(_point);
    const int   before     = qMax(0, coordinate - (corner && positive ? 0 : 1));
    const int   after      = qMin(int(axis.size()) - 1, coordinate + (corner && !positive ? 0 : 1));
    return (points[offset + after * stride] - points[offset + before * stride]) /
           (axis[after] - axis[before]);
}

QPointF Mesh::handlePoint(int _point, int _direction) const
{
    const bool  vertical   = _direction >= 2;
    const auto &axis       = vertical ? rows : columns;
    const int   coordinate = vertical ? _point / columns.size() : _point % columns.size();
    const int   neighbor   = coordinate + (_direction % 2 ? 1 : -1);
    return points[_point] +
           derivative(_point, _direction) * ((axis[neighbor] - axis[coordinate]) / 3.);
}

void Mesh::moveHandle(int _point, int _direction, const QPointF &_position)
{
    if (!hasHandle(_point, _direction))
        return;
    const bool  vertical   = _direction >= 2;
    const auto &axis       = vertical ? rows : columns;
    const int   coordinate = vertical ? _point / columns.size() : _point % columns.size();
    const int   neighbor   = coordinate + (_direction % 2 ? 1 : -1);
    const auto  value = (_position - points[_point]) * (3. / (axis[neighbor] - axis[coordinate]));
    tangents.insert(_point * 4 + _direction, value);
    if (!corners.contains(_point) && hasHandle(_point, _direction ^ 1))
        tangents.insert(_point * 4 + (_direction ^ 1), value);
}

QPointF Mesh::sample(qreal _u, qreal _v) const
{
    if (empty())
        return {_u, _v};
    const int   x = interval(columns, _u), y = interval(rows, _v);
    const qreal width = columns[x + 1] - columns[x], height = rows[y + 1] - rows[y];
    const qreal u = (_u - columns[x]) / width, v = (_v - rows[y]) / height;
    const int   a = y * columns.size() + x, b = a + columns.size();
    const auto  row = [&](int i) {
        return hermite(points[i], points[i + 1], derivative(i, 1) * width,
                       derivative(i + 1, 0) * width, u);
    };
    const auto tangent = [&](int i, int direction) {
        return (derivative(i, direction) * (1. - u) + derivative(i + 1, direction) * u) * height;
    };
    return hermite(row(a), row(b), tangent(a, 3), tangent(b, 2), v);
}

void Mesh::split(const QPointF &_at, bool _horizontal, bool _vertical)
{
    const Mesh previous = *this;
    const auto insert   = [](QVector<qreal> &_axis, qreal _position) {
        if (_axis.size() >= MaxGridLines)
            return;
        const int i = interval(_axis, _position);
        if (_position - _axis[i] >= .015 && _axis[i + 1] - _position >= .015)
            _axis.insert(i + 1, _position);
    };
    if (_horizontal)
        insert(rows, _at.y());
    if (_vertical)
        insert(columns, _at.x());
    points.clear();
    corners.clear();
    tangents.clear();
    for (qreal y : rows)
        for (qreal x : columns) {
            const int node = int(points.size());
            const int oldX = previous.columns.indexOf(x), oldY = previous.rows.indexOf(y);
            if (oldX >= 0 && oldY >= 0 &&
                previous.corners.contains(int(oldY * previous.columns.size() + oldX)))
                corners.insert(node);
            points.append(previous.sample(x, y));
        }
    if (previous.tangents.isEmpty())
        return;
    constexpr qreal epsilon = 1e-5;
    for (int node = 0; node < points.size(); ++node) {
        const qreal x = columns[node % columns.size()], y = rows[node / columns.size()];
        for (int direction = 0; direction < 4; ++direction) {
            if (!hasHandle(node, direction))
                continue;
            const qreal delta = direction % 2 ? epsilon : -epsilon;
            const auto  near =
                direction < 2 ? previous.sample(x + delta, y) : previous.sample(x, y + delta);
            tangents.insert(node * 4 + direction, (near - points[node]) / delta);
        }
    }
}

void Mesh::remove(int _point)
{
    if (_point < 0 || _point >= points.size())
        return;
    const Mesh previous = *this;
    const int  x = _point % columns.size(), y = _point / columns.size();
    if (x > 0 && x < columns.size() - 1)
        columns.removeAt(x);
    if (y > 0 && y < rows.size() - 1)
        rows.removeAt(y);
    points.clear();
    corners.clear();
    tangents.clear();
    for (qreal v : rows)
        for (qreal u : columns) {
            const int index = int(previous.rows.indexOf(v) * previous.columns.size() +
                                  previous.columns.indexOf(u));
            if (previous.corners.contains(index))
                corners.insert(int(points.size()));
            const int node = int(points.size());
            points.append(previous.points[index]);
            for (int direction = 0; direction < 4; ++direction)
                if (previous.tangents.contains(index * 4 + direction))
                    tangents.insert(node * 4 + direction, previous.tangents[index * 4 + direction]);
        }
}

void Mesh::preset(const QString &_style, qreal _bend, bool _vertical)
{
    reset(4, 4);
    const qreal amount = qBound(-100., _bend, 100.) / 200.;
    for (auto &point : points) {
        const qreal u    = _vertical ? point.y() : point.x();
        const qreal v    = _vertical ? point.x() : point.y();
        const qreal arch = 4 * u * (1 - u);
        QPointF     offset;
        if (_style == "arc")
            offset.setY(-amount * arch);
        else if (_style == "arcLower")
            offset.setY(amount * arch * v);
        else if (_style == "arcUpper")
            offset.setY(-amount * arch * (1 - v));
        else if (_style == "arch")
            offset = {amount * (u - .5) * (v - .5), -amount * arch};
        else if (_style == "bulge")
            offset.setY(amount * arch * (2 * v - 1));
        else if (_style == "flag")
            offset.setY(amount * std::sin(2 * M_PI * u));
        else if (_style == "wave")
            offset.setY(amount * std::sin(2 * M_PI * u) * (1 - .5 * v));
        point += _vertical ? QPointF(offset.y(), offset.x()) : offset;
    }
}

QPolygonF corners(const QRectF &_rect)
{
    return {_rect.topLeft(), _rect.topRight(), _rect.bottomRight(), _rect.bottomLeft()};
}

QPointF handle(const QPolygonF &_quad, int _index)
{
    if (_index < 4)
        return _quad[_index];
    const int edge = _index - 4;
    return (_quad[edge] + _quad[(edge + 1) % 4]) / 2.;
}

bool Mapping::matrix(QTransform &_matrix) const
{
    if (source.isEmpty() || quad.size() != 4)
        return false;
    qreal orientation = 0.;
    for (int i = 0; i < 4; ++i) {
        if (!finite(quad[i]))
            return false;
        const qreal side =
            cross(quad[(i + 1) % 4] - quad[i], quad[(i + 2) % 4] - quad[(i + 1) % 4]);
        if (qAbs(side) < 1e-5 || (i && orientation * side <= 0.))
            return false;
        orientation = side;
    }
    return QTransform::quadToQuad(corners(source), quad, _matrix) && _matrix.isInvertible();
}

Mapping Mapping::scaled(const QSizeF &_scale) const
{
    const auto transform = QTransform::fromScale(_scale.width(), _scale.height());
    auto       copy      = *this;
    copy.source          = transform.mapRect(source);
    copy.quad            = transform.map(quad);
    return copy;
}

QPointF Mapping::sourcePoint(const QPointF &_normalized) const
{
    return source.topLeft() +
           QPointF(_normalized.x() * source.width(), _normalized.y() * source.height());
}

QPointF Mapping::warpPoint(const QPointF &_normalized) const
{
    QTransform transform;
    return matrix(transform) ? transform.map(sourcePoint(_normalized)) : QPointF{};
}

QRect Mapping::destination(const QRect &_canvas) const
{
    if (mesh.empty())
        return quad.boundingRect().adjusted(-1, -1, 1, 1).toAlignedRect().intersected(_canvas);
    QTransform transform;
    if (!matrix(transform))
        return {};
    QRectF bounds;
    for (const auto &triangle : meshTriangles(*this, transform)) {
        if (std::all_of(triangle.target.begin(), triangle.target.end(), finite))
            bounds =
                bounds.united(QPolygonF{triangle.target[0], triangle.target[1], triangle.target[2]}
                                  .boundingRect());
    }
    return bounds.intersected(QRectF(_canvas)).toAlignedRect();
}

bool Mapping::unchanged() const
{
    if (quad != corners(source) || !mesh.tangents.isEmpty())
        return false;
    for (int y = 0; y < mesh.rows.size(); ++y)
        for (int x = 0; x < mesh.columns.size(); ++x)
            if (mesh.points[y * mesh.columns.size() + x] != QPointF(mesh.columns[x], mesh.rows[y]))
                return false;
    return true;
}

QPolygonF dragQuad(const QPolygonF &_quad, const QPointF &_start, const QPointF &_point,
                   const QPointF &_pivot, int _handle, const QString &_mode, bool _linked,
                   bool _shift, bool _alternate)
{
    QPolygonF     result = _quad;
    const QPointF delta  = _point - _start;
    if (_mode == "move") {
        QPointF offset = delta;
        if (_shift) {
            if (qAbs(offset.x()) > qAbs(offset.y()))
                offset.setY(0.);
            else
                offset.setX(0.);
        }
        return result.translated(offset);
    }
    if (_mode == "rotate") {
        const auto a = _start - _pivot, b = _point - _pivot;
        qreal      angle = qRadiansToDegrees(std::atan2(cross(a, b), QPointF::dotProduct(a, b)));
        if (_shift)
            angle = qRound(angle / 15.) * 15.;
        QTransform turn;
        turn.translate(_pivot.x(), _pivot.y()).rotate(angle).translate(-_pivot.x(), -_pivot.y());
        return turn.map(result);
    }
    if (_handle < 0 || _handle >= 8)
        return result;
    const bool corner = _handle < 4;
    const int  edge   = _handle - 4;
    if (_mode == "distort") {
        if (corner)
            result[_handle] += delta;
        else {
            result[edge] += delta;
            result[(edge + 1) % 4] += delta;
        }
        return result;
    }
    if (_mode == "perspective") {
        if (!corner)
            return result;
        const int  horizontal = _handle ^ 1, vertical = 3 - _handle;
        const auto h       = _quad[horizontal] - _quad[_handle];
        const auto v       = _quad[vertical] - _quad[_handle];
        const auto project = [&](const QPointF &_axis) {
            return _axis * (QPointF::dotProduct(delta, _axis) / QPointF::dotProduct(_axis, _axis));
        };
        const auto dx = project(h), dy = project(v);
        const bool alongX = QPointF::dotProduct(dx, dx) >= QPointF::dotProduct(dy, dy);
        result[_handle] += alongX ? dx : dy;
        result[alongX ? horizontal : vertical] -= alongX ? dx : dy;
        return result;
    }
    const auto  origin = _quad[0], x = _quad[1] - origin, y = _quad[3] - origin;
    const qreal determinant = cross(x, y);
    if (qAbs(determinant) < 1e-10)
        return result;
    const auto local = [&](const QPointF &_p) {
        const auto d = _p - origin;
        return QPointF(cross(d, y) / determinant, cross(x, d) / determinant);
    };
    const auto world = [&](const QPointF &_p) { return origin + x * _p.x() + y * _p.y(); };
    if (_mode == "skew") {
        if (corner)
            return result;
        const auto axis = _quad[(edge + 1) % 4] - _quad[edge];
        const auto offset =
            axis * (QPointF::dotProduct(delta, axis) / QPointF::dotProduct(axis, axis));
        // Affine shear: shift one edge while the opposite edge stays in place.
        result[edge] += offset;
        result[(edge + 1) % 4] += offset;
        return result;
    }
    const int  opposite    = corner ? (_handle + 2) % 4 : 4 + (edge + 2) % 4;
    const auto fixed       = local(_alternate ? _pivot : handle(_quad, opposite));
    const auto dragged     = local(handle(_quad, _handle));
    const auto next        = local(handle(_quad, _handle) + delta);
    const auto denominator = dragged - fixed;
    qreal      sx = qAbs(denominator.x()) > 1e-8 ? (next.x() - fixed.x()) / denominator.x() : 1.;
    qreal      sy = qAbs(denominator.y()) > 1e-8 ? (next.y() - fixed.y()) / denominator.y() : 1.;
    if (!corner) {
        if (edge % 2 == 0)
            sx = 1.;
        else
            sy = 1.;
    } else if (_linked != _shift) {
        const auto  diagonal = handle(_quad, _handle) - world(fixed);
        const qreal length   = QPointF::dotProduct(diagonal, diagonal);
        if (length > 1e-8)
            sx = sy = 1. + QPointF::dotProduct(delta, diagonal) / length;
    }
    for (auto &p : result) {
        const auto d = local(p) - fixed;
        p            = world(fixed + QPointF(d.x() * sx, d.y() * sy));
    }
    return result;
}

QRect contentBounds(const PixelPlane &_plane, const QSize &_size)
{
    if (_plane.isNull())
        return {};
    if (_plane.knownOpaque() && _plane.format() != QImage::Format_Grayscale8)
        return QRect(QPoint{}, _size);
    const bool gray     = _plane.format() == QImage::Format_Grayscale8;
    const auto occupied = [gray](const QColor &_color) {
        return gray ? _color.red() != 0 : _color.alphaF() > 0.;
    };
    if (_plane.isUniform())
        return occupied(_plane.uniformPixel().pixelColor(0, 0)) ? QRect(QPoint{}, _size) : QRect{};
    if (const auto background = _plane.tiles().backgroundPixel();
        !background.isNull() && occupied(background.pixelColor(0, 0)))
        return QRect(QPoint{}, _size);
    constexpr int side = TiledImage::TileSize;
    QRect         bounds;
    for (int y = 0; y < _size.height(); y += side) {
        for (int x = 0; x < _size.width(); x += side) {
            const auto tile = _plane.tileImage(x / side, y / side);
            if (tile.isNull())
                continue;
            if (tile.size() == QSize(1, 1)) {
                if (occupied(tile.pixelColor(0, 0)))
                    bounds = bounds.united(
                        QRect(x, y, qMin(side, _size.width() - x), qMin(side, _size.height() - y)));
                continue;
            }
            const bool wide = ImageUtils::is16Bit(tile);
            for (int row = 0; row < tile.height(); ++row) {
                const auto *line    = tile.constScanLine(row);
                const auto  present = [&](int _column) {
                    if (gray)
                        return line[_column] != 0;
                    if (wide)
                        return reinterpret_cast<const QRgba64 *>(line)[_column].alpha() != 0;
                    return qAlpha(reinterpret_cast<const QRgb *>(line)[_column]) != 0;
                };
                int left = 0, right = tile.width() - 1;
                while (left <= right && !present(left))
                    ++left;
                if (left > right)
                    continue;
                while (right > left && !present(right))
                    --right;
                bounds = bounds.united(QRect(x + left, y + row, right - left + 1, 1));
            }
        }
    }
    return bounds;
}

bool applyLayers(QVector<Layer> &_layers, const QSize &_size, const QSize &_nativeSize,
                 const Job &_job, bool _preview, const std::function<bool()> &_continue)
{
    QTransform transform;
    if (!_job.mapping.matrix(transform))
        return false;
    const auto   mapping = _size == _nativeSize
                               ? _job.mapping
                               : _job.mapping.scaled({qreal(_size.width()) / _nativeSize.width(),
                                                    qreal(_size.height()) / _nativeSize.height()});
    const QImage selection =
        _job.selection.isNull() || _size == _nativeSize
            ? _job.selection
            : _job.selection.scaled(_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QVector<QPair<int, Layer>> copies;
    for (int index : _job.layers) {
        if ((_continue && !_continue()) || index < 0 || index >= _layers.size())
            return false;
        Layer layer = _layers[index];
        if (_preview && !layer.visible && !_job.maskView)
            continue;
        const bool wholeLayer = selection.isNull() && !_job.mask && !_job.vectorMask;
        if ((wholeLayer || _job.vectorMask || _job.copy) && Masks::hasVector(layer) &&
            !Masks::transformVector(layer, transform, _nativeSize))
            return false;
        if (_job.vectorMask) {
            _layers[index] = std::move(layer);
            continue;
        }
        if (!_preview && wholeLayer && layer.model3D &&
            !Model3D::transformPlacement(layer.parameters, transform))
            return false;
        if (_job.mask) {
            if (!transformPlane(layer.mask, _size, mapping, selection, false))
                return false;
        } else {
            if (!_preview && wholeLayer && (layer.kind == "text" || layer.kind == "shape")) {
                if (layer.kind == "text") {
                    Text::transform(layer.parameters, transform);
                    layer.image = Text::renderPlane(layer.parameters, _size, layer.image.format());
                } else {
                    if (!Shape::transform(layer.parameters, transform))
                        return false;
                    layer.image = Shape::renderPlane(layer.parameters, _size, layer.image.format());
                }
                if (layer.image.isNull())
                    return false;
            } else if (!layer.image.isNull() &&
                       !transformPlane(layer.image, _size, mapping, selection, _job.copy))
                return false;
            if ((wholeLayer || _job.copy) && !layer.mask.isNull() && !layer.mask.isUniform() &&
                !transformPlane(layer.mask, _size, mapping, {}, false))
                return false;
            layer.aiSource.reset();
        }
        if (_job.copy) {
            layer.historyId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            copies.append({index, std::move(layer)});
        } else {
            _layers[index] = std::move(layer);
        }
    }
    for (auto it = copies.rbegin(); it != copies.rend(); ++it)
        _layers.insert(it->first + 1, std::move(it->second));
    return true;
}

QImage transformSelection(const QImage &_selection, const Mapping &_mapping)
{
    if (_selection.isNull())
        return {};
    PixelPlane plane(_selection);
    if (!transformPlane(plane, _selection.size(), _mapping, {}, false))
        return {};
    return plane.region(_selection.rect());
}
} // namespace PhotoCrap::Transform
