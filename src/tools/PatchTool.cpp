#include "Document.h"

#include <QPainter>
#include <QtMath>
#include <array>
#include <cmath>

namespace
{
using Tone = std::array<qreal, 3>;

struct PatchTone {
    QSize         size;
    QRect         bounds;
    QVector<Tone> values;

    Tone at(int _x, int _y) const
    {
        const qreal gx = qBound(0., (_x - bounds.x() + .5) * size.width() / bounds.width() - .5,
                                qreal(size.width() - 1));
        const qreal gy = qBound(0., (_y - bounds.y() + .5) * size.height() / bounds.height() - .5,
                                qreal(size.height() - 1));
        const int   x = int(gx), y = int(gy);
        const int   nx = qMin(x + 1, size.width() - 1), ny = qMin(y + 1, size.height() - 1);
        Tone        result{};
        for (int c = 0; c < 3; ++c) {
            const qreal top    = values[y * size.width() + x][c] * (1 - gx + x) +
                                 values[y * size.width() + nx][c] * (gx - x);
            const qreal bottom = values[ny * size.width() + x][c] * (1 - gx + x) +
                                 values[ny * size.width() + nx][c] * (gx - x);
            result[c]          = top * (1 - gy + y) + bottom * (gy - y);
        }
        return result;
    }
};

// Extend colour differences from the healthy pixels outside the selected area.
// A bounded harmonic field preserves the donor's full-resolution texture while
// avoiding both a full-image filter and a blur that would reintroduce the defect.
PatchTone patchTone(const QImage &_image, const QRegion &_region, const QPoint &_offset,
                    int _diffusion)
{
    const QRect   bounds = _region.boundingRect().adjusted(-4, -4, 4, 4).intersected(_image.rect());
    const int     side   = 48 - 5 * (_diffusion - 1);
    const QSize   size(qBound(2, bounds.width(), side), qBound(2, bounds.height(), side));
    PatchTone     field{size, bounds, QVector<Tone>(size.width() * size.height())};
    QVector<bool> fixed(field.values.size(), false);
    Tone          mean{};
    int           count = 0;
    for (int gy = 0; gy < size.height(); ++gy)
        for (int gx = 0; gx < size.width(); ++gx) {
            const int   index = gy * size.width() + gx;
            const QRect cell(bounds.x() + gx * bounds.width() / size.width(),
                             bounds.y() + gy * bounds.height() / size.height(),
                             qMax(1, (gx + 1) * bounds.width() / size.width() -
                                         gx * bounds.width() / size.width()),
                             qMax(1, (gy + 1) * bounds.height() / size.height() -
                                         gy * bounds.height() / size.height()));
            // Nine probes also catch the healthy rim in cells spanning a border.
            Tone tone{};
            int  samples = 0;
            for (int py = 0; py < 3; ++py)
                for (int px = 0; px < 3; ++px) {
                    const QPoint p(cell.x() + px * (cell.width() - 1) / 2,
                                   cell.y() + py * (cell.height() - 1) / 2);
                    const QPoint donor = p + _offset;
                    if (_region.contains(p) || !_image.rect().contains(donor))
                        continue;
                    const QRgb a = qUnpremultiply(_image.pixel(p));
                    const QRgb b = qUnpremultiply(_image.pixel(donor));
                    if (qAlpha(a) == 0 || qAlpha(b) == 0)
                        continue;
                    tone[0] += qRed(a) - qRed(b);
                    tone[1] += qGreen(a) - qGreen(b);
                    tone[2] += qBlue(a) - qBlue(b);
                    ++samples;
                }
            if (samples) {
                fixed[index] = true;
                for (int c = 0; c < 3; ++c) {
                    field.values[index][c] = tone[c] / samples;
                    mean[c] += field.values[index][c];
                }
                ++count;
            }
        }
    for (auto &v : mean)
        v /= qMax(1, count);
    for (qsizetype i = 0; i < field.values.size(); ++i)
        if (!fixed[i])
            field.values[i] = mean;
    for (int iteration = 0; iteration < 180; ++iteration)
        for (int y = 0; y < size.height(); ++y)
            for (int x = 0; x < size.width(); ++x) {
                const int index = y * size.width() + x;
                if (fixed[index])
                    continue;
                const int left   = y * size.width() + qMax(0, x - 1);
                const int right  = y * size.width() + qMin(size.width() - 1, x + 1);
                const int top    = qMax(0, y - 1) * size.width() + x;
                const int bottom = qMin(size.height() - 1, y + 1) * size.width() + x;
                for (int c = 0; c < 3; ++c) {
                    const qreal average = (field.values[left][c] + field.values[right][c] +
                                           field.values[top][c] + field.values[bottom][c]) /
                                          4;
                    field.values[index][c] += 1.4 * (average - field.values[index][c]);
                }
            }
    return field;
}
} // namespace

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
    m_beforeStroke  = snapshot(tr("Pièce"));
    m_patchSource   = *paintTarget();
    m_patchOffset   = {};
    m_patchDragging = true;
    m_drawing       = true;
    emit patchPreviewChanged();
}

