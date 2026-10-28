#include "SelectionGeometry.h"
#include <QImage>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace
{
constexpr int Infinity = 1000000000;
constexpr int MaxRects = 1000000;

// Squared Euclidean distance transform: lower envelope of parabolas, O(n).
// Only one full distance plane is retained, plus scanline-sized work buffers.
void transformLine(const std::vector<int> &_input, std::vector<int> &_output, int _length,
                   std::vector<int> &_sites, std::vector<double> &_limits)
{
    int k      = 0;
    _sites[0]  = 0;
    _limits[0] = -std::numeric_limits<double>::infinity();
    _limits[1] = std::numeric_limits<double>::infinity();
    for (int q = 1; q < _length; ++q) {
        double s;
        do {
            const int p = _sites[k];
            s = (double(_input[q]) + double(q) * q - _input[p] - double(p) * p) / (2. * (q - p));
            if (s > _limits[k])
                break;
            --k;
        } while (k >= 0);
        ++k;
        _sites[k]      = q;
        _limits[k]     = s;
        _limits[k + 1] = std::numeric_limits<double>::infinity();
    }
    k = 0;
    for (int q = 0; q < _length; ++q) {
        while (_limits[k + 1] < q)
            ++k;
        const int delta = q - _sites[k];
        _output[q]      = std::min(Infinity, delta * delta + _input[_sites[k]]);
    }
}

std::vector<int> distanceTo(const QImage &_mask, bool _selected,
                            const std::shared_ptr<std::atomic_bool> &_cancel)
{
    const int width = _mask.width(), height = _mask.height(), length = std::max(width, height);
    std::vector<int>    distances(size_t(width) * height);
    std::vector<int>    input(length), output(length), sites(length);
    std::vector<double> limits(length + 1);
    for (int y = 0; y < height; ++y) {
        if (_cancel->load())
            return {};
        const auto *row = _mask.constScanLine(y);
        for (int x = 0; x < width; ++x)
            input[x] = (row[x] != 0) == _selected ? 0 : Infinity;
        transformLine(input, output, width, sites, limits);
        std::copy_n(output.begin(), width, distances.begin() + size_t(y) * width);
    }
    for (int x = 0; x < width; ++x) {
        if (_cancel->load())
            return {};
        for (int y = 0; y < height; ++y)
            input[y] = distances[size_t(y) * width + x];
        transformLine(input, output, height, sites, limits);
        for (int y = 0; y < height; ++y)
            distances[size_t(y) * width + x] = output[y];
    }
    return distances;
}

template <class Predicate>
SelectionGeometry::Result makeRegion(const QRect &_bounds, const Predicate &_matches,
                                     const std::shared_ptr<std::atomic_bool> &_cancel)
{
    QVector<QRect> rows, current;
    qsizetype      previousStart = 0, previousCount = 0;
    for (int y = 0; y < _bounds.height(); ++y) {
        if (_cancel->load())
            return {};
        current.clear();
        int x = 0;
        while (x < _bounds.width()) {
            while (x < _bounds.width() && !_matches(x, y))
                ++x;
            const int left = x;
            while (x < _bounds.width() && _matches(x, y))
                ++x;
            if (left < x) {
                if (rows.size() + current.size() >= MaxRects)
                    return {{}, false, "complexity"};
                current.append(QRect(_bounds.x() + left, _bounds.y() + y, x - left, 1));
            }
        }
        // QRegion requires canonical bands: vertically merge identical scanlines.
        bool same = !current.isEmpty() && current.size() == previousCount;
        for (qsizetype i = 0; same && i < current.size(); ++i)
            same = rows[previousStart + i].left() == current[i].left() &&
                   rows[previousStart + i].width() == current[i].width() &&
                   rows[previousStart + i].bottom() + 1 == current[i].top();
        if (same) {
            for (qsizetype i = 0; i < previousCount; ++i)
                rows[previousStart + i].setBottom(_bounds.y() + y);
        } else {
            previousStart = rows.size();
            previousCount = current.size();
            rows += current;
        }
    }
    QRegion region;
    // Bands are disjoint and sorted by scanline, then x, as setRects requires.
    if (!rows.isEmpty())
        region.setRects(rows.constData(), int(rows.size()));
    return {region, true, {}};
}

SelectionGeometry::Result radial(const QImage &_mask, const QRect &_bounds, int _radius,
                                 bool _expand, const std::shared_ptr<std::atomic_bool> &_cancel)
{
    const auto distances = distanceTo(_mask, _expand, _cancel);
    if (_cancel->load())
        return {};
    const int squaredRadius = _radius * _radius;
    return makeRegion(
        _bounds,
        [&](int _x, int _y) {
            const int distance = distances[size_t(_y) * _mask.width() + _x];
            return _expand ? distance <= squaredRadius : distance > squaredRadius;
        },
        _cancel);
}

SelectionGeometry::Result smooth(const QImage &_mask, const QRect &_bounds, const QSize &_size,
                                 int _radius, const std::shared_ptr<std::atomic_bool> &_cancel)
{
    // A rolling two-dimensional majority window, O(area) regardless of radius.
    // Areas beyond the ROI are known to be unselected; canvas edges are clipped.
    std::vector<int> columns(_mask.width(), 0);
    for (int y = 0; y <= std::min(_radius, _mask.height() - 1); ++y)
        for (int x = 0; x < _mask.width(); ++x)
            columns[x] += _mask.constScanLine(y)[x] != 0;
    int preparedRow = -1, previousX = -1, sum = 0;
    return makeRegion(
        _bounds,
        [&](int _x, int _y) {
            if (_y != preparedRow) {
                if (_y > 0) {
                    const int remove = _y - _radius - 1, add = _y + _radius;
                    for (int x = 0; x < _mask.width(); ++x) {
                        if (remove >= 0)
                            columns[x] -= _mask.constScanLine(remove)[x] != 0;
                        if (add < _mask.height())
                            columns[x] += _mask.constScanLine(add)[x] != 0;
                    }
                }
                preparedRow = _y;
                previousX   = -1;
                sum         = 0;
            }
            if (_x != previousX) {
                if (_x == 0) {
                    for (int x = 0; x <= std::min(_radius, _mask.width() - 1); ++x)
                        sum += columns[x];
                } else {
                    if (_x - _radius - 1 >= 0)
                        sum -= columns[_x - _radius - 1];
                    if (_x + _radius < _mask.width())
                        sum += columns[_x + _radius];
                }
                previousX = _x;
            }
            const int x = _bounds.x() + _x, y = _bounds.y() + _y;
            const int area =
                (std::min(_size.width() - 1, x + _radius) - std::max(0, x - _radius) + 1) *
                (std::min(_size.height() - 1, y + _radius) - std::max(0, y - _radius) + 1);
            return 2 * sum > area || (2 * sum == area && _mask.constScanLine(_y)[_x]);
        },
        _cancel);
}
} // namespace

