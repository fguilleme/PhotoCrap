#include "ContentFill.h"
#include "Document.h"
#include "RegionHealing.h"

#include <QtConcurrent>
#include <QtMath>

namespace
{
struct MoveResult {
    QImage image;
    QRect  dirty;
};

MoveResult assembleMove(const QImage &_source, const QRegion &_original,
                        const QRegion &_destination, const QImage &_fill, const QImage &_moved,
                        int _mode, const QImage &_coverage, const QPoint &_offset,
                        const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (_cancel->load() || _moved.isNull() || (_mode == 0 && _fill.isNull()))
        return {};
    const QRect sourceBounds = _original.boundingRect(), targetBounds = _destination.boundingRect();
    const QImage oldSource = _source.copy(sourceBounds), oldTarget = _source.copy(targetBounds);
    if (oldSource.isNull() || oldTarget.isNull() || _cancel->load())
        return {};
    QImage sourceWork = oldSource, targetWork = oldTarget;
    if (_mode == 0) {
        QPainter painter(&sourceWork);
        painter.setClipRegion(_original.translated(-sourceBounds.topLeft()));
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(QPoint{}, _fill);
    }
    {
        QPainter painter(&targetWork);
        if (_mode == 0) {
            painter.setClipRegion(_original.translated(-targetBounds.topLeft()));
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.drawImage(sourceBounds.topLeft() - targetBounds.topLeft(), sourceWork);
        }
        painter.setClipRegion(_destination.translated(-targetBounds.topLeft()));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(QPoint{}, _moved);
    }
    const bool sourceChanged = _mode == 0 && sourceWork != oldSource;
    const bool targetChanged = targetWork != oldTarget;
    if (_cancel->load())
        return {};
    if (!sourceChanged && !targetChanged)
        return {_source, {}};
    // Detach the full layer here, not in the GUI completion callback.
    QImage result = _source.copy();
    if (result.isNull() || _cancel->load())
        return {};
    {
        QPainter painter(&result);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        if (sourceChanged) {
            painter.setClipRegion(_original);
            painter.drawImage(sourceBounds.topLeft(), sourceWork);
        }
        if (targetChanged || (sourceChanged && !(_original & _destination).isEmpty())) {
            painter.setClipRegion(_destination);
            painter.drawImage(targetBounds.topLeft(), targetWork);
        }
    }
    QRect dirty;
    if (sourceChanged)
        dirty = sourceBounds;
    if (targetChanged)
        dirty |= targetBounds;
    if (!_coverage.isNull()) {
        QImage coverage = _coverage;
        {
            QPainter painter(&coverage);
            painter.setCompositionMode(QPainter::CompositionMode_Lighten);
            painter.drawImage(_offset, _coverage);
        }
        // The empty gap between separated source/destination regions needs no
        // soft-coverage work. The same maximum coverage preserves their overlap.
        const QRegion affected = (_original | _destination) & dirty;
        for (const QRect &span : affected) {
            if (_cancel->load())
                return {};
            SelectionCoverage::blend(result, _source, coverage, span);
        }
    }
    return _cancel->load() ? MoveResult{} : MoveResult{result, dirty};
}

} // namespace

void Document::setContentMoveMode(int _value)
{
    endStroke();
    invalidateContentMove(true);
    m_contentMoveMode = qBound(0, _value, 1);
    emit settingsChanged();
}
void Document::setContentMoveStructure(int _value)
{
    endStroke();
    invalidateContentMove(true);
    m_contentMoveStructure = qBound(1, _value, 7);
    emit settingsChanged();
}
void Document::setContentMoveColor(int _value)
{
    endStroke();
    invalidateContentMove(true);
    m_contentMoveColor = qBound(0, _value, 10);
    emit settingsChanged();
}
void Document::setContentMoveSampleAllLayers(bool _value)
{
    endStroke();
    invalidateContentMove(true);
    m_contentMoveSampleAllLayers = _value;
    emit settingsChanged();
}

