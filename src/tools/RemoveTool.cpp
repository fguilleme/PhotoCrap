#include "ContentFill.h"
#include "Document.h"

#include <QPainterPathStroker>
#include <QtConcurrent>
#include <QtMath>
#include <algorithm>

namespace
{
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
    if (busy)
        emit settingsChanged();
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
    const QVector<Layer> layers     = m_removeSampleAllLayers ? m_layers : QVector<Layer>{};
    const QSize          size       = m_size;
    const QRegion        region     = m_removeRegion;
    const quint64        generation = ++m_removeGeneration;
    auto                 cancel     = std::make_shared<std::atomic_bool>(false);
    m_removeCancel                  = cancel;
    m_removeBusy                    = true;
    emit  settingsChanged();
    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this,
            [this, watcher, before, region, generation, cancel] {
                const QImage fill = watcher->result();
                watcher->deleteLater();
                if (cancel->load() || generation != m_removeGeneration || m_stateId != before.id ||
                    m_active != before.active)
                    return;
                m_removeBusy = false;
                m_removeCancel.reset();
                emit settingsChanged();
                if (fill.isNull()) {
                    emit statusMessage(
                        tr("Pas assez de pixels voisins pour reconstruire cette zone."));
                    return;
                }
                QImage      *target = paintTarget();
                const QRect  bounds = region.boundingRect();
                const QImage old    = target->copy(bounds);
                QImage       work   = old;
                {
                    QPainter painter(&work);
                    painter.setClipRegion(region.translated(-bounds.topLeft()));
                    painter.setCompositionMode(QPainter::CompositionMode_Source);
                    painter.drawImage(QPoint{}, fill);
                }
                cancelRemoval();
                if (work == old)
                    return;
                QPainter painter(target);
                painter.setClipRegion(region);
                painter.setCompositionMode(QPainter::CompositionMode_Source);
                painter.drawImage(bounds.topLeft(), work);
                painter.end();
                m_dirtyRegion = m_dirtyRegion.united(bounds);
                commit(before, tr("Suppression"), false, false);
                ++m_thumbnailRevision;
                emit thumbnailsChanged();
                requestPreviewRender(true, true);
            });
    // Document, QML and UI state stay on the GUI thread. The worker sees only
    // immutable images/regions and a cancellation token.
    watcher->setFuture(QtConcurrent::run([source, layers, size, region, cancel] {
        const QImage sample =
            layers.isEmpty() ? source : renderPreviewSnapshot(layers, size, size, false);
        return ContentFill::reconstruct(sample, region, region, 4, cancel);
    }));
}
