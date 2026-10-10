#include "Document.h"
#include "ImageUtils.h"

#include <QCoreApplication>
#include <QFont>
#include <QMap>
#include <QPainter>
#include <cmath>

namespace
{
bool isPaintStrokeTool(const QString &_tool)
{
    return _tool == "brush" || _tool == "pencil" || _tool == "eraser" || _tool == "blur" ||
           _tool == "sharpen" || _tool == "dodge" || _tool == "burn" || _tool == "sponge";
}

bool isShapeStrokeTool(const QString &_tool)
{
    return _tool == "move" || _tool == "rectangle" || _tool == "ellipse" ||
           _tool == "triangle" || _tool == "polygon" || _tool == "star" || _tool == "line" ||
           _tool == "gradient";
}

QString toolLabel(const QString &_tool)
{
    static const QMap<QString, const char *> labels{
        {"brush", QT_TRANSLATE_NOOP("Document", "Pinceau")},
        {"pencil", QT_TRANSLATE_NOOP("Document", "Crayon")},
        {"eraser", QT_TRANSLATE_NOOP("Document", "Gomme")},
        {"fill", QT_TRANSLATE_NOOP("Document", "Remplissage")},
        {"move", QT_TRANSLATE_NOOP("Document", "Déplacement")},
        {"select", QT_TRANSLATE_NOOP("Document", "Sélection rectangulaire")},
        {"ellipticalSelect", QT_TRANSLATE_NOOP("Document", "Sélection elliptique")},
        {"singleRowSelect", QT_TRANSLATE_NOOP("Document", "Rangée unique")},
        {"singleColumnSelect", QT_TRANSLATE_NOOP("Document", "Colonne unique")},
        {"lasso", QT_TRANSLATE_NOOP("Document", "Lasso")},
        {"polygonalLasso", QT_TRANSLATE_NOOP("Document", "Lasso polygonal")},
        {"rectangle", QT_TRANSLATE_NOOP("Document", "Rectangle")},
        {"ellipse", QT_TRANSLATE_NOOP("Document", "Ellipse")},
        {"triangle", QT_TRANSLATE_NOOP("Document", "Triangle")},
        {"polygon", QT_TRANSLATE_NOOP("Document", "Polygone")},
        {"star", QT_TRANSLATE_NOOP("Document", "Étoile")},
        {"blur", QT_TRANSLATE_NOOP("Document", "Flou")},
        {"sharpen", QT_TRANSLATE_NOOP("Document", "Netteté")},
        {"dodge", QT_TRANSLATE_NOOP("Document", "Densité moins")},
        {"burn", QT_TRANSLATE_NOOP("Document", "Densité plus")},
        {"sponge", QT_TRANSLATE_NOOP("Document", "Éponge")},
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
    if (m_tool == "hand" || m_tool == "rotateView" || m_tool == "zoomTool" ||
        m_tool == "colorSampler" || m_tool == "ruler")
        return;
    if (m_tool == "picker") {
        if (composite().rect().contains(QPoint(int(_x), int(_y))))
            setColor(composite().pixelColor(int(_x), int(_y)));
        return;
    }
    if (m_tool == "lasso") {
        beginLasso(_x, _y);
        return;
    }
    if (m_tool == "select" || m_tool == "ellipticalSelect" || m_tool == "singleRowSelect" ||
        m_tool == "singleColumnSelect" || m_tool == "crop") {
        m_drawing         = true;
        m_selection       = {};
        m_selectionRegion = {};
        m_selectionShape  = m_tool == "ellipticalSelect"     ? 1
                            : m_tool == "singleRowSelect"    ? 2
                            : m_tool == "singleColumnSelect" ? 3
                                                             : 0;
        m_selectionPath.clear();
        m_polygonLassoActive = false;
        if (m_tool == "singleRowSelect") {
            const int y       = qBound(0, int(_y), height() - 1);
            m_selection       = QRect(0, y, width(), 1);
            m_selectionRegion = QRegion(m_selection);
        } else if (m_tool == "singleColumnSelect") {
            const int x       = qBound(0, int(_x), width() - 1);
            m_selection       = QRect(x, 0, 1, height());
            m_selectionRegion = QRegion(m_selection);
        }
        emit selectionChanged();
        return;
    }
    if (!editable())
        return;
    if (m_editingMask && m_tool != "brush" && m_tool != "pencil" && m_tool != "eraser" &&
        m_tool != "fill") {
        fail(tr("Utilisez le pinceau, la gomme ou le remplissage pour modifier un masque."));
        return;
    }
    m_beforeStroke  = snapshot(toolLabel(m_tool));
    m_drawing       = true;
    m_strokeChanged = false;
    if (isPaintStrokeTool(m_tool)) {
        QImage *target = paintTarget();
        if (!target) {
            m_drawing = false;
            return;
        }
        m_strokeBase     = *target;
        m_strokeInkColor = m_color;
        if (m_tool == "pencil" && m_pencilAutoErase &&
            target->rect().contains(QPoint(int(_x), int(_y))) &&
            target->pixelColor(int(_x), int(_y)).rgba() == m_color.rgba())
            m_strokeInkColor = m_secondary;
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
                painter.setClipRegion(m_selectionRegion);
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
    if (m_tool == "lasso")
        continueLasso(point.x(), point.y());
    else if (m_tool == "select" || m_tool == "ellipticalSelect" || m_tool == "crop") {
        m_selection       = QRect(m_anchor.toPoint(), point.toPoint())
                                .normalized()
                                .intersected(QRect(QPoint{}, m_size));
        m_selectionRegion = m_tool == "ellipticalSelect" ? QRegion(m_selection, QRegion::Ellipse)
                                                         : QRegion(m_selection);
        emit selectionChanged();
    } else if (isPaintStrokeTool(m_tool))
        paintSegment(m_last, point);
    else if (isShapeStrokeTool(m_tool))
        renderShape(point);
    else
        m_drawing = false;
    m_last = point;
}
void Document::endStroke()
{
    if (!m_drawing)
        return;
    if (isPaintStrokeTool(m_tool) && QLineF(m_smoothedLast, m_last).length() > .01) {
        const qreal smoothing = m_brushSmoothing;
        m_brushSmoothing      = 0;
        paintSegment(m_smoothedLast, m_last);
        m_brushSmoothing = smoothing;
    }
    m_drawing = false;
    if (m_tool == "lasso") {
        finishLasso();
        return;
    }
    if (m_tool == "select" || m_tool == "ellipticalSelect" || m_tool == "singleRowSelect" ||
        m_tool == "singleColumnSelect" || m_tool == "crop")
        return;
    if (m_strokeChanged) {
        if (m_tool == "move" && hasSelection()) {
            const QPoint offset = (m_last - m_anchor).toPoint();
            m_selection = m_selection.translated(offset).intersected(QRect(QPoint{}, m_size));
            m_selectionRegion =
                m_selectionRegion.translated(offset) & QRegion(QRect(QPoint{}, m_size));
            m_selectionPath.translate(offset);
            emit selectionChanged();
        }
        commit(m_beforeStroke, toolLabel(m_tool), !isPaintStrokeTool(m_tool));
        if (isPaintStrokeTool(m_tool)) {
            ++m_thumbnailRevision;
            emit thumbnailsChanged();
        }
    }
    m_beforeStroke   = {};
    m_strokeBase     = {};
    m_strokeCoverage = {};
    m_strokeBounds   = {};
    m_strokeInkColor = {};
}
void Document::cancelStroke()
{
    if (!m_drawing)
        return;
    m_drawing = false;
    if (m_tool == "lasso") {
        deselect();
        return;
    }
    if (m_tool == "select" || m_tool == "ellipticalSelect" || m_tool == "singleRowSelect" ||
        m_tool == "singleColumnSelect" || m_tool == "crop")
        deselect();
    else
        restore(m_beforeStroke);
    m_beforeStroke   = {};
    m_strokeBase     = {};
    m_strokeCoverage = {};
    m_strokeBounds   = {};
    m_strokeInkColor = {};
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