void Document::invalidateContentMove(bool _restoreSelection)
{
    if (!m_contentMoveBusy && !m_contentMoveAwaitingImage)
        return;
    const bool restoreSelection = _restoreSelection && m_contentMoveBusy;
    m_contentMoveBusy           = false;
    m_contentMoveAwaitingImage  = false;
    ++m_contentMoveGeneration;
    if (m_contentMoveCancel)
        m_contentMoveCancel->store(true);
    m_contentMoveCancel.reset();
    clearPatch();
    if (restoreSelection) {
        m_selection           = m_beforeContentMove.selection;
        m_selectionRegion     = m_beforeContentMove.selectionRegion;
        m_selectionShape      = m_beforeContentMove.selectionShape;
        m_selectionPath       = m_beforeContentMove.selectionPath;
        m_selectionMask       = m_beforeContentMove.selectionMask;
        m_selectionOutline    = m_beforeContentMove.selectionOutline;
        m_selectionMaskRegion = m_selectionRegion;
        emit selectionChanged();
    }
    m_beforeContentMove = {};
    emit settingsChanged();
    emit historyChanged();
}

void Document::beginContentMove(const QPointF &_point)
{
    beginPatch(_point);
    if (m_patchDragging) {
        m_beforeStroke.label = tr("Déplacement basé sur le contenu");
        if (m_contentMoveSampleAllLayers)
            m_regionPreviewSource = displayComposite();
    }
}

void Document::finishContentMove(bool _cancel)
{
    if (!m_patchDragging || _cancel) {
        // Cancellation restores the original selection, as in the Patch tool.
        finishPatch(_cancel);
        return;
    }
    m_drawing = false;
    if (m_patchOffset.isNull()) {
        m_beforeStroke = {};
        clearPatch();
        return;
    }
    const Snapshot       before        = m_beforeStroke;
    const QImage         source        = m_patchSource;
    const QVector<Layer> sampledLayers = m_contentMoveSampleAllLayers ? m_layers : QVector<Layer>{};
    const QSize          documentSize  = m_size;
    const QImage         coverage      = before.selectionMask;
    const QRegion        original = before.selectionRegion, destination = m_patchTarget;
    const QPoint         offset = m_patchOffset;
    const int            mode = m_contentMoveMode, structure = m_contentMoveStructure,
                         color      = m_contentMoveColor;
    const quint64        generation = ++m_contentMoveGeneration;
    auto                 cancel     = std::make_shared<std::atomic_bool>(false);
    m_contentMoveCancel             = cancel;
    m_beforeContentMove             = before;
    m_beforeStroke                  = {};
    m_contentMoveBusy               = true;
    emit  settingsChanged();
    emit  historyChanged();
    auto *watcher = new QFutureWatcher<MoveResult>(this);
    connect(watcher, &QFutureWatcher<MoveResult>::finished, this,
            [this, watcher, before, generation, cancel] {
                const MoveResult result = watcher->result();
                watcher->deleteLater();
                if (cancel->load() || generation != m_contentMoveGeneration ||
                    m_stateId != before.id || m_active != before.active)
                    return;
                m_contentMoveBusy = false;
                m_contentMoveCancel.reset();
                m_beforeContentMove = {};
                if (result.image.isNull() || result.dirty.isEmpty()) {
                    clearPatch();
                    emit settingsChanged();
                    emit historyChanged();
                    if (result.image.isNull())
                        emit statusMessage(
                            tr("Pas assez de pixels voisins pour reconstruire cette zone."));
                    return;
                }
                QImage *target = paintTarget();
                if (!target) {
                    clearPatch();
                    emit settingsChanged();
                    emit historyChanged();
                    return;
                }
                *target = result.image;
                m_dirtyRegion =
                    m_dirtyRegion.isEmpty() ? result.dirty : m_dirtyRegion.united(result.dirty);
                commit(before, tr("Déplacement basé sur le contenu"), false, false);
                ++m_thumbnailRevision;
                emit thumbnailsChanged();
                m_contentMoveAwaitingImage = true;
                requestPreviewRender(true, true);
                emit settingsChanged();
            });
    // Only value types cross the worker boundary: no QML objects or Document access.
    watcher->setFuture(
        QtConcurrent::run([source, sampledLayers, documentSize, original, destination, offset, mode,
                           structure, color, coverage, cancel] {
            const QImage sample =
                sampledLayers.isEmpty()
                    ? source
                    : renderPreviewSnapshot(sampledLayers, documentSize, documentSize, false);
            if (cancel->load())
                return MoveResult{};
            QImage fill;
            if (mode == 0) {
                fill = ContentFill::reconstruct(sample, original, original | destination, structure,
                                                cancel);
                if (fill.isNull())
                    return MoveResult{};
            }
            const QImage moved =
                RegionHealing::blend(sample, sample, destination, -offset, 8 - structure,
                                     color / 10., false, (7 - structure) / 2, cancel);
            return assembleMove(source, original, destination, fill, moved, mode, coverage, offset,
                                cancel);
        }));
}
