#include "Document.h"
#include "ImageUtils.h"

#include <QPainter>
#include <QtMath>

void Document::renderShape(const QPointF &_to)
{
    m_layers[m_active].image = m_beforeStroke.layers[m_active].image;
    if (m_tool == "move") {
        const QImage source = m_beforeStroke.layers[m_active].image;
        QImage       result = hasSelection() ? source : ImageUtils::blank(m_size);
        QPainter     painter(&result);
        const QPoint offset = (_to - m_anchor).toPoint();
        if (hasSelection()) {
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.fillRect(m_selection, Qt::transparent);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.drawImage(m_selection.topLeft() + offset, source.copy(m_selection));
        } else
            painter.drawImage(offset, source);
        painter.end();
        m_layers[m_active].image = result;
    } else {
        QPainter painter(&m_layers[m_active].image);
        painter.setRenderHint(QPainter::Antialiasing);
        if (hasSelection())
            painter.setClipRect(m_selection);
        painter.setOpacity(m_brushOpacity);
        if (m_tool == "rectangle" || m_tool == "ellipse" || m_tool == "line") {
            const auto penStyle = m_shapeStrokeStyle == 1   ? Qt::DashLine
                                  : m_shapeStrokeStyle == 2 ? Qt::DotLine
                                                            : Qt::SolidLine;
            painter.setPen(m_shapeStrokeEnabled ? QPen(shapeStrokeColor(), m_shapeStrokeWidth,
                                                       penStyle, Qt::SquareCap, Qt::MiterJoin)
                                                : QPen(Qt::NoPen));
            painter.setBrush(m_filledShapes ? QBrush(shapeFillColor()) : Qt::NoBrush);
        } else
            painter.setPen(QPen(m_color, m_brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QRectF rect = QRectF(m_anchor, _to).normalized();
        if (m_shapeAlignPixels &&
            (m_tool == "rectangle" || m_tool == "ellipse" || m_tool == "line"))
            rect = QRectF(qRound(rect.left()), qRound(rect.top()), qRound(rect.width()),
                          qRound(rect.height()));
        if (m_tool == "rectangle") {
            const qreal radius = qMin(m_shapeCornerRadius, qMin(rect.width(), rect.height()) / 2.);
            painter.drawRoundedRect(rect, radius, radius);
        } else if (m_tool == "ellipse")
            painter.drawEllipse(rect);
        else if (m_tool == "line") {
            QPointF from = m_anchor;
            QPointF to   = _to;
            if (m_shapeAlignPixels) {
                from = QPointF(qRound(from.x()), qRound(from.y()));
                to   = QPointF(qRound(to.x()), qRound(to.y()));
            }
            painter.drawLine(from, to);
        } else if (m_tool == "gradient") {
            QLinearGradient gradient(m_anchor, _to);
            gradient.setColorAt(0, m_color);
            gradient.setColorAt(1, m_secondary);
            painter.fillRect(QRect(QPoint{}, m_size), gradient);
        }
    }
    m_strokeChanged      = true;
    m_fullCompositeDirty = true;
    changed(false);
}
