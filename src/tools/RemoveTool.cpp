#include "ContentFill.h"
#include "Document.h"
#include "ImageUtils.h"

#include <QPainterPathStroker>
#include <QtConcurrent>
#include <QtMath>
#include <algorithm>

namespace
{
struct RemovalResult {
    QImage image;
    QRect  dirty;
    bool   valid = false;
};

RemovalResult finishRemoval(const QImage &_source, const QImage &_fill, const QRegion &_region,
                            const QImage                            &_coverage,
                            const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (_fill.isNull() || _source.isNull() || _cancel->load())
        return {};
    const QRect  bounds = _region.boundingRect();
    const QImage old    = _source.copy(bounds);
    if (old.isNull())
        return {};
    QImage work = old;
    {
        QPainter painter(&work);
        painter.setClipRegion(_region.translated(-bounds.topLeft()));
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(QPoint{}, _fill);
    }
    if (_cancel->load())
        return {};
    if (work == old)
        return {{}, {}, true};
    // All full-plane allocation and detachment stays on this worker, including
    // uniform/empty layers that are still stored as a single pixel in Document.
    QImage result = _source.copy();
    if (result.isNull() || _cancel->load())
        return {};
    {
        QPainter painter(&result);
        painter.setClipRegion(_region);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(bounds.topLeft(), work);
    }
    for (const QRect &span : _region) {
        if (_cancel->load())
            return {};
        SelectionCoverage::blend(result, _source, _coverage, span);
    }
    return _cancel->load() ? RemovalResult{} : RemovalResult{result, bounds, true};
}

// Close edge-to-edge strokes along the shorter image perimeter. In particular,
// a diagonal stroke across a corner includes that corner without marking the
// rest of the image. A stroke away from the edges remains an ordinary brush.
QPolygonF closedRemovalLoop(const QPolygonF &_points, const QSize &_size, qreal _brushSize)
{
    if (_points.size() < 3)
        return {};
    QPolygonF loop = _points;
    if (QLineF(loop.first(), loop.last()).length() <= qMax(8., _brushSize))
        return loop;
    const qreal w = _size.width() - 1, h = _size.height() - 1, perimeter = 2 * (w + h);
    if (w <= 0 || h <= 0)
        return {};
    const auto project = [w, h, _brushSize](const QPointF &_point, QPointF &_edge,
                                            qreal &_position) {
        const qreal x = qBound(0., _point.x(), w), y = qBound(0., _point.y(), h);
        const qreal distances[] = {qAbs(_point.y()), qAbs(_point.x() - w), qAbs(_point.y() - h),
                                   qAbs(_point.x())};
        const int   side        = int(std::min_element(std::begin(distances), std::end(distances)) -
                                      std::begin(distances));
        if (distances[side] > qMax(4., _brushSize / 2))
            return false;
        if (side == 0) {
            _edge     = QPointF(x, 0);
            _position = x;
        } else if (side == 1) {
            _edge     = QPointF(w, y);
            _position = w + y;
        } else if (side == 2) {
            _edge     = QPointF(x, h);
            _position = w + h + w - x;
        } else {
            _edge     = QPointF(0, y);
            _position = 2 * w + 2 * h - y;
        }
        return true;
    };
    QPointF firstEdge, lastEdge;
    qreal   first = 0, last = 0;
    if (!project(loop.first(), firstEdge, first) || !project(loop.last(), lastEdge, last))
        return {};
    qreal distance = first - last;
    if (distance < 0)
        distance += perimeter;
    if (distance > perimeter / 2)
        distance -= perimeter;
    QVector<QPair<qreal, QPointF>> corners;
    for (int cycle = -1; cycle <= 1; ++cycle) {
        const qreal offset = cycle * perimeter;
        for (const auto &corner :
             {qMakePair(0., QPointF(0, 0)), qMakePair(w, QPointF(w, 0)),
              qMakePair(w + h, QPointF(w, h)), qMakePair(2 * w + h, QPointF(0, h))}) {
            const qreal position = corner.first + offset;
            if ((distance > 0 && position > last && position < last + distance) ||
                (distance < 0 && position < last && position > last + distance))
                corners.append(qMakePair(position, corner.second));
        }
    }
    std::sort(corners.begin(), corners.end(), [distance](const auto &_a, const auto &_b) {
        return distance > 0 ? _a.first < _b.first : _a.first > _b.first;
    });
    loop.append(lastEdge);
    for (const auto &corner : corners)
        loop.append(corner.second);
    loop.append(firstEdge);
    return loop;
}
} // namespace

