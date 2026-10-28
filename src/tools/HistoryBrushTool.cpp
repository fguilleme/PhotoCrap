#include "Document.h"
#include "ImageUtils.h"

#include <QPainter>
#include <QtMath>

int Document::historyBrushSourceIndex() const
{
    for (int i = 0; i < m_undo.size(); ++i)
        if (m_undo[i].id == m_historyBrushSource.id)
            return i;
    if (m_stateId == m_historyBrushSource.id)
        return int(m_undo.size());
    for (int i = 0; i < m_redo.size(); ++i)
        if (m_redo[m_redo.size() - 1 - i].id == m_historyBrushSource.id)
            return int(m_undo.size()) + 1 + i;
    return -1; // The selected source is retained even after its branch is discarded.
}

bool Document::historyBrushSourceAvailable() const
{
    if (m_editingMask || m_historyBrushSource.size != m_size || m_active < 0 ||
        m_active >= m_layers.size() || m_layers[m_active].kind != "pixel")
        return false;
    const QString id = m_layers[m_active].historyId;
    for (const Layer &layer : m_historyBrushSource.layers)
        if (layer.historyId == id && layer.kind == "pixel" && !layer.image.isNull())
            return true;
    return false;
}

void Document::setHistoryBrushSource(int _index)
{
    endStroke();
    const int current = int(m_undo.size());
    if (_index < 0 || _index > current + m_redo.size())
        return;
    const QString label = history().at(_index);
    if (_index < current)
        m_historyBrushSource = m_undo[_index];
    else if (_index == current)
        m_historyBrushSource = snapshot(label);
    else
        m_historyBrushSource = m_redo[m_redo.size() - (_index - current)];
    m_historyBrushSourceLabel = label;
    trimHistory();
    emit historyBrushSourceChanged();
    emit historyChanged();
}

bool Document::prepareHistoryBrushStroke()
{
    if (m_historyBrushSource.size != m_size)
        return fail(tr(
            "La source d’historique a des dimensions différentes. Choisissez un état compatible."));
    if (!historyBrushSourceAvailable())
        return fail(tr("Ce calque n’existe pas dans l’état source. Choisissez un autre état dans "
                       "l’Historique."));
    const QString id = m_layers[m_active].historyId;
    for (const Layer &layer : m_historyBrushSource.layers)
        if (layer.historyId == id) {
            // Immutable for the gesture, including restoration from a future
            // state after Undo. Layer identity survives reordering and renaming.
            m_historyStrokeSource = ImageUtils::expandedPlane(layer.image, m_size);
            return true;
        }
    return false;
}

void Document::renderHistoryBrushStroke()
{
    QImage *target = paintTarget();
    if (!target || m_historyStrokeSource.isNull() || m_strokeBase.isNull()) {
        m_strokeBounds = {};
        return;
    }
    const QRect   bounds = m_strokeBounds.toAlignedRect().intersected(target->rect());
    const QRegion region = hasSelection() ? QRegion(bounds) & m_selectionRegion : QRegion(bounds);
    if (region.isEmpty()) {
        m_strokeBounds = {};
        return;
    }
    const QRect work = region.boundingRect();
    QImage output    = m_strokeBase.copy(work).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage sample =
        m_historyStrokeSource.copy(work).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (m_brushBlend != 0)
        sample = blendImages(output, sample, m_brushBlend, 1.);
    const int opacity = qRound(m_brushOpacity * 255);
    QRect     dirty;
    // Restore all premultiplied channels, including alpha: transparent source
    // pixels must erase old paint rather than disappear in SourceOver blending.
    for (const QRect &span : region)
        for (int y = span.top(); y <= span.bottom(); ++y) {
            auto       *out    = reinterpret_cast<QRgb *>(output.scanLine(y - work.y()));
            const auto *source = reinterpret_cast<const QRgb *>(sample.constScanLine(y - work.y()));
            const auto *mask   = reinterpret_cast<const QRgb *>(m_strokeCoverage.constScanLine(y));
            const auto *old    = reinterpret_cast<const QRgb *>(target->constScanLine(y));
            for (int x = span.left(); x <= span.right(); ++x) {
                const int amount =
                    (qAlpha(mask[x]) * opacity * selectionCoverage(x, y) + 32512) / 65025;
                if (amount == 0)
                    continue;
                const int  column = x - work.x();
                const QRgb a = out[column], b = source[column];
                const auto mix = [amount](int _a, int _b) {
                    return (_a * (255 - amount) + _b * amount + 127) / 255;
                };
                out[column] = qRgba(mix(qRed(a), qRed(b)), mix(qGreen(a), qGreen(b)),
                                    mix(qBlue(a), qBlue(b)), mix(qAlpha(a), qAlpha(b)));
                if (out[column] != old[x])
                    dirty |= QRect(x, y, 1, 1);
            }
        }
    if (dirty.isEmpty()) {
        m_strokeBounds = {};
        return;
    }
    QPainter painter(target);
    painter.setClipRegion(region);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(work.topLeft(), output);
    painter.end();
    if (!m_fullCompositeDirty)
        m_dirtyRegion = m_dirtyRegion.united(dirty);
}
