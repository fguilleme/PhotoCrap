#include "ContentFill.h"
#include "Document.h"
#include "RegionHealing.h"

#include <QtConcurrent>
#include <QtMath>

namespace
{
struct MoveResult {
    QImage fill, moved;
};

} // namespace

void Document::setContentMoveMode(int _value)
{
    endStroke();
    m_contentMoveMode = qBound(0, _value, 1);
    emit settingsChanged();
}
void Document::setContentMoveStructure(int _value)
{
    endStroke();
    m_contentMoveStructure = qBound(1, _value, 7);
    emit settingsChanged();
}
void Document::setContentMoveColor(int _value)
{
    endStroke();
    m_contentMoveColor = qBound(0, _value, 10);
    emit settingsChanged();
}
void Document::setContentMoveSampleAllLayers(bool _value)
{
    endStroke();
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
    auto *watcher = new QFutureWatcher<MoveResult>(this);
    connect(
        watcher, &QFutureWatcher<MoveResult>::finished, this,
        [this, watcher, before, original, destination, mode, generation, cancel] {
            const MoveResult result = watcher->result();
            watcher->deleteLater();
            if (cancel->load() || generation != m_contentMoveGeneration || m_stateId != before.id ||
                m_active != before.active)
                return;
            m_contentMoveBusy = false;
            m_contentMoveCancel.reset();
            m_beforeContentMove = {};
            if (result.moved.isNull() || (mode == 0 && result.fill.isNull())) {
                clearPatch();
                emit settingsChanged();
                emit statusMessage(tr("Pas assez de pixels voisins pour reconstruire cette zone."));
                return;
            }
            QImage      *target       = paintTarget();
            const QRect  sourceBounds = original.boundingRect(),
                         targetBounds = destination.boundingRect();
            const QImage oldSource    = target->copy(sourceBounds),
                         oldTarget    = target->copy(targetBounds);
            QImage       sourceWork = oldSource, targetWork = oldTarget;
            if (mode == 0) {
                QPainter painter(&sourceWork);
                painter.setClipRegion(original.translated(-sourceBounds.topLeft()));
                painter.setCompositionMode(QPainter::CompositionMode_Source);
                painter.drawImage(QPoint{}, result.fill);
                painter.end();
            }
            {
                QPainter painter(&targetWork);
                if (mode == 0) {
                    painter.setClipRegion(original.translated(-targetBounds.topLeft()));
                    painter.setCompositionMode(QPainter::CompositionMode_Source);
                    painter.drawImage(sourceBounds.topLeft() - targetBounds.topLeft(), sourceWork);
                }
                painter.setClipRegion(destination.translated(-targetBounds.topLeft()));
                painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
                painter.drawImage(QPoint{}, result.moved);
            }
            const bool sourceChanged = mode == 0 && sourceWork != oldSource;
            const bool targetChanged = targetWork != oldTarget;
            if (!sourceChanged && !targetChanged) {
                clearPatch();
                emit settingsChanged();
                return;
            }
            QPainter painter(target);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            if (sourceChanged) {
                painter.setClipRegion(original);
                painter.drawImage(sourceBounds.topLeft(), sourceWork);
            }
            if (targetChanged || (sourceChanged && !(original & destination).isEmpty())) {
                painter.setClipRegion(destination);
                painter.drawImage(targetBounds.topLeft(), targetWork);
            }
            painter.end();
            QRect dirty;
            if (sourceChanged)
                dirty = sourceBounds;
            if (targetChanged)
                dirty |= targetBounds;
            if (!before.selectionMask.isNull()) {
                QImage coverage = before.selectionMask;
                {
                    QPainter p(&coverage);
                    p.setCompositionMode(QPainter::CompositionMode_Lighten);
                    p.drawImage(m_patchOffset, before.selectionMask);
                }
                SelectionCoverage::blend(*target, before.layers[m_active].image, coverage, dirty);
            }
            m_dirtyRegion = m_dirtyRegion.isEmpty() ? dirty : m_dirtyRegion.united(dirty);
            commit(before, tr("Déplacement basé sur le contenu"), false, false);
            ++m_thumbnailRevision;
            emit thumbnailsChanged();
            m_contentMoveAwaitingImage = true;
            requestPreviewRender(true, true);
            emit settingsChanged();
        });
    // Only value types cross the worker boundary: no QML objects or Document access.
    watcher->setFuture(QtConcurrent::run([source, sampledLayers, documentSize, original,
                                          destination, offset, mode, structure, color, cancel] {
        const QImage sample =
            sampledLayers.isEmpty()
                ? source
                : renderPreviewSnapshot(sampledLayers, documentSize, documentSize, false);
        MoveResult result;
        if (mode == 0) {
            result.fill = ContentFill::reconstruct(sample, original, original | destination,
                                                   structure, cancel);
            if (result.fill.isNull())
                return result;
        }
        result.moved = RegionHealing::blend(sample, sample, destination, -offset, 8 - structure,
                                            color / 10., false, (7 - structure) / 2, cancel);
        return result;
    }));
}
