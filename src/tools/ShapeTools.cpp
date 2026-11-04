#include "Document.h"
#include "ImageUtils.h"
#include "SelectionMove.h"
#include "shapes/ShapeLayer.h"
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
    if (m_tool == "move" &&
        (m_layers[m_active].kind == "text" || m_layers[m_active].kind == "shape")) {
        if (m_moveCopy && !m_moveDuplicatedLayer && _to != m_anchor) {
            auto layer      = m_layers[m_active];
            layer.historyId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            layer.name += tr(" — copie");
            m_layers.insert(++m_active, layer);
            selectOnly(m_active);
            m_moveCopy            = false;
            m_moveDuplicatedLayer = true;
        }
        auto       parameters  = m_textMoveParameters;
        const auto delta       = _to - m_anchor;
        const bool shape       = m_layers[m_active].kind == "shape";
        const auto translation = QTransform::fromTranslate(delta.x(), delta.y());
        if (shape) {
            if (!PhotoCrap::Shape::transform(parameters, translation)) {
                cancelStroke();
                fail(tr("Transformation des tracés impossible."));
                return;
            }
        } else
            PhotoCrap::Text::transform(parameters, translation);
        if (parameters == m_layers[m_active].parameters)
            return;
        auto image = parameters == m_textMoveParameters ? m_strokePlaneBase
                     : shape ? PhotoCrap::Shape::renderPlane(parameters, m_size, workingFormat())
                             : PhotoCrap::Text::renderPlane(parameters, m_size, workingFormat());
        if (image.isNull()) {
            cancelStroke();
            fail(tr("Mémoire insuffisante pour rendre le texte."));
            return;
        }
        const auto oldBounds = shape
                                   ? PhotoCrap::Shape::bounds(m_layers[m_active].parameters, m_size)
                                   : PhotoCrap::Text::bounds(m_layers[m_active].parameters, m_size);
        const auto newBounds = shape ? PhotoCrap::Shape::bounds(parameters, m_size)
                                     : PhotoCrap::Text::bounds(parameters, m_size);
        m_dirtyRegion        = m_dirtyRegion.united(oldBounds).united(newBounds);
        m_layers[m_active].parameters = parameters;
        m_layers[m_active].image      = image;
        m_strokeChanged               = parameters != m_textMoveParameters || m_moveDuplicatedLayer;
        changed(false);
        if (shape)
            syncPenShapePath();
        return;
    }
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
    if (m_tool == "move" && !hasSelection() && !m_strokePlaneBase.isNull()) {
        const auto result =
            PhotoCrap::Tools::moveLayerTiled(m_strokePlaneBase, m_size, (_to - m_anchor).toPoint());
        if (!result.valid) {
            cancelStroke();
            fail(tr("Mémoire insuffisante pour déplacer le calque."));
            return;
        }
        m_layers[m_active].image = result.image;
        m_dirtyRegion  = m_dirtyRegion.united(m_strokeBounds.toAlignedRect().united(result.region));
        m_strokeBounds = result.region;
        m_strokeChanged =
            result.image.cacheKey() != m_strokePlaneBase.cacheKey() || m_moveDuplicatedLayer;
        changed(false);
        return;
    }
    if (m_tool == "move" && hasSelection()) {
        const auto result = PhotoCrap::Tools::moveSelection(m_strokePlaneBase, m_size, m_selection,
                                                            m_selectionRegion, m_selectionMask,
                                                            (_to - m_anchor).toPoint(), m_moveCopy);
        if (!result.valid) {
            cancelStroke();
            fail(tr("Mémoire insuffisante pour déplacer la sélection."));
            return;
        }
        m_layers[m_active].image = result.image;
        m_dirtyRegion  = m_dirtyRegion.united(m_strokeBounds.toAlignedRect().united(result.region));
        m_strokeBounds = result.region;
        // The selection itself moves even when its pixels are transparent or
        // copied entirely outside the canvas. Preserve its undo step.
        m_strokeChanged = true;
        changed(false);
        return;
    }
    const bool geometric = m_tool != "move" && m_tool != "gradient";
    const bool regional  = geometric || (m_tool == "gradient" && hasSelection());
    const auto draw      = [&](QPainter &_painter) {
        _painter.setRenderHint(QPainter::Antialiasing);
        if (hasSelection())
            _painter.setClipRegion(m_selectionRegion);
        _painter.setOpacity(m_brushOpacity);
        if (m_tool == "rectangle" || m_tool == "ellipse" || m_tool == "triangle" ||
            m_tool == "polygon" || m_tool == "star" || m_tool == "line") {
            const auto penStyle = m_shapeStrokeStyle == 1   ? Qt::DashLine
                                  : m_shapeStrokeStyle == 2 ? Qt::DotLine
                                                            : Qt::SolidLine;
            _painter.setPen(m_shapeStrokeEnabled ? QPen(shapeStrokeColor(), m_shapeStrokeWidth,
                                                        penStyle, Qt::SquareCap, Qt::MiterJoin)
                                                 : QPen(Qt::NoPen));
            _painter.setBrush(m_filledShapes ? QBrush(shapeFillColor()) : Qt::NoBrush);
        } else
            _painter.setPen(QPen(m_color, m_brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QRectF rect = toolRectangle(m_anchor, _to, m_toolAlternate);
        if (m_shapeAlignPixels &&
            (m_tool == "rectangle" || m_tool == "ellipse" || m_tool == "triangle" ||
             m_tool == "polygon" || m_tool == "star" || m_tool == "line"))
            rect = QRectF(qRound(rect.left()), qRound(rect.top()), qRound(rect.width()),
                          qRound(rect.height()));
        if (m_tool == "rectangle") {
            const qreal radius = qMin(m_shapeCornerRadius, qMin(rect.width(), rect.height()) / 2.);
            _painter.drawRoundedRect(rect, radius, radius);
        } else if (m_tool == "ellipse")
            _painter.drawEllipse(rect);
        else if (m_tool == "triangle")
            _painter.drawPolygon(regularPolygon(rect, 3));
        else if (m_tool == "polygon")
            _painter.drawPolygon(regularPolygon(rect, m_shapeSides));
        else if (m_tool == "star")
            _painter.drawPolygon(regularPolygon(rect, m_shapeSides, m_shapeStarRatio));
        else if (m_tool == "line") {
            QPointF from = m_anchor;
            QPointF to   = _to;
            if (m_shapeAlignPixels) {
                from = QPointF(qRound(from.x()), qRound(from.y()));
                to   = QPointF(qRound(to.x()), qRound(to.y()));
            }
            _painter.drawLine(from, to);
        } else if (m_tool == "gradient") {
            QLinearGradient gradient(m_anchor, _to);
            gradient.setColorAt(0, m_color);
            gradient.setColorAt(1, m_secondary);
            _painter.fillRect(QRect(QPoint{}, m_size), gradient);
        }
    };
    if (m_tool == "gradient" && !hasSelection() && !m_strokePlaneBase.isNull()) {
        // Preserve global gradient coordinates, while bounding native scratch
        // memory to two full-width bands rather than two document-sized images.
        PixelPlane next  = m_strokePlaneBase;
        const int  bytes = workingFormat() == QImage::Format_RGBA64_Premultiplied ? 8 : 4;
        const int  rows  = qMax(1, qMin(256, (4 * 1024 * 1024) / (width() * bytes)));
        for (int top = 0; top < height(); top += rows) {
            const QRect bounds(0, top, width(), qMin(rows, height() - top));
            QImage      image = m_strokePlaneBase.region(bounds);
            if (image.isNull()) {
                cancelStroke();
                fail(tr("Mémoire insuffisante pour dessiner la forme."));
                return;
            }
            QPainter painter(&image);
            painter.translate(0, -top);
            draw(painter);
            painter.end();
            if (!next.writeRegion(bounds.topLeft(), image, m_size)) {
                cancelStroke();
                fail(tr("Mémoire insuffisante pour dessiner la forme."));
                return;
            }
        }
        m_layers[m_active].image = std::move(next);
        m_strokeChanged = m_layers[m_active].image.cacheKey() != m_strokePlaneBase.cacheKey();
        m_dirtyRegion   = m_dirtyRegion.united(QRect(QPoint{}, m_size));
        changed(false);
        return;
    }
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
    m_layers[m_active].image = sourceImage;
    if (m_tool == "move") {
        QImage   result = m_moveCopy ? sourceImage : ImageUtils::blank(m_size, workingFormat());
        QPainter painter(&result);
        painter.drawImage((_to - m_anchor).toPoint(), sourceImage);
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
