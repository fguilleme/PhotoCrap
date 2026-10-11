#include "Document.h"
#include "RegionHealing.h"

#include <QPainter>
#include <QtMath>
#include <cmath>

bool Document::selectionContains(qreal _x, qreal _y) const
{
    return std::isfinite(_x) && std::isfinite(_y) && _x >= 0 && _y >= 0 && _x < width() &&
           _y < height() && m_selectionRegion.contains(QPoint(qFloor(_x), qFloor(_y)));
}

void Document::setPatchMode(int _value)
{
    endStroke();
    m_patchMode = qBound(0, _value, 1);
    emit settingsChanged();
}

void Document::setPatchDiffusion(int _value)
{
    endStroke();
    m_patchDiffusion = qBound(1, _value, 7);
    emit settingsChanged();
}

void Document::beginPatch(const QPointF &_point)
{
    if (!editable())
        return;
    if (m_editingMask) {
        fail(tr("Utilisez le pinceau, la gomme ou le remplissage pour modifier un masque."));
        return;
    }
    if (!selectionContains(_point.x(), _point.y())) {
        beginLasso(_point.x(), _point.y());
        return;
    }
    m_beforeStroke        = snapshot(tr("Pièce"));
    m_patchSource         = *paintTarget();
    m_regionPreviewSource = m_patchSource;
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
    m_patchOffset            = offset;
    m_selectionRegion        = m_beforeStroke.selectionRegion.translated(offset);
    m_selection              = original.translated(offset);
    m_selectionPath          = m_beforeStroke.selectionPath.translated(offset);
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
    const QImage &sample = m_regionPreviewSource;
    const QRectF  sampleRect(source.x() * qreal(sample.width()) / width(),
                             source.y() * qreal(sample.height()) / height(),
                             source.width() * qreal(sample.width()) / width(),
                             source.height() * qreal(sample.height()) / height());
    painter.drawImage(m_patchPreview.rect(), sample, sampleRect);
    painter.end();
    emit selectionChanged();
    emit patchPreviewChanged();
}

void Document::applyPatch()
{
    if (m_patchOffset.isNull() || m_patchSource.isNull())
        return;
    QImage *target = paintTarget();
    if (!target)
        return;
    const QRegion region       = m_patchTarget;
    const QRect   bounds       = region.boundingRect();
    const QPoint  sourceOffset = m_patchMode == 0 ? m_patchOffset : -m_patchOffset;
    const QImage repaired = RegionHealing::blend(m_patchSource, m_patchSource, region, sourceOffset,
                                                 m_patchDiffusion, 1., true);
    QRect        modified;
    for (const QRect &span : region)
        for (int y = span.top(); y <= span.bottom(); ++y) {
            const auto *line =
                reinterpret_cast<const QRgb *>(repaired.constScanLine(y - bounds.y()));
            const auto *before = reinterpret_cast<const QRgb *>(target->constScanLine(y));
            for (int x = span.left(); x <= span.right(); ++x)
                if (line[x - bounds.x()] != before[x])
                    modified |= QRect(QPoint(x - bounds.x(), y - bounds.y()), QSize(1, 1));
        }
    if (modified.isEmpty())
        return;
    const QRect dirty = modified.translated(bounds.topLeft());
    QPainter    painter(target);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(dirty.topLeft(), repaired, modified);
    painter.end();
    m_dirtyRegion = m_dirtyRegion.isEmpty() ? dirty : m_dirtyRegion.united(dirty);
    commit(m_beforeStroke, tr("Pièce"), false);
    ++m_thumbnailRevision;
    emit thumbnailsChanged();
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
        m_selection       = m_beforeStroke.selection;
        m_selectionRegion = m_beforeStroke.selectionRegion;
        m_selectionPath   = m_beforeStroke.selectionPath;
    }
    // Hide the donor preview before imageChanged requests the final composite.
    m_patchPreview = {};
    if (!_cancel)
        applyPatch();
    m_beforeStroke = {};
    clearPatch();
    emit selectionChanged();
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
