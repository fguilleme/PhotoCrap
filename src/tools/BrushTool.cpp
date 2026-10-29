#include "Document.h"
#include "ImageUtils.h"

#include <QPainter>
#include <QtMath>
#include <cstring>

namespace
{
QColor maskColor(const QColor &_color)
{
    const int value = qGray(_color.rgb());
    return QColor(value, value, value);
}
} // namespace

void Document::paintSegment(const QPointF &_from, const QPointF &_to)
{
    Q_UNUSED(_from)
    if (m_tool != "smudge" && m_tool != "mixerBrush" && m_tool != "healing" &&
        m_tool != "spotHealing" && m_strokeCoverage.isNull() && m_tiledStrokeCoverage.isNull())
        return;
    m_strokeBounds = {};
    if (m_anchor == _to && !m_strokeChanged)
        stampBrush(_to);
    else {
        const qreal   response = 1. - m_brushSmoothing * .85;
        const QPointF target   = m_smoothedLast + (_to - m_smoothedLast) * response;
        QPointF       cursor   = m_smoothedLast;
        qreal         distance = QLineF(cursor, target).length();
        const qreal   step     = qMax(1., m_brushSize * m_brushSpacing);
        while (distance >= m_distanceToNextDab && distance > 0) {
            const QPointF direction = (target - cursor) / distance;
            cursor += direction * m_distanceToNextDab;
            stampBrush(cursor);
            distance            = QLineF(cursor, target).length();
            m_distanceToNextDab = step;
        }
        m_distanceToNextDab -= distance;
        m_smoothedLast = target;
    }
    if (m_strokeCoverageFailed) {
        cancelStroke();
        fail(tr("Mémoire insuffisante pour la couverture du trait."));
        return;
    }
    renderBrushStroke();
    if (m_strokeCoverageFailed) {
        cancelStroke();
        fail(tr("Mémoire insuffisante pour la couverture du trait."));
        return;
    }
    if ((m_tool == "colorReplacement" || m_tool == "mixerBrush" || m_tool == "healing" ||
         m_tool == "spotHealing" || (m_tool == "historyBrush" || m_tool == "artHistoryBrush")) &&
        m_strokeBounds.isEmpty())
        return;
    m_strokeChanged = true;
    changed(false);
}
void Document::stampBrush(const QPointF &_point)
{
    if (m_strokeCoverageFailed)
        return;
    if (m_tool == "artHistoryBrush") {
        artHistoryDab(_point);
        return;
    }
    if (m_tool == "spotHealing") {
        spotHealingDab(_point);
        return;
    }
    if (m_tool == "healing") {
        healingDab(_point);
        return;
    }
    if (m_tool == "smudge") {
        smudgeDab(_point);
        return;
    }
    if (m_tool == "mixerBrush") {
        mixerDab(_point);
        return;
    }
    if (m_tool == "colorReplacement") {
        colorReplacementDab(_point);
        return;
    }
    const qreal  radius = m_brushSize / 2.;
    const QRectF dabBounds(_point.x() - radius - 2, _point.y() - radius - 2, radius * 2 + 4,
                           radius * 2 + 4);
    const auto   paintDab = [&](QPainter &_painter) {
        _painter.setRenderHint(QPainter::Antialiasing, m_tool != "pencil");
        _painter.translate(_point);
        _painter.rotate(m_brushAngle);
        _painter.scale(1., m_brushRoundness);
        const QColor paint(255, 255, 255, qRound(m_brushFlow * 255));
        if (m_tool == "pencil" || m_brushHardness >= .999)
            _painter.setBrush(paint);
        else {
            QRadialGradient gradient(QPointF{}, radius);
            gradient.setColorAt(qBound(0., m_brushHardness, .999), paint);
            gradient.setColorAt(1., QColor(255, 255, 255, 0));
            _painter.setBrush(gradient);
        }
        _painter.setPen(Qt::NoPen);
        _painter.drawEllipse(QPointF{}, radius, radius);
    };
    if (!m_tiledStrokeCoverage.isNull())
        m_strokeCoverageFailed =
            !m_tiledStrokeCoverage.paintTransient(dabBounds.toAlignedRect(), paintDab);
    else {
        QPainter painter(&m_strokeCoverage);
        paintDab(painter);
    }
    m_strokeBounds = m_strokeBounds.isEmpty() ? dabBounds : m_strokeBounds.united(dabBounds);
}
void Document::renderBrushStroke()
{
    if (m_tool == "smudge" || m_tool == "colorReplacement" || m_tool == "mixerBrush" ||
        m_tool == "healing" || m_tool == "spotHealing")
        return;
    if (m_tool == "blur" || m_tool == "sharpen" || m_tool == "dodge" || m_tool == "burn" ||
        m_tool == "sponge") {
        renderRetouchStroke();
        return;
    }
    if (m_tool == "artHistoryBrush") {
        renderArtHistoryStroke();
        return;
    }
    if (m_tool == "historyBrush") {
        renderHistoryBrushStroke();
        return;
    }
    if (m_tool == "cloneStamp" || m_tool == "patternStamp") {
        renderStampStroke();
        return;
    }
    QImage *target = paintTarget();
    if (!target || m_strokeBase.isNull())
        return;
    QRect region = m_strokeBounds.toAlignedRect().intersected(QRect(QPoint{}, m_size));
    if (hasSelection())
        region = region.intersected(m_selection);
    if (region.isEmpty())
        return;
    const QImage coverage = m_tiledStrokeCoverage.isNull() ? m_strokeCoverage.copy(region)
                                                           : m_tiledStrokeCoverage.region(region);
    if (coverage.isNull()) {
        m_strokeCoverageFailed = true;
        return;
    }
    if (!m_fullCompositeDirty)
        m_dirtyRegion = m_dirtyRegion.isEmpty() ? region : m_dirtyRegion.united(region);
    QImage overlay = ImageUtils::blank(region.size(), ImageUtils::workingFormat(m_strokeBase));
    const QColor strokeColor = m_strokeInkColor.isValid() ? m_strokeInkColor : m_color;
    const QColor ink =
        m_editingMask ? maskColor(m_tool == "eraser" ? m_secondary : strokeColor) : strokeColor;
    QPainter maskPainter(&overlay);
    if (hasSelection())
        maskPainter.setClipRegion(m_selectionRegion.translated(-region.topLeft()));
    maskPainter.fillRect(overlay.rect(), ink);
    maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    maskPainter.drawImage(0, 0, coverage);
    maskPainter.end();
    SelectionCoverage::attenuate(overlay, m_selectionMask, region.topLeft());
    QImage canvas =
        m_strokeBase.copy(region).convertToFormat(ImageUtils::workingFormat(m_strokeBase));
    if ((m_tool == "brush" || m_tool == "pencil") && !m_editingMask) {
        canvas = blendImages(canvas, overlay, m_brushBlend, m_brushOpacity);
    } else {
        QPainter painter(&canvas);
        painter.setOpacity(m_brushOpacity);
        if (m_tool == "eraser" && !m_editingMask)
            painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        painter.drawImage(0, 0, overlay);
        painter.end();
    }
    if (m_editingMask) {
        const QImage gray = canvas.convertToFormat(QImage::Format_Grayscale8);
        for (int y = 0; y < region.height(); ++y)
            memcpy(target->scanLine(region.y() + y) + region.x(), gray.constScanLine(y),
                   size_t(region.width()));
    } else {
        QPainter output(target);
        output.setCompositionMode(QPainter::CompositionMode_Source);
        output.drawImage(region.topLeft(), canvas);
    }
}
