#include "Document.h"

#include <QBitArray>
#include <QLineF>
#include <QtMath>

namespace
{
bool similarPixel(QRgb _pixel, QRgb _reference, int _tolerance)
{
    return qAbs(qRed(_pixel) - qRed(_reference)) <= _tolerance &&
           qAbs(qGreen(_pixel) - qGreen(_reference)) <= _tolerance &&
           qAbs(qBlue(_pixel) - qBlue(_reference)) <= _tolerance &&
           qAbs(qAlpha(_pixel) - qAlpha(_reference)) <= _tolerance;
}

QRegion colorRegion(const QImage &_input, const QPoint &_seed, int _tolerance, bool _contiguous,
                    const QRect &_limit)
{
    if (_input.isNull() || !_input.rect().contains(_seed))
        return {};
    const QImage image = _input.convertToFormat(QImage::Format_ARGB32);
    const QRect  limit = _limit.intersected(image.rect());
    if (!limit.contains(_seed))
        return {};
    const QRgb reference =
        reinterpret_cast<const QRgb *>(image.constScanLine(_seed.y()))[_seed.x()];
    const auto matches = [&](int _x, int _y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(_y));
        return similarPixel(line[_x], reference, _tolerance);
    };

    QRegion result;
    if (!_contiguous) {
        for (int y = limit.top(); y <= limit.bottom(); ++y) {
            int x = limit.left();
            while (x <= limit.right()) {
                while (x <= limit.right() && !matches(x, y))
                    ++x;
                const int start = x;
                while (x <= limit.right() && matches(x, y))
                    ++x;
                if (start < x)
                    result |= QRect(start, y, x - start, 1);
            }
        }
        return result;
    }

    const int       width = image.width();
    QBitArray       visited(image.width() * image.height());
    QVector<QPoint> pending{_seed};
    while (!pending.isEmpty()) {
        const QPoint point      = pending.takeLast();
        const int    pointIndex = point.y() * width + point.x();
        if (visited.testBit(pointIndex) || !matches(point.x(), point.y()))
            continue;
        int left  = point.x();
        int right = point.x();
        while (left > limit.left() && !visited.testBit(point.y() * width + left - 1) &&
               matches(left - 1, point.y()))
            --left;
        while (right < limit.right() && !visited.testBit(point.y() * width + right + 1) &&
               matches(right + 1, point.y()))
            ++right;
        for (int x = left; x <= right; ++x)
            visited.setBit(point.y() * width + x);
        result |= QRect(left, point.y(), right - left + 1, 1);

        for (const int adjacentY : {point.y() - 1, point.y() + 1}) {
            if (adjacentY < limit.top() || adjacentY > limit.bottom())
                continue;
            int x = left;
            while (x <= right) {
                while (x <= right &&
                       (visited.testBit(adjacentY * width + x) || !matches(x, adjacentY)))
                    ++x;
                if (x <= right)
                    pending.append(QPoint(x, adjacentY));
                while (x <= right && !visited.testBit(adjacentY * width + x) &&
                       matches(x, adjacentY))
                    ++x;
            }
        }
    }
    return result;
}

QRegion smoothRegion(const QRegion &_region)
{
    if (_region.isEmpty())
        return _region;
    QRegion expanded = _region;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            expanded |= _region.translated(x, y);
    QRegion smoothed = expanded;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            smoothed &= expanded.translated(x, y);
    return smoothed;
}
} // namespace

void Document::applySelectionCandidate(const QRegion &_candidate, bool _firstDab)
{
    QRegion       candidate = _candidate & QRegion(QRect(QPoint{}, m_size));
    const QRegion base      = _firstDab ? m_selectionRegionBeforeStroke : m_selectionRegion;
    const int     mode      = m_selectionMode == 0 && !_firstDab ? 1 : m_selectionMode;
    switch (mode) {
    case 1:
        m_selectionRegion = base.united(candidate);
        break;
    case 2:
        m_selectionRegion = base.subtracted(candidate);
        break;
    case 3:
        m_selectionRegion = base.intersected(candidate);
        break;
    default:
        m_selectionRegion = candidate;
        break;
    }
    m_selection      = m_selectionRegion.boundingRect();
    m_selectionShape = m_selectionRegion.isEmpty() ? 0 : 6;
    m_selectionPath.clear();
    m_polygonLassoActive = false;
    emit selectionChanged();
}

void Document::applyMagicWand(const QPoint &_point)
{
    if (!QRect(QPoint{}, m_size).contains(_point))
        return;
    QImage source;
    if (m_selectionSampleAllLayers || m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind != QStringLiteral("pixel"))
        source = composite();
    else
        source = layerImage(m_active);
    m_selectionRegionBeforeStroke = m_selectionRegion;
    QRegion candidate =
        colorRegion(source, _point, m_selectionTolerance, m_selectionContiguous, source.rect());
    applySelectionCandidate(candidate, true);
}

void Document::beginQuickSelection(const QPointF &_point)
{
    m_selectionRegionBeforeStroke = m_selectionRegion;
    m_selectionBoundsBeforeStroke = m_selection;
    m_selectionShapeBeforeStroke  = m_selectionShape;
    m_selectionPathBeforeStroke   = m_selectionPath;
    m_quickSelectionFirstDab      = true;
    m_drawing                     = true;
    continueQuickSelection(_point);
}

void Document::continueQuickSelection(const QPointF &_point)
{
    if (!m_drawing || m_size.isEmpty())
        return;
    QImage source;
    if (m_selectionSampleAllLayers || m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind != QStringLiteral("pixel"))
        source = composite();
    else
        source = layerImage(m_active);
    const QPoint seed(qBound(0, qRound(_point.x()), m_size.width() - 1),
                      qBound(0, qRound(_point.y()), m_size.height() - 1));
    const int    radius = qMax(2, qRound(m_brushSize / 2));
    const int    reach  = qMax(8, radius * 4);
    const QRect  limit(seed.x() - reach, seed.y() - reach, reach * 2 + 1, reach * 2 + 1);
    QRegion      candidate =
        colorRegion(source, seed, qBound(12, m_selectionTolerance, 64), true, limit);
    if (m_selectionEnhanceEdge)
        candidate = smoothRegion(candidate) & QRegion(source.rect());
    applySelectionCandidate(candidate, m_quickSelectionFirstDab);
    m_quickSelectionFirstDab = false;
}

void Document::finishQuickSelection()
{
    m_drawing                     = false;
    m_selectionRegionBeforeStroke = {};
    m_selectionBoundsBeforeStroke = {};
    m_selectionShapeBeforeStroke  = 0;
    m_selectionPathBeforeStroke.clear();
    m_quickSelectionFirstDab = false;
}

void Document::cancelQuickSelection()
{
    m_drawing                     = false;
    m_selectionRegion             = m_selectionRegionBeforeStroke;
    m_selection                   = m_selectionBoundsBeforeStroke;
    m_selectionShape              = m_selectionShapeBeforeStroke;
    m_selectionPath               = m_selectionPathBeforeStroke;
    m_selectionRegionBeforeStroke = {};
    m_selectionBoundsBeforeStroke = {};
    m_selectionShapeBeforeStroke  = 0;
    m_selectionPathBeforeStroke.clear();
    m_quickSelectionFirstDab = false;
    emit selectionChanged();
}
