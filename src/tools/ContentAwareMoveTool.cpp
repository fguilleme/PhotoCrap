#include "ContentFill.h"
#include "Document.h"
#include "RegionHealing.h"

#include <QtConcurrent>
#include <QtMath>

namespace
{
struct MoveResult {
    PixelPlane plane;
    QRect      dirty;
};

MoveResult assembleMove(const PixelPlane &_source, const QSize &_size, const QRegion &_original,
                        const QRegion &_destination, const QImage &_fill, const QImage &_moved,
                        int _mode, const QImage &_coverage, const QPoint &_offset,
                        const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (_cancel->load() || _moved.isNull() || (_mode == 0 && _fill.isNull()))
        return {};
    const QRect sourceBounds = _original.boundingRect(), targetBounds = _destination.boundingRect();
    const QImage oldSource = _source.region(sourceBounds), oldTarget = _source.region(targetBounds);
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
    PixelPlane result = _source;
    const auto apply  = [&](const QRect &_bounds, QImage &_work, const QImage &_before) {
        if (_cancel->load())
            return false;
        if (!_coverage.isNull()) {
            auto coverage = _coverage.copy(_bounds);
            if (coverage.isNull())
                return false;
            QPainter painter(&coverage);
            painter.setCompositionMode(QPainter::CompositionMode_Lighten);
            painter.drawImage(_offset - _bounds.topLeft(), _coverage);
            painter.end();
            SelectionCoverage::blend(_work, _before, coverage, _work.rect());
        }
        return result.writeRegion(_bounds.topLeft(), _work, _size);
    };
    QRect dirty;
    if (sourceChanged) {
        if (!apply(sourceBounds, sourceWork, oldSource))
            return {};
        dirty = sourceBounds;
    }
    if (targetChanged || (sourceChanged && !(_original & _destination).isEmpty())) {
        if (!apply(targetBounds, targetWork, oldTarget))
            return {};
    }
    if (targetChanged)
        dirty |= targetBounds;
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
        if (m_contentMoveSampleAllLayers && !m_editingMask)
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
    const PixelPlane     plane         = m_patchSource;
    const QVector<Layer> sampledLayers =
        m_contentMoveSampleAllLayers && !m_editingMask ? m_layers : QVector<Layer>{};
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
                    m_stateId != before.id || m_active != before.active ||
                    m_editingMask != before.editingMask)
                    return;
                m_contentMoveBusy = false;
                m_contentMoveCancel.reset();
                m_beforeContentMove = {};
                if (result.plane.isNull() || result.dirty.isEmpty()) {
                    clearPatch();
                    emit settingsChanged();
                    emit historyChanged();
                    if (result.plane.isNull())
                        emit statusMessage(
                            tr("Pas assez de pixels voisins pour reconstruire cette zone."));
                    return;
                }
                if (!replaceToolPlane(result.plane))
                    return;
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
        QtConcurrent::run([plane, sampledLayers, documentSize, original, destination, offset, mode,
                           structure, color, coverage, cancel, mask = m_editingMask] {
            const QImage sample =
                sampledLayers.isEmpty()
                    ? plane.region(QRect(QPoint{}, documentSize))
                    : renderPreviewSnapshot(sampledLayers, documentSize, documentSize, false);
            if (sample.isNull() || cancel->load())
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
            auto result = assembleMove(plane, documentSize, original, destination, fill, moved,
                                       mode, coverage, offset, cancel);
            if (mask && !result.plane.isNull() &&
                !result.plane.convertTo(QImage::Format_Grayscale8))
                return MoveResult{};
            return result;
        }));
}
