#include "Document.h"
#include "RegionHealing.h"

#include <QPainter>
#include <QtConcurrent>
#include <QtMath>
#include <cmath>
#include <cstring>

bool Document::selectionContains(qreal _x, qreal _y) const
{
    return std::isfinite(_x) && std::isfinite(_y) && _x >= 0 && _y >= 0 && _x < width() &&
           _y < height() && m_selectionRegion.contains(QPoint(qFloor(_x), qFloor(_y)));
}

void Document::setPatchMode(int _value)
{
    endStroke();
    invalidatePatch(true);
    m_patchMode = qBound(0, _value, 1);
    emit settingsChanged();
}

void Document::setPatchDiffusion(int _value)
{
    endStroke();
    invalidatePatch(true);
    m_patchDiffusion = qBound(1, _value, 7);
    emit settingsChanged();
}

void Document::beginPatch(const QPointF &_point)
{
    if (!editable())
        return;
    if (!selectionContains(_point.x(), _point.y())) {
        beginLasso(_point.x(), _point.y());
        return;
    }
    m_beforeStroke        = snapshot(tr("Pièce"));
    m_patchSource         = toolPlane();
    if (m_editingMask && !m_patchSource.convertTo(QImage::Format_ARGB32_Premultiplied))
        return;
    m_patchOffset         = {};
    m_patchDragging       = true;
    m_drawing             = true;
    emit patchPreviewChanged();
}

void Document::continuePatch(const QPointF &_point)
{
    continueRegionDrag(_point, m_patchMode == 0);
}

void Document::continueRegionDrag(const QPointF &_point, bool _sourceMode)
{
    if (!m_patchDragging) {
        continueLasso(_point.x(), _point.y());
        return;
    }
    const QRect  original = m_beforeStroke.selection;
    const QPoint delta    = (_point - m_anchor).toPoint();
    const QPoint offset(qBound(-original.left(), delta.x(), width() - 1 - original.right()),
                        qBound(-original.top(), delta.y(), height() - 1 - original.bottom()));
    if (offset == m_patchOffset)
        return;
    m_patchOffset     = offset;
    m_selectionRegion = m_beforeStroke.selectionRegion.translated(offset);
    m_selection       = original.translated(offset);
    m_selectionPath   = m_beforeStroke.selectionPath.translated(offset);
    translateSelectionCoverage(m_beforeStroke, offset);
    m_patchTarget            = _sourceMode ? m_beforeStroke.selectionRegion : m_selectionRegion;
    const QRect target       = m_patchTarget.boundingRect();
    const QRect sourceBounds = m_beforeStroke.selectionRegion.boundingRect();
    const QRect source       = _sourceMode ? sourceBounds.translated(offset) : sourceBounds;
    QSize       previewSize  = target.size();
    previewSize.scale(1000, 1000, Qt::KeepAspectRatio);
    previewSize    = previewSize.boundedTo(target.size());
    m_patchPreview = QImage(previewSize, QImage::Format_ARGB32_Premultiplied);
    m_patchPreview.fill(Qt::transparent);
    QPainter painter(&m_patchPreview);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (m_regionPreviewSource.isNull()) {
        const auto sampleBounds =
            source.adjusted(-1, -1, 1, 1).intersected(QRect(QPoint{}, m_size));
        const auto sample = m_patchSource.region(sampleBounds);
        painter.drawImage(m_patchPreview.rect(), sample,
                          source.translated(-sampleBounds.topLeft()));
    } else {
        const auto  &sample = m_regionPreviewSource;
        const QRectF sampleRect(source.x() * qreal(sample.width()) / width(),
                                source.y() * qreal(sample.height()) / height(),
                                source.width() * qreal(sample.width()) / width(),
                                source.height() * qreal(sample.height()) / height());
        painter.drawImage(m_patchPreview.rect(), sample, sampleRect);
    }
    painter.end();
    emit selectionChanged();
    emit patchPreviewChanged();
}

namespace
{
struct PatchResult {
    QImage image;
    QRect  dirty;
};

PatchResult repairPatch(const PixelPlane &_source, const QSize &_size, const QRegion &_region,
                        const QPoint &_offset, int _diffusion, const QImage &_coverage,
                        const std::shared_ptr<std::atomic_bool> &_cancel)
{
    const QRect  document(QPoint{}, _size);
    const QRect  bounds          = _region.boundingRect();
    const int    halo            = RegionHealing::ToneHalo;
    const QRect  referenceBounds = bounds.adjusted(-halo, -halo, halo, halo).intersected(document);
    const QRect  donorBounds     = referenceBounds.translated(_offset).intersected(document);
    const QImage reference       = _source.region(referenceBounds);
    const QImage donor           = _source.region(donorBounds);
    if (reference.isNull() || donor.isNull() || _cancel->load())
        return {};
    const auto localRegion = _region.translated(-referenceBounds.topLeft());
    QImage     repaired    = RegionHealing::blend(
        donor, reference, localRegion, _offset + referenceBounds.topLeft() - donorBounds.topLeft(),
        _diffusion, 1., true, 0, _cancel);
    const QImage original = reference.copy(bounds.translated(-referenceBounds.topLeft()));
    if (repaired.isNull() || original.isNull() || _cancel->load())
        return {};
    QRect modified;
    for (const QRect &span : _region)
        for (int y = span.top(); y <= span.bottom(); ++y) {
            if (_cancel->load())
                return {};
            const auto *line       = repaired.constScanLine(y - bounds.y());
            const auto *before     = original.constScanLine(y - bounds.y());
            const int   pixelBytes = repaired.depth() / 8;
            int         left = span.right() + 1, right = span.left() - 1;
            for (int x = span.left(); x <= span.right(); ++x)
                if (std::memcmp(line + (x - bounds.x()) * pixelBytes,
                                before + (x - bounds.x()) * pixelBytes, pixelBytes) != 0) {
                    left  = qMin(left, x);
                    right = x;
                }
            if (left <= right)
                modified |= QRect(left, y, right - left + 1, 1);
        }
    if (modified.isEmpty())
        return {original, {}};
    const QRect local = modified.translated(-bounds.topLeft());
    if (!_coverage.isNull())
        SelectionCoverage::blend(repaired, original, _coverage.copy(bounds), local);
    if (_cancel->load())
        return {};
    return {repaired.copy(local), modified};
}
} // namespace

