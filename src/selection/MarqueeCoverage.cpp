#include "MarqueeCoverage.h"
#include <QPainter>
#include <new>

namespace
{
SelectionCoverage::Result describeArea(QImage _mask, const QRect &_bounds,
                                       const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (_mask.isNull())
        return {{}, {}, {}, false, "memory"};
    if (_bounds.isEmpty())
        return {{}, {}, {}, true, {}};
    auto result = SelectionCoverage::describe(_mask.copy(_bounds), _cancel);
    if (!result.valid)
        return result;
    result.support.translate(_bounds.topLeft());
    result.outline.translate(_bounds.topLeft());
    result.mask = std::move(_mask);
    return result;
}
SelectionCoverage::Result rasterPath(const QPainterPath &_path, const QSize &_size,
                                     const std::shared_ptr<std::atomic_bool> &_cancel)
{
    const QRect bounds = _path.boundingRect()
                             .toAlignedRect()
                             .adjusted(-1, -1, 1, 1)
                             .intersected(QRect(QPoint{}, _size));
    if (bounds.isEmpty())
        return {{}, {}, {}, true, {}};
    QImage patch(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    QImage mask(_size, QImage::Format_Grayscale8);
    if (patch.isNull() || mask.isNull())
        return {{}, {}, {}, false, "memory"};
    patch.fill(0);
    mask.fill(0);
    QPainter painter(&patch);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(-bounds.topLeft());
    painter.fillPath(_path, Qt::white);
    painter.end();
    for (int y = 0; y < patch.height(); ++y) {
        if (_cancel->load())
            return {};
        const auto line = reinterpret_cast<const QRgb *>(patch.constScanLine(y));
        auto       out  = mask.scanLine(bounds.y() + y) + bounds.x();
        for (int x = 0; x < patch.width(); ++x)
            out[x] = uchar(qAlpha(line[x]));
    }
    return describeArea(std::move(mask), bounds, _cancel);
}
} // namespace
SelectionCoverage::Result MarqueeCoverage::create(const QRegion &_region, const QPainterPath &_path,
                                                  const QSize &_size, bool _antialias, int _feather,
                                                  const QRegion &_previous,
                                                  const QImage &_previousMask, int _mode,
                                                  const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (!_cancel || _cancel->load() || !_size.isValid() || _size.width() > 16384 ||
        _size.height() > 16384 || qint64(_size.width()) * _size.height() > 32LL * 1024 * 1024 ||
        _feather < 0 || _feather > 1000 || _mode < 0 || _mode > 3)
        return {};
    try {
        SelectionCoverage::Result result;
        if (_antialias)
            result = rasterPath(_path, _size, _cancel);
        else
            result = {{}, _region, _region, true, {}};
        if (!result.valid || _cancel->load())
            return result;
        if (_feather && !result.support.isEmpty()) {
            result =
                SelectionCoverage::feather(result.support, result.mask, _size, _feather, _cancel);
            if (!result.valid)
                return result;
        }
        if (_mode == 0)
            return result;
        const auto bounds = result.support.boundingRect()
                                .united(_previous.boundingRect())
                                .intersected(QRect(QPoint{}, _size));
        QImage     mask(_size, QImage::Format_Grayscale8);
        if (mask.isNull())
            return {{}, {}, {}, false, "memory"};
        mask.fill(0);
        for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
            if (_cancel->load())
                return {};
            auto       out  = mask.scanLine(y);
            const auto old  = _previousMask.isNull() ? nullptr : _previousMask.constScanLine(y);
            const auto next = result.mask.isNull() ? nullptr : result.mask.constScanLine(y);
            for (int x = bounds.left(); x <= bounds.right(); ++x) {
                const int a = old ? old[x] : (_previous.contains({x, y}) ? 255 : 0);
                const int b = next ? next[x] : (result.support.contains({x, y}) ? 255 : 0);
                out[x] = uchar(_mode == 1 ? qMax(a, b) : _mode == 2 ? qMax(0, a - b) : qMin(a, b));
            }
        }
        return describeArea(std::move(mask), bounds, _cancel);
    } catch (const std::bad_alloc &) {
        return {{}, {}, {}, false, "memory"};
    }
}
