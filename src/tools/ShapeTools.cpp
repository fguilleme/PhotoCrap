#include "Document.h"
#include "ImageUtils.h"

#include <QPainter>
#include <QPolygonF>
#include <QtMath>
#include <cmath>

namespace
{
QPolygonF regularPolygon(const QRectF &_bounds, int _points, qreal _innerRatio = 1.)
{
    QPolygonF     result;
    const QPointF center   = _bounds.center();
    const qreal   radiusX  = _bounds.width() / 2.;
    const qreal   radiusY  = _bounds.height() / 2.;
    const int     vertices = _innerRatio < 1. ? _points * 2 : _points;
    for (int i = 0; i < vertices; ++i) {
        const qreal radius = (_innerRatio < 1. && i % 2) ? _innerRatio : 1.;
        const qreal angle  = -M_PI_2 + qreal(i) * 2. * M_PI / qreal(vertices);
        result.append(center + QPointF(std::cos(angle) * radiusX * radius,
                                       std::sin(angle) * radiusY * radius));
    }
    return result;
}
} // namespace

void Document::renderShape(const QPointF &_to)
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_active >= m_beforeStroke.layers.size()) {
        m_drawing       = false;
        m_strokeChanged = false;
        return;
    }
    const QImage sourceImage = m_beforeStroke.layers.at(m_active).image;
    m_layers[m_active].image = sourceImage;
    if (m_tool == "move") {
        const QImage source = sourceImage;
        QImage       result = hasSelection() ? source : ImageUtils::blank(m_size);
        QPainter     painter(&result);
        const QPoint offset = (_to - m_anchor).toPoint();
        if (hasSelection()) {
            QImage   selected = ImageUtils::blank(m_selection.size());
            QPainter selectedPainter(&selected);
            selectedPainter.setClipRegion(m_selectionRegion.translated(-m_selection.topLeft()));
            selectedPainter.drawImage(-m_selection.topLeft(), source);
            selectedPainter.end();
            painter.setClipRegion(m_selectionRegion);
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.fillRect(QRect(QPoint{}, m_size), Qt::transparent);
            painter.setClipping(false);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.drawImage(m_selection.topLeft() + offset, selected);
        } else
            painter.drawImage(offset, source);
        painter.end();
        m_layers[m_active].image = result;
    } else {
        QPainter painter(&m_layers[m_active].image);
        painter.setRenderHint(QPainter::Antialiasing);
        if (hasSelection())
            painter.setClipRegion(m_selectionRegion);
        painter.setOpacity(m_brushOpacity);
        if (m_tool == "rectangle" || m_tool == "ellipse" || m_tool == "triangle" ||
            m_tool == "polygon" || m_tool == "star" || m_tool == "line") {
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
            (m_tool == "rectangle" || m_tool == "ellipse" || m_tool == "triangle" ||
             m_tool == "polygon" || m_tool == "star" || m_tool == "line"))
            rect = QRectF(qRound(rect.left()), qRound(rect.top()), qRound(rect.width()),
                          qRound(rect.height()));
        if (m_tool == "rectangle") {
            const qreal radius = qMin(m_shapeCornerRadius, qMin(rect.width(), rect.height()) / 2.);
            painter.drawRoundedRect(rect, radius, radius);
        } else if (m_tool == "ellipse")
            painter.drawEllipse(rect);
        else if (m_tool == "triangle")
            painter.drawPolygon(regularPolygon(rect, 3));
        else if (m_tool == "polygon")
            painter.drawPolygon(regularPolygon(rect, m_shapeSides));
        else if (m_tool == "star")
            painter.drawPolygon(regularPolygon(rect, m_shapeSides, m_shapeStarRatio));
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