void Document::setRemoveSampleAllLayers(bool _value)
{
    endStroke();
    stopRemovalJob();
    m_removeSampleAllLayers = _value;
    emit settingsChanged();
}
void Document::setRemoveAfterEachStroke(bool _value)
{
    endStroke();
    m_removeAfterEachStroke = _value;
    emit settingsChanged();
}
void Document::setRemoveSubtract(bool _value)
{
    endStroke();
    m_removeSubtract = _value;
    emit settingsChanged();
}
void Document::stopRemovalJob()
{
    ++m_removeGeneration;
    if (m_removeCancel)
        m_removeCancel->store(true);
    m_removeCancel.reset();
    const bool busy = m_removeBusy;
    m_removeBusy    = false;
    if (busy) {
        emit settingsChanged();
        emit historyChanged();
    }
}
void Document::cancelRemoval()
{
    stopRemovalJob();
    if (m_tool == "remove")
        m_drawing = false;
    m_removePath.clear();
    if (!m_removeRegion.isEmpty()) {
        m_removeRegion = {};
        emit removeRegionChanged();
    }
}
void Document::beginRemove(const QPointF &_point, bool _alternate)
{
    if (!editable() || m_editingMask)
        return;
    // New marks invalidate the old result, retaining every unapplied mark. This
    // avoids losing a stroke when another stroke starts while the worker runs.
    stopRemovalJob();
    m_removeStrokeSubtract = m_removeSubtract || _alternate;
    m_removePath           = {_point};
    m_drawing              = true;
    continueRemove(_point);
}
void Document::continueRemove(const QPointF &_point)
{
    const QPointF previous = m_removePath.last();
    if (QLineF(previous, _point).length() > .01)
        m_removePath.append(_point);
    QPainterPath path;
    path.moveTo(previous);
    path.lineTo(_point);
    QPainterPathStroker stroker;
    stroker.setWidth(m_brushSize);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    QPainterPath area = stroker.createStroke(path);
    area.addEllipse(_point, m_brushSize / 2, m_brushSize / 2);
    QRegion region(area.toFillPolygon().toPolygon(), Qt::WindingFill);
    region &= QRegion(QRect(QPoint{}, m_size));
    if (hasSelection())
        region &= m_selectionRegion;
    if (m_removeStrokeSubtract)
        m_removeRegion -= region;
    else
        m_removeRegion |= region;
    emit removeRegionChanged();
}
void Document::finishRemove()
{
    m_drawing = false;
    // Mark the interior of loops and edge-to-edge contours, as well as the rim.
    const QPolygonF loop = closedRemovalLoop(m_removePath, m_size, m_brushSize);
    if (!loop.isEmpty()) {
        QRegion interior(loop.toPolygon(), Qt::OddEvenFill);
        interior &= QRegion(QRect(QPoint{}, m_size));
        if (hasSelection())
            interior &= m_selectionRegion;
        if (m_removeStrokeSubtract)
            m_removeRegion -= interior;
        else
            m_removeRegion |= interior;
        emit removeRegionChanged();
    }
    m_removePath.clear();
    if (m_removeAfterEachStroke)
        applyRemoval();
}
void Document::applyRemoval()
{
    if (m_drawing) {
        // Finish the current path once; avoid recursion in automatic mode.
        const bool automatic    = m_removeAfterEachStroke;
        m_removeAfterEachStroke = false;
        finishRemove();
        m_removeAfterEachStroke = automatic;
    }
    if (m_removeRegion.isEmpty() || m_removeBusy || !editable() || m_editingMask)
        return;
    const Snapshot       before     = snapshot(tr("Suppression"));
    const QImage         source     = m_layers[m_active].image;
    const QImage         coverage   = before.selectionMask;
    const QVector<Layer> layers     = m_removeSampleAllLayers ? m_layers : QVector<Layer>{};
    const QSize          size       = m_size;
    const QRegion        region     = m_removeRegion;
    const quint64        generation = ++m_removeGeneration;
    auto                 cancel     = std::make_shared<std::atomic_bool>(false);
    m_removeCancel                  = cancel;
    m_removeBusy                    = true;
    emit  settingsChanged();
    emit  historyChanged();
    auto *watcher = new QFutureWatcher<RemovalResult>(this);
    connect(watcher, &QFutureWatcher<RemovalResult>::finished, this,
            [this, watcher, before, generation, cancel] {
                const RemovalResult result = watcher->result();
                watcher->deleteLater();
                if (cancel->load() || generation != m_removeGeneration || m_stateId != before.id ||
                    m_active != before.active)
                    return;
                m_removeBusy = false;
                m_removeCancel.reset();
                emit settingsChanged();
                if (!result.valid) {
                    emit historyChanged();
                    emit statusMessage(
                        tr("Pas assez de pixels voisins pour reconstruire cette zone."));
                    return;
                }
                cancelRemoval();
                if (result.dirty.isEmpty()) {
                    emit historyChanged();
                    return;
                }
                // The worker already expanded compact layers and owns the
                // finished pixels; paintTarget() would expand again on the GUI.
                m_layers[m_active].image = result.image;
                m_dirtyRegion            = m_dirtyRegion.united(result.dirty);
                commit(before, tr("Suppression"), false, false);
                ++m_thumbnailRevision;
                emit thumbnailsChanged();
                requestPreviewRender(true, true);
            });
    // Document, QML and UI state stay on the GUI thread. The worker sees only
    // immutable images/regions and a cancellation token.
    watcher->setFuture(QtConcurrent::run([source, layers, size, region, coverage, cancel] {
        if (cancel->load())
            return RemovalResult{};
        const QImage target = ImageUtils::expandedPlane(source, size);
        if (target.isNull() || cancel->load())
            return RemovalResult{};
        const QImage sample =
            layers.isEmpty() ? target : renderPreviewSnapshot(layers, size, size, false);
        const QImage fill = ContentFill::reconstruct(sample, region, region, 4, cancel);
        return finishRemoval(target, fill, region, coverage, cancel);
    }));
}
