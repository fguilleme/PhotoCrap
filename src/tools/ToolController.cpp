#include "Document.h"
#include "ImageUtils.h"

#include <QCoreApplication>
#include <QFont>
#include <QMap>
#include <QPainter>
#include <cmath>

namespace
{
QString toolLabel(const QString &_tool)
{
    static const QMap<QString, const char *> labels{
        {"brush", QT_TRANSLATE_NOOP("Document", "Pinceau")},
        {"eraser", QT_TRANSLATE_NOOP("Document", "Gomme")},
        {"fill", QT_TRANSLATE_NOOP("Document", "Remplissage")},
        {"move", QT_TRANSLATE_NOOP("Document", "Déplacement")},
        {"rectangle", QT_TRANSLATE_NOOP("Document", "Rectangle")},
        {"ellipse", QT_TRANSLATE_NOOP("Document", "Ellipse")},
        {"line", QT_TRANSLATE_NOOP("Document", "Ligne")},
        {"gradient", QT_TRANSLATE_NOOP("Document", "Dégradé")},
        {"text", QT_TRANSLATE_NOOP("Document", "Texte")}};
    return labels.contains(_tool) ? QCoreApplication::translate("Document", labels.value(_tool))
                                  : _tool;
}
} // namespace

void Document::beginStroke(qreal _x, qreal _y)
{
    if (!std::isfinite(_x) || !std::isfinite(_y))
        return;
    endStroke();
    m_anchor = m_last = {_x, _y};
    if (m_tool == "hand")
        return;
    if (m_tool == "picker") {
        if (composite().rect().contains(QPoint(int(_x), int(_y))))
            setColor(composite().pixelColor(int(_x), int(_y)));
        return;
    }
    if (m_tool == "select" || m_tool == "crop") {
        m_drawing   = true;
        m_selection = {};
        emit selectionChanged();
        return;
    }
    if (!editable())
        return;
    if (m_editingMask && m_tool != "brush" && m_tool != "eraser" && m_tool != "fill") {
        fail(tr("Utilisez le pinceau, la gomme ou le remplissage pour modifier un masque."));
        return;
    }
    m_beforeStroke  = snapshot(toolLabel(m_tool));
    m_drawing       = true;
    m_strokeChanged = false;
    if (m_tool == "brush" || m_tool == "eraser") {
        QImage *target = paintTarget();
        if (!target) {
            m_drawing = false;
            return;
        }
        m_strokeBase        = *target;
        m_strokeCoverage    = ImageUtils::blank(m_size);
        m_strokeBounds      = {};
        m_smoothedLast      = m_anchor;
        m_distanceToNextDab = qMax(1., m_brushSize * m_brushSpacing);
        paintSegment(m_anchor, m_anchor);
    } else if (m_tool == "fill") {
        floodFill(QPoint(int(_x), int(_y)));
        endStroke();
    } else if (m_tool == "text") {
        if (!m_text.isEmpty()) {
            QPainter painter(&m_layers[m_active].image);
            painter.setRenderHint(QPainter::TextAntialiasing);
            if (hasSelection())
                painter.setClipRect(m_selection);
            QFont font("sans-serif");
            font.setPixelSize(m_textSize);
            font.setWeight(QFont::DemiBold);
            painter.setFont(font);
            painter.setPen(m_color);
            painter.setOpacity(m_brushOpacity);
            painter.drawText(m_anchor + QPointF(0, m_textSize), m_text);
            painter.end();
            m_strokeChanged = true;
        }
        endStroke();
    }
}
void Document::continueStroke(qreal _x, qreal _y)
{
    if (!m_drawing || !std::isfinite(_x) || !std::isfinite(_y))
        return;
    const QPointF point(qBound(-32768., _x, 32768.), qBound(-32768., _y, 32768.));
    if (m_tool == "select" || m_tool == "crop") {
        m_selection = QRect(m_anchor.toPoint(), point.toPoint())
                          .normalized()
                          .intersected(QRect(QPoint{}, m_size));
        emit selectionChanged();
    } else if (m_tool == "brush" || m_tool == "eraser")
        paintSegment(m_last, point);
    else
        renderShape(point);
    m_last = point;
}
void Document::endStroke()
{
    if (!m_drawing)
        return;
    if ((m_tool == "brush" || m_tool == "eraser") &&
        QLineF(m_smoothedLast, m_last).length() > .01) {
        const qreal smoothing = m_brushSmoothing;
        m_brushSmoothing      = 0;
        paintSegment(m_smoothedLast, m_last);
        m_brushSmoothing = smoothing;
    }
    m_drawing = false;
    if (m_tool == "select" || m_tool == "crop")
        return;
    if (m_strokeChanged) {
        if (m_tool == "move" && hasSelection()) {
            m_selection = m_selection.translated((m_last - m_anchor).toPoint())
                              .intersected(QRect(QPoint{}, m_size));
            emit selectionChanged();
        }
        commit(m_beforeStroke, toolLabel(m_tool), m_tool != "brush" && m_tool != "eraser");
        if (m_tool == "brush" || m_tool == "eraser") {
            ++m_thumbnailRevision;
            emit thumbnailsChanged();
        }
    }
    m_beforeStroke   = {};
    m_strokeBase     = {};
    m_strokeCoverage = {};
    m_strokeBounds   = {};
}
void Document::cancelStroke()
{
    if (!m_drawing)
        return;
    m_drawing = false;
    if (m_tool == "select" || m_tool == "crop")
        deselect();
    else
        restore(m_beforeStroke);
    m_beforeStroke   = {};
    m_strokeBase     = {};
    m_strokeCoverage = {};
    m_strokeBounds   = {};
}
QImage *Document::paintTarget()
{
    if (m_active < 0 || m_active >= m_layers.size())
        return nullptr;
    if (m_editingMask && !m_layers[m_active].mask.isNull())
        return &m_layers[m_active].mask;
    if (m_layers[m_active].kind == "pixel")
        return &m_layers[m_active].image;
    return nullptr;
}