SelectionGeometry::Result
SelectionGeometry::modify(const QRegion &_selection, const QSize &_size, const QString &_operation,
                          int _amount, const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (!_cancel || _selection.isEmpty() || !_size.isValid() || _size.width() > 16384 ||
        _size.height() > 16384 || qint64(_size.width()) * _size.height() > 32LL * 1024 * 1024 ||
        _amount < 1 || _amount > (_operation == "border" ? 200 : 100) ||
        (_operation != "expand" && _operation != "contract" && _operation != "border" &&
         _operation != "smooth"))
        return {{}, false, "parameters"};
    if (_cancel->load())
        return {};
    try {
        const int   radius = _operation == "border" ? (_amount + 1) / 2 : _amount;
        const QRect bounds = _selection.boundingRect()
                                 .adjusted(-radius, -radius, radius, radius)
                                 .intersected(QRect(QPoint{}, _size));
        if (bounds.isEmpty())
            return {{}, false, "parameters"};
        QImage mask(bounds.size(), QImage::Format_Grayscale8);
        if (mask.isNull())
            return {{}, false, "memory"};
        mask.fill(0);
        for (const QRect &rect : _selection) {
            const auto local = rect.intersected(bounds).translated(-bounds.topLeft());
            for (int y = local.top(); y <= local.bottom(); ++y) {
                if (_cancel->load())
                    return {};
                std::memset(mask.scanLine(y) + local.left(), 255, size_t(local.width()));
            }
        }
        if (_operation == "smooth")
            return smooth(mask, bounds, _size, _amount, _cancel);
        if (_operation == "border") {
            auto outer = radial(mask, bounds, _amount / 2, true, _cancel);
            if (!outer.valid)
                return outer;
            auto inner = radial(mask, bounds, (_amount + 1) / 2, false, _cancel);
            if (!inner.valid)
                return inner;
            return {outer.region.subtracted(inner.region), true, {}};
        }
        return radial(mask, bounds, _amount, _operation == "expand", _cancel);
    } catch (const std::bad_alloc &) {
        return {{}, false, "memory"};
    }
}
