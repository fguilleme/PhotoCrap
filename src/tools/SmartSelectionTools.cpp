#include "ColorFlood.h"
#include "Document.h"
#include "ImageUtils.h"

#include <QBitArray>
#include <QLineF>
#include <QtConcurrent>
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

bool uniformWandStack(const QVector<Layer> &_layers, const QSize &_size)
{
    const auto uniform = [&](const PixelPlane &_plane) {
        return _plane.isNull() ||
               (_plane.isUniform() && (_plane.size() == QSize(1, 1) || _plane.size() == _size) &&
                _plane.devicePixelRatio() == 1.);
    };
    for (const auto &layer : _layers) {
        if (layer.kind == "group" || !layer.visible || layer.opacity <= 0)
            continue;
        // Dissolve is coordinate-dependent even when its input is constant.
        // Other layer kinds remain on the normal path, including adjustments.
        if (layer.kind != "pixel" || layer.blend == 1 || !uniform(layer.image) ||
            (layer.maskEnabled && !uniform(layer.mask)))
            return false;
    }
    return true;
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

void Document::applySelectionCandidate(const QRegion &_candidate, bool _firstDab, int _mode)
{
    QRegion       candidate = _candidate & QRegion(QRect(QPoint{}, m_size));
    const QRegion base      = _firstDab ? m_selectionRegionBeforeStroke : m_selectionRegion;
    const int     requested = _mode < 0 ? m_selectionMode : _mode;
    const int     mode      = requested == 0 && !_firstDab ? 1 : requested;
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
    combineSoftSelection(candidate, mode, _firstDab ? m_beforeSelectionGesture : snapshot({}));
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
    const bool composed =
        m_active < 0 || m_active >= m_layers.size() ||
        (!m_editingMask && (m_selectionSampleAllLayers || m_layers[m_active].kind != "pixel"));
    const PixelPlane source     = composed ? PixelPlane{} : toolPlane();
    const int        tolerance  = m_selectionTolerance;
    const bool       contiguous = m_selectionContiguous;
    const bool       uniform =
        composed ? uniformWandStack(m_layers, m_size)
                 : source.isUniform() && (source.size() == QSize(1, 1) || source.size() == m_size);
    if (uniform) {
        // Every source pixel has the sampled value, so both connectivity modes
        // produce the whole document without reading or converting any pixels.
        rememberSelection();
        applySelectionCandidate(QRegion(QRect(QPoint{}, m_size)), true);
        finishSelectionGesture();
        return;
    }
    if (!m_backgroundWorker && editCost(false) >= m_backgroundEditThreshold) {
        finishLayerPreview();
        cancelSelectionOperation();
        const auto   before  = snapshot({});
        const auto   session = m_documentSession;
        const int    mode    = m_selectionMode;
        const auto   size    = m_size;
        const int    bits    = m_channelBits;
        const auto   scratch = m_scratchCache;
        const QImage completed =
            composed && !m_cacheDirty && m_composite.size() == m_size ? m_composite : QImage{};
        auto layers            = composed && completed.isNull() ? before.layers : QVector<Layer>{};
        auto cancel            = std::make_shared<std::atomic_bool>(false);
        m_selectionCancel      = cancel;
        m_selectionTaskStateId = before.id;
        emit selectionOperationChanged();
        using Result  = PhotoCrap::Tools::ColorFloodResult;
        auto *watcher = new QFutureWatcher<Result>(this);
        connect(watcher, &QFutureWatcher<Result>::finished, this,
                [this, watcher, before, session, mode, cancel] {
                    Result result;
                    try {
                        result = watcher->result();
                    } catch (...) {
                        result.valid = false;
                    }
                    watcher->deleteLater();
                    if (m_selectionCancel != cancel)
                        return;
                    cancelSelectionOperation();
                    if (session != m_documentSession || before.id != m_stateId || m_drawing ||
                        m_previewingLayer)
                        return;
                    if (!result.valid) {
                        if (!result.cancelled)
                            fail(tr("Mémoire insuffisante pour modifier la sélection."));
                        return;
                    }
                    rememberSelection();
                    applySelectionCandidate(result.region, true, mode);
                    finishSelectionGesture();
                });
        watcher->setFuture(QtConcurrent::run([source, layers = std::move(layers), size, bits,
                                              composed, tolerance, contiguous, _point, cancel,
                                              scratch, completed]() mutable {
            if (cancel->load()) {
                Result result;
                result.cancelled = true;
                return result;
            }
            if (!completed.isNull())
                return PhotoCrap::Tools::colorFlood(
                    size, _point, tolerance, contiguous,
                    [&](const QRect &_region) { return completed.copy(_region); }, cancel);
            if (!composed)
                return PhotoCrap::Tools::colorFlood(
                    size, _point, tolerance, contiguous,
                    [&](const QRect &_region) { return source.region(_region); }, cancel);
            // Construct the renderer on its worker thread; no GUI QObject or
            // JavaScript engine is accessed by the immutable layer snapshot.
            Document renderer;
            renderer.m_backgroundWorker   = true;
            renderer.m_historyBrushSource = {};
            renderer.m_layers             = std::move(layers);
            renderer.m_size               = size;
            renderer.m_channelBits        = bits;
            renderer.m_scratchCache       = scratch;
            renderer.m_cacheDirty = renderer.m_fullCompositeDirty = true;
            renderer.m_dirtyRegion                                = {};
            return PhotoCrap::Tools::colorFlood(
                size, _point, tolerance, contiguous,
                [&](const QRect &_region) { return renderer.compositeRegion(_region); }, cancel);
        }));
        return;
    }
    const auto result = PhotoCrap::Tools::colorFlood(
        m_size, _point, tolerance, contiguous, [&](const QRect &_region) {
            return composed ? compositeRegion(_region) : source.region(_region);
        });
    if (!result.valid) {
        fail(tr("Mémoire insuffisante pour modifier la sélection."));
        return;
    }
    rememberSelection();
    applySelectionCandidate(result.region, true);
    finishSelectionGesture();
}

void Document::beginQuickSelection(const QPointF &_point)
{
    rememberSelection();
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
    const QPoint seed(qBound(0, qRound(_point.x()), m_size.width() - 1),
                      qBound(0, qRound(_point.y()), m_size.height() - 1));
    const int    radius = qMax(2, qRound(m_brushSize / 2));
    const int    reach  = qMax(8, radius * 4);
    const QRect  documentRect(QPoint{}, m_size);
    const QRect  limit = QRect(seed.x() - reach, seed.y() - reach, reach * 2 + 1, reach * 2 + 1)
                             .intersected(documentRect);
    QImage       source;
    if (m_active < 0 || m_active >= m_layers.size() ||
        (!m_editingMask &&
         (m_selectionSampleAllLayers || m_layers[m_active].kind != QStringLiteral("pixel"))))
        source = compositeRegion(limit);
    else {
        const auto &plane = toolPlane();
        source = ImageUtils::compact(plane) ? ImageUtils::expandedPlane(plane, limit.size())
                                            : plane.region(limit);
    }
    // The flood search, native-to-8-bit conversion and visited map cover only
    // the search window. Restore document coordinates before combining edges.
    QRegion candidate = colorRegion(source, seed - limit.topLeft(),
                                    qBound(12, m_selectionTolerance, 64), true, source.rect())
                            .translated(limit.topLeft());
    if (m_selectionEnhanceEdge)
        candidate = smoothRegion(candidate) & QRegion(documentRect);
    applySelectionCandidate(candidate, m_quickSelectionFirstDab);
    m_quickSelectionFirstDab = false;
}

void Document::finishQuickSelection()
{
    finishSelectionGesture();
    m_drawing                     = false;
    m_selectionRegionBeforeStroke = {};
    m_selectionBoundsBeforeStroke = {};
    m_selectionShapeBeforeStroke  = 0;
    m_selectionPathBeforeStroke.clear();
    m_quickSelectionFirstDab = false;
}

void Document::cancelQuickSelection()
{
    m_selectionMask               = m_beforeSelectionGesture.selectionMask;
    m_selectionOutline            = m_beforeSelectionGesture.selectionOutline;
    m_selectionMaskRegion         = m_selectionRegionBeforeStroke;
    m_selectionGestureRecorded    = false;
    m_beforeSelectionGesture      = {};
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
