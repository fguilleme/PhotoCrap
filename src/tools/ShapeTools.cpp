#include "Document.h"
#include "ImageUtils.h"
#include "text/TextLayer.h"

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
    if (m_active < 0 || m_active >= m_layers.size()) {
        m_drawing       = false;
        m_strokeChanged = false;
        return;
    }
    if (m_tool == "move" && m_layers[m_active].kind == "text") {
        if (_to == m_anchor)
            return;
        if (m_moveCopy && !m_moveDuplicatedLayer) {
            auto layer      = m_layers[m_active];
            layer.historyId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            layer.name += tr(" — copie");
            m_layers.insert(++m_active, layer);
            selectOnly(m_active);
            m_moveCopy            = false;
            m_moveDuplicatedLayer = true;
        }
        auto       parameters = m_textMoveParameters;
        const auto delta      = _to - m_anchor;
        PhotoCrap::Text::transform(parameters, QTransform::fromTranslate(delta.x(), delta.y()));
        auto image = PhotoCrap::Text::render(parameters, m_size, workingFormat());
        if (image.isNull()) {
            cancelStroke();
            fail(tr("Mémoire insuffisante pour rendre le texte."));
            return;
        }
        m_layers[m_active].parameters = parameters;
        m_layers[m_active].image      = image;
        m_strokeChanged               = true;
        m_fullCompositeDirty          = true;
        changed(false);
        return;
    }
    const bool geometric = m_tool != "move" && m_tool != "gradient";
    const bool regional  = geometric || (m_tool == "gradient" && hasSelection());
    const auto draw      = [&](QPainter &painter) {
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
        QRectF rect = toolRectangle(m_anchor, _to, m_toolAlternate);
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
    };
    if (regional) {
        const QRect previous = m_strokeBounds.toAlignedRect();
        QRect       bounds;
        PixelPlane  next = m_strokePlaneBase;
        if (!geometric || _to != m_anchor) {
            const QPen pen(shapeStrokeColor(), m_shapeStrokeWidth, Qt::SolidLine, Qt::SquareCap,
                           Qt::MiterJoin);
            // Qt measures the miter limit in pen widths. Include the join and
            // antialiasing fringe, even for narrow stars with a thick outline.
            const int margin = m_shapeStrokeEnabled
                                   ? qCeil(qMax(qreal(1), pen.widthF()) * pen.miterLimit()) + 4
                                   : 4;
            bounds           = geometric ? toolRectangle(m_anchor, _to, m_toolAlternate)
                                               .toAlignedRect()
                                               .adjusted(-margin, -margin, margin, margin)
                                         : QRect(QPoint{}, m_size);
            bounds           = bounds.intersected(QRect(QPoint{}, m_size));
            if (hasSelection())
                bounds = bounds.intersected(m_selectionRegion.boundingRect());
            if (!bounds.isEmpty()) {
                const QImage before = m_strokePlaneBase.region(bounds);
                QImage       image  = before.copy();
                if (image.isNull()) {
                    cancelStroke();
                    fail(tr("Mémoire insuffisante pour dessiner la forme."));
                    return;
                }
                QPainter painter(&image);
                painter.translate(-bounds.topLeft());
                draw(painter);
                painter.end();
                SelectionCoverage::blend(image, before,
                                         m_selectionMask.isNull() ? QImage{}
                                                                  : m_selectionMask.copy(bounds),
                                         image.rect());
                if (!next.writeRegion(bounds.topLeft(), image, m_size)) {
                    cancelStroke();
                    fail(tr("Mémoire insuffisante pour dessiner la forme."));
                    return;
                }
            }
        }
        m_layers[m_active].image = std::move(next);
        m_strokeBounds           = bounds;
        m_strokeChanged = m_layers[m_active].image.cacheKey() != m_strokePlaneBase.cacheKey();
        m_dirtyRegion   = m_dirtyRegion.united(previous.united(bounds));
        changed(false);
        return;
    }
    const QImage sourceImage = ImageUtils::expandedPlane(m_strokeBase, m_size);
    if (m_tool == "move" && m_moveCopy && !hasSelection() && !m_moveDuplicatedLayer &&
        _to != m_anchor) {
        auto layer = m_layers[m_active];
        layer.name += tr(" — copie");
        layer.historyId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_layers.insert(++m_active, layer);
        selectOnly(m_active);
        m_moveCopy            = false;
        m_moveDuplicatedLayer = true;
    }
    m_layers[m_active].image = sourceImage;
    if (m_tool == "move") {
        const QImage source = sourceImage;
        QImage       result =
            hasSelection() || m_moveCopy ? source : ImageUtils::blank(m_size, workingFormat());
        QPainter     painter(&result);
        const QPoint offset = (_to - m_anchor).toPoint();
        if (hasSelection()) {
            QImage   selected = ImageUtils::blank(m_selection.size(), workingFormat());
            QPainter selectedPainter(&selected);
            selectedPainter.setClipRegion(m_selectionRegion.translated(-m_selection.topLeft()));
            selectedPainter.drawImage(-m_selection.topLeft(), source);
            selectedPainter.end();
            SelectionCoverage::attenuate(selected, m_selectionMask, m_selection.topLeft());
            if (!m_moveCopy) {
                painter.setClipRegion(m_selectionRegion);
                if (m_selectionMask.isNull()) {
                    painter.setCompositionMode(QPainter::CompositionMode_Clear);
                    painter.fillRect(QRect(QPoint{}, m_size), Qt::transparent);
                } else {
                    QImage removal(m_selection.size(), workingFormat());
                    removal.fill(Qt::white);
                    SelectionCoverage::attenuate(removal, m_selectionMask, m_selection.topLeft());
                    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
                    painter.drawImage(m_selection.topLeft(), removal);
                }
                painter.setClipping(false);
            }
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.drawImage(m_selection.topLeft() + offset, selected);
        } else
            painter.drawImage(offset, source);
        painter.end();
        m_layers[m_active].image = result;
    } else {
        QPainter painter(&m_layers[m_active].image.edit());
        draw(painter);
        painter.end();
        SelectionCoverage::blend(m_layers[m_active].image.edit(), sourceImage, m_selectionMask,
                                 QRect(QPoint{}, m_size));
    }
    m_strokeChanged      = true;
    m_fullCompositeDirty = true;
    changed(false);
}