void Document::invalidatePatch(bool _restoreSelection)
{
    if (!m_patchBusy && !m_patchAwaitingImage)
        return;
    const bool restoreSelection = _restoreSelection && m_patchBusy;
    m_patchBusy                 = false;
    m_patchAwaitingImage        = false;
    ++m_patchGeneration;
    if (m_patchCancel)
        m_patchCancel->store(true);
    m_patchCancel.reset();
    clearPatch();
    if (restoreSelection) {
        m_selection           = m_beforePatch.selection;
        m_selectionRegion     = m_beforePatch.selectionRegion;
        m_selectionShape      = m_beforePatch.selectionShape;
        m_selectionPath       = m_beforePatch.selectionPath;
        m_selectionMask       = m_beforePatch.selectionMask;
        m_selectionOutline    = m_beforePatch.selectionOutline;
        m_selectionMaskRegion = m_selectionRegion;
        emit selectionChanged();
    }
    m_beforePatch = {};
    emit settingsChanged();
    emit historyChanged();
}

void Document::startPatchRepair()
{
    const Snapshot before = m_beforeStroke;
    const PixelPlane source     = m_patchSource;
    const QImage     coverage   = m_selectionMask;
    const QSize      size       = m_size;
    const QRegion  region     = m_patchTarget;
    const QPoint   offset     = m_patchMode == 0 ? m_patchOffset : -m_patchOffset;
    const int      diffusion  = m_patchDiffusion;
    const quint64  generation = ++m_patchGeneration;
    auto           cancel     = std::make_shared<std::atomic_bool>(false);
    m_patchCancel             = cancel;
    m_beforePatch             = before;
    m_beforeStroke            = {};
    m_patchBusy               = true;
    emit  settingsChanged();
    emit  historyChanged();
    auto *watcher = new QFutureWatcher<PatchResult>(this);
    connect(watcher, &QFutureWatcher<PatchResult>::finished, this,
            [this, watcher, before, generation, cancel] {
                const PatchResult result = watcher->result();
                watcher->deleteLater();
                if (cancel->load() || generation != m_patchGeneration || m_stateId != before.id ||
                    m_active != before.active || m_editingMask != before.editingMask)
                    return;
                m_patchBusy = false;
                m_patchCancel.reset();
                m_beforePatch = {};
                if (result.image.isNull() || result.dirty.isEmpty()) {
                    clearPatch();
                    emit settingsChanged();
                    emit historyChanged();
                    if (result.image.isNull())
                        emit statusMessage(
                            tr("Impossible de calculer la réparation de cette zone."));
                    return;
                }
                if (!writeToolRegion(result.dirty.topLeft(), result.image)) {
                    clearPatch();
                    emit settingsChanged();
                    emit historyChanged();
                    return;
                }
                m_dirtyRegion =
                    m_dirtyRegion.isEmpty() ? result.dirty : m_dirtyRegion.united(result.dirty);
                commit(before, tr("Pièce"), false, false);
                ++m_thumbnailRevision;
                emit thumbnailsChanged();
                m_patchAwaitingImage = true;
                requestPreviewRender(true, true);
                emit settingsChanged();
            });
    // All inputs are immutable values; the worker never accesses Document or QML.
    watcher->setFuture(
        QtConcurrent::run([source, size, region, offset, diffusion, coverage, cancel] {
            return repairPatch(source, size, region, offset, diffusion, coverage, cancel);
        }));
}

void Document::finishPatch(bool _cancel)
{
    m_drawing = false;
    if (!m_patchDragging) {
        if (_cancel)
            deselect();
        else
            finishLasso();
        return;
    }
    if (_cancel || m_patchMode == 0) {
        m_selection           = m_beforeStroke.selection;
        m_selectionRegion     = m_beforeStroke.selectionRegion;
        m_selectionPath       = m_beforeStroke.selectionPath;
        m_selectionMask       = m_beforeStroke.selectionMask;
        m_selectionOutline    = m_beforeStroke.selectionOutline;
        m_selectionMaskRegion = m_selectionRegion;
    }
    // Restore the Source selection before arming the job; subsequent selection
    // changes invalidate it. Keep the donor visible until the repaired composite arrives.
    emit selectionChanged();
    if (!_cancel && !m_patchOffset.isNull() && !m_patchSource.isNull())
        startPatchRepair();
    else {
        m_beforeStroke = {};
        clearPatch();
    }
}

void Document::clearPatch()
{
    m_patchDragging       = false;
    m_patchSource         = {};
    m_regionPreviewSource = {};
    m_patchPreview        = {};
    m_patchTarget         = {};
    m_patchOffset         = {};
    emit patchPreviewChanged();
}