void Document::continuePatch(const QPointF &_point)
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
    m_patchOffset      = offset;
    m_selectionRegion  = m_beforeStroke.selectionRegion.translated(offset);
    m_selection        = original.translated(offset);
    m_selectionPath    = m_beforeStroke.selectionPath.translated(offset);
    m_patchTarget      = m_patchMode == 0 ? m_beforeStroke.selectionRegion : m_selectionRegion;
    const QRect target = m_patchTarget.boundingRect();
    const QRect sourceBounds = m_beforeStroke.selectionRegion.boundingRect();
    const QRect source       = m_patchMode == 0 ? sourceBounds.translated(offset) : sourceBounds;
    QSize       previewSize  = target.size();
    previewSize.scale(1000, 1000, Qt::KeepAspectRatio);
    previewSize    = previewSize.boundedTo(target.size());
    m_patchPreview = QImage(previewSize, QImage::Format_ARGB32_Premultiplied);
    m_patchPreview.fill(Qt::transparent);
    QPainter painter(&m_patchPreview);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(m_patchPreview.rect(), m_patchSource, source);
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
    const QRegion   region       = m_patchTarget;
    const QRect     bounds       = region.boundingRect();
    const QPoint    sourceOffset = m_patchMode == 0 ? m_patchOffset : -m_patchOffset;
    const PatchTone tone         = patchTone(m_patchSource, region, sourceOffset, m_patchDiffusion);
    QImage          repaired =
        m_patchSource.copy(bounds).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QRect modified;
    // Iterate selected spans only; holes and unselected pixels are never written.
    for (const QRect &span : region)
        for (int y = span.top(); y <= span.bottom(); ++y) {
            auto       *line = reinterpret_cast<QRgb *>(repaired.scanLine(y - bounds.y()));
            const auto *source =
                reinterpret_cast<const QRgb *>(m_patchSource.constScanLine(y + sourceOffset.y()));
            for (int x = span.left(); x <= span.right(); ++x) {
                const QRgb donor = qUnpremultiply(source[x + sourceOffset.x()]);
                if (qAlpha(donor) == 0)
                    continue;
                const Tone correction = tone.at(x, y);
                const QRgb corrected  = qPremultiply(
                    qRgba(qBound(0, qRound(qRed(donor) + correction[0]), 255),
                          qBound(0, qRound(qGreen(donor) + correction[1]), 255),
                          qBound(0, qRound(qBlue(donor) + correction[2]), 255), qAlpha(donor)));
                const int  index   = x - bounds.x();
                const QRgb before  = line[index];
                const int  inverse = 255 - qAlpha(corrected);
                const QRgb after =
                    qRgba(qRed(corrected) + (qRed(before) * inverse + 127) / 255,
                          qGreen(corrected) + (qGreen(before) * inverse + 127) / 255,
                          qBlue(corrected) + (qBlue(before) * inverse + 127) / 255,
                          qAlpha(corrected) + (qAlpha(before) * inverse + 127) / 255);
                if (before != after) {
                    line[index] = after;
                    modified |= QRect(QPoint(index, y - bounds.y()), QSize(1, 1));
                }
            }
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
    m_patchDragging = false;
    m_patchSource   = {};
    m_patchPreview  = {};
    m_patchTarget   = {};
    m_patchOffset   = {};
    emit patchPreviewChanged();
}
