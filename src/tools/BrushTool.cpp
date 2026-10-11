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
        m_strokeCoverage.isNull())
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
    renderBrushStroke();
    if ((m_tool == "colorReplacement" || m_tool == "mixerBrush" || m_tool == "healing") &&
        m_strokeBounds.isEmpty())
        return;
    m_strokeChanged = true;
    changed(false);
}
void Document::stampBrush(const QPointF &_point)
{
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
    QPainter painter(&m_strokeCoverage);
    painter.setRenderHint(QPainter::Antialiasing, m_tool != "pencil");
    painter.translate(_point);
    painter.rotate(m_brushAngle);
    painter.scale(1., m_brushRoundness);
    const qreal  radius = m_brushSize / 2.;
    const QColor paint(255, 255, 255, qRound(m_brushFlow * 255));
    if (m_tool == "pencil" || m_brushHardness >= .999)
        painter.setBrush(paint);
    else {
        QRadialGradient gradient(QPointF{}, radius);
        gradient.setColorAt(qBound(0., m_brushHardness, .999), paint);
        gradient.setColorAt(1., QColor(255, 255, 255, 0));
        painter.setBrush(gradient);
    }
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF{}, radius, radius);
    painter.end();
    const QRectF dabBounds(_point.x() - radius - 2, _point.y() - radius - 2, radius * 2 + 4,
                           radius * 2 + 4);
    m_strokeBounds = m_strokeBounds.isEmpty() ? dabBounds : m_strokeBounds.united(dabBounds);
}
void Document::renderBrushStroke()
{
    if (m_tool == "smudge" || m_tool == "colorReplacement" || m_tool == "mixerBrush" ||
        m_tool == "healing")
        return;
    if (m_tool == "blur" || m_tool == "sharpen" || m_tool == "dodge" || m_tool == "burn" ||
        m_tool == "sponge") {
        renderRetouchStroke();
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
    if (!m_fullCompositeDirty)
        m_dirtyRegion = m_dirtyRegion.isEmpty() ? region : m_dirtyRegion.united(region);
    QImage       overlay     = ImageUtils::blank(region.size());
    const QColor strokeColor = m_strokeInkColor.isValid() ? m_strokeInkColor : m_color;
    const QColor ink =
        m_editingMask ? maskColor(m_tool == "eraser" ? m_secondary : strokeColor) : strokeColor;
    QPainter maskPainter(&overlay);
    if (hasSelection())
        maskPainter.setClipRegion(m_selectionRegion.translated(-region.topLeft()));
    maskPainter.fillRect(overlay.rect(), ink);
    maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    maskPainter.drawImage(0, 0, m_strokeCoverage.copy(region));
    maskPainter.end();
    QImage canvas = m_strokeBase.copy(region).convertToFormat(QImage::Format_ARGB32_Premultiplied);
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
