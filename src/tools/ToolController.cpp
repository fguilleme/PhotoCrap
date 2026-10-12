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
           _tool == "sharpen" || _tool == "dodge" || _tool == "burn" || _tool == "sponge" ||
           _tool == "cloneStamp" || _tool == "patternStamp" || _tool == "smudge" ||
           _tool == "colorReplacement" || _tool == "mixerBrush" || _tool == "healing" ||
           _tool == "spotHealing" || (_tool == "historyBrush" || _tool == "artHistoryBrush");
}

bool isShapeStrokeTool(const QString &_tool)
{
    return _tool == "move" || _tool == "rectangle" || _tool == "ellipse" || _tool == "triangle" ||
           _tool == "polygon" || _tool == "star" || _tool == "line" || _tool == "gradient";
}

QString toolLabel(const QString &_tool)
{
    static const QMap<QString, const char *> labels{
        {"brush", QT_TRANSLATE_NOOP("Document", "Pinceau")},
        {"pencil", QT_TRANSLATE_NOOP("Document", "Crayon")},
        {"colorReplacement", QT_TRANSLATE_NOOP("Document", "Remplacement de couleur")},
        {"mixerBrush", QT_TRANSLATE_NOOP("Document", "Pinceau mélangeur")},
        {"redEye", QT_TRANSLATE_NOOP("Document", "Œil rouge")},
        {"healing", QT_TRANSLATE_NOOP("Document", "Correcteur")},
        {"spotHealing", QT_TRANSLATE_NOOP("Document", "Correcteur localisé")},
        {"historyBrush", QT_TRANSLATE_NOOP("Document", "Forme d’historique")},
        {"artHistoryBrush", QT_TRANSLATE_NOOP("Document", "Forme d’historique artistique")},
        {"patch", QT_TRANSLATE_NOOP("Document", "Pièce")},
        {"contentAwareMove", QT_TRANSLATE_NOOP("Document", "Déplacement basé sur le contenu")},
        {"cloneStamp", QT_TRANSLATE_NOOP("Document", "Tampon de duplication")},
        {"patternStamp", QT_TRANSLATE_NOOP("Document", "Tampon de motif")},
        {"eraser", QT_TRANSLATE_NOOP("Document", "Gomme")},
        {"fill", QT_TRANSLATE_NOOP("Document", "Remplissage")},
        {"move", QT_TRANSLATE_NOOP("Document", "Déplacement")},
        {"select", QT_TRANSLATE_NOOP("Document", "Sélection rectangulaire")},
        {"ellipticalSelect", QT_TRANSLATE_NOOP("Document", "Sélection elliptique")},
        {"singleRowSelect", QT_TRANSLATE_NOOP("Document", "Rangée unique")},
        {"singleColumnSelect", QT_TRANSLATE_NOOP("Document", "Colonne unique")},
        {"lasso", QT_TRANSLATE_NOOP("Document", "Lasso")},
        {"polygonalLasso", QT_TRANSLATE_NOOP("Document", "Lasso polygonal")},
        {"quickSelection", QT_TRANSLATE_NOOP("Document", "Sélection rapide")},
        {"magicWand", QT_TRANSLATE_NOOP("Document", "Baguette magique")},
        {"rectangle", QT_TRANSLATE_NOOP("Document", "Rectangle")},
        {"ellipse", QT_TRANSLATE_NOOP("Document", "Ellipse")},
        {"triangle", QT_TRANSLATE_NOOP("Document", "Triangle")},
        {"polygon", QT_TRANSLATE_NOOP("Document", "Polygone")},
        {"star", QT_TRANSLATE_NOOP("Document", "Étoile")},
        {"blur", QT_TRANSLATE_NOOP("Document", "Flou")},
        {"sharpen", QT_TRANSLATE_NOOP("Document", "Netteté")},
        {"smudge", QT_TRANSLATE_NOOP("Document", "Doigt")},
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

void Document::beginStroke(qreal _x, qreal _y, bool _alternate, bool _straight)
{
    if (!std::isfinite(_x) || !std::isfinite(_y))
        return;
    endStroke();
    if (!toolSupportsDepth(m_tool)) {
        fail(tr("Cet outil nécessite actuellement un document 8 bits. Changez la profondeur dans "
                "le menu Image."));
        return;
    }
    invalidateContentMove();
    if (m_tool != "remove")
        cancelRemoval();
    m_toolAlternate             = _alternate;
    m_toolShift                 = _straight;
    m_toolSpace                 = false;
    m_selectionInitialAlternate = _alternate;
    m_selectionMoving           = false;
    m_moveDuplicatedLayer       = false;
    m_moveCopy                  = m_tool == "move" && _alternate;
    m_strokeConnect             = false;
    m_geometryPointer           = QPointF(_x, _y);
    m_anchor = m_last = (m_tool == "redEye" || m_tool == "healing" || m_tool == "spotHealing" ||
                         m_tool == "remove" || m_tool == "patch" || m_tool == "contentAwareMove")
                            ? QPointF(qBound(-32768., _x, 32768.), qBound(-32768., _y, 32768.))
                            : QPointF(_x, _y);
    if (m_tool == "slice" || m_tool == "sliceSelect") {
        beginSliceGesture(_x, _y);
        return;
    }
    if (m_tool == "crop" || m_tool == "perspectiveCrop") {
        beginCropGesture(_x, _y);
        return;
    }
    if (m_tool == "hand" || m_tool == "rotateView" || m_tool == "zoomTool" ||
        m_tool == "colorSampler" || m_tool == "ruler")
        return;
    if (m_tool == "picker" || (m_tool == "colorReplacement" && _alternate)) {
        pickToolColor(int(std::floor(_x)), int(std::floor(_y)), m_tool == "picker" && _alternate);
        return;
    }
    if (m_tool == "healing" && _alternate) {
        setHealingSource(_x, _y);
        return;
    }
    if (m_tool == "mixerBrush" && _alternate) {
        sampleMixerBrush(_x, _y);
        return;
    }
    if (m_tool == "magicWand") {
        applyMagicWand(QPoint(qRound(_x), qRound(_y)));
        return;
    }
    if (m_tool == "quickSelection") {
        beginQuickSelection(m_anchor);
        return;
    }
    if (m_tool == "remove") {
        beginRemove(m_anchor, _alternate);
        return;
    }
    if (m_tool == "contentAwareMove") {
        beginContentMove(m_anchor);
        return;
    }
    if (m_tool == "patch") {
        beginPatch(m_anchor);
        return;
    }
    if (m_tool == "lasso") {
        beginLasso(_x, _y);
        return;
    }
    if (m_tool == "select" || m_tool == "ellipticalSelect" || m_tool == "singleRowSelect" ||
        m_tool == "singleColumnSelect") {
        rememberSelection();
        m_selectionMoving = m_selectionMode == 0 && !_alternate && !_straight &&
                            m_selectionRegion.contains(QPoint(qRound(_x), qRound(_y)));
        m_drawing         = true;
        if (m_selectionMoving)
            return;
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
        m_tool != "fill" && m_tool != "cloneStamp" && m_tool != "patternStamp" &&
        m_tool != "smudge" && m_tool != "mixerBrush") {
        fail(tr("Utilisez le pinceau, la gomme ou le remplissage pour modifier un masque."));
        return;
    }
    if (m_moveCopy && !hasSelection() && !validSize(width(), height(), int(m_layers.size()) + 1))
        return;
    m_beforeStroke = snapshot(toolLabel(m_tool));
    if (m_tool == "move")
        m_strokeBase = m_layers[m_active].image;
    m_drawing       = true;
    m_strokeChanged = false;
    if (m_tool == "redEye") {
        m_redEyeRegion = {};
        emit redEyeRegionChanged();
    } else if (isPaintStrokeTool(m_tool)) {
        QImage *target = paintTarget();
        if (!target) {
            m_drawing = false;
            return;
        }
        if ((m_tool == "cloneStamp" || m_tool == "patternStamp") && !prepareStampStroke()) {
            m_drawing      = false;
            m_beforeStroke = {};
            return;
        }
        if (m_tool == "healing" && !prepareHealingStroke()) {
            m_drawing      = false;
            m_beforeStroke = {};
            return;
        }
        if ((m_tool == "historyBrush" || m_tool == "artHistoryBrush") &&
            !prepareHistoryBrushStroke()) {
            m_drawing      = false;
            m_beforeStroke = {};
            return;
        }
        m_artHistoryDabIndex = 0;
        m_strokeBase         = *target;
        m_strokeInkColor     = m_color;
        if (m_tool == "colorReplacement") {
            const QPoint sample(int(std::floor(_x)), int(std::floor(_y)));
            m_replacementSample =
                target->rect().contains(sample) ? target->pixelColor(sample) : QColor{};
        }
        if (m_tool == "pencil" && m_pencilAutoErase &&
            target->rect().contains(QPoint(int(_x), int(_y))) &&
            target->pixelColor(int(_x), int(_y)).rgba() == m_color.rgba())
            m_strokeInkColor = m_secondary;
        const QPointF requestedEnd = m_anchor;
        if (_straight && m_paintLastSession == m_documentSession && m_paintLastState == m_stateId &&
            m_paintLastTool == m_tool && m_paintLastLayer == m_active &&
            m_paintLastMask == m_editingMask) {
            m_anchor = m_last = m_paintLastEnd;
            m_strokeConnect   = true;
        }
        m_strokeCoverage = (m_tool == "smudge" || m_tool == "mixerBrush" || m_tool == "healing" ||
                            m_tool == "spotHealing")
                               ? QImage{}
                               : ImageUtils::blank(m_size, workingFormat());
        if (m_tool == "spotHealing")
            prepareSpotHealingStroke();
        if (m_tool == "smudge")
            prepareSmudgeStroke(_alternate);
        if (m_tool == "mixerBrush")
            prepareMixerStroke();
        m_strokeBounds      = {};
        m_smoothedLast      = m_anchor;
        m_distanceToNextDab = qMax(1., m_brushSize * m_brushSpacing);
        paintSegment(m_anchor, m_anchor);
        if (m_strokeConnect && m_anchor != requestedEnd)
            continueStroke(requestedEnd.x(), requestedEnd.y());
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
    if (m_sliceDragging) {
        updateSliceGesture(_x, _y);
        return;
    }
    if (m_tool == "crop" || m_tool == "perspectiveCrop") {
        updateCropGesture(_x, _y);
        return;
    }
    if (!m_drawing || !std::isfinite(_x) || !std::isfinite(_y))
        return;
    QPointF point(qBound(-32768., _x, 32768.), qBound(-32768., _y, 32768.));
    if (m_toolSpace &&
        (m_tool == "select" || m_tool == "ellipticalSelect" || m_tool == "rectangle" ||
         m_tool == "ellipse" || m_tool == "triangle" || m_tool == "polygon" || m_tool == "star" ||
         m_tool == "line" || m_tool == "gradient"))
        m_anchor += point - m_geometryPointer;
    m_geometryPointer = point;
    if (m_tool == "line" || m_tool == "gradient" || m_tool == "move")
        point = constrainedToolPoint(point, true);
    else if (isPaintStrokeTool(m_tool) && !m_strokeConnect)
        point = constrainedToolPoint(point, false);
    if (m_tool == "remove") {
        continueRemove(point);
    } else if (m_tool == "contentAwareMove") {
        continueRegionDrag(point, false);
    } else if (m_tool == "patch") {
        continuePatch(point);
    } else if (m_tool == "redEye") {
        m_redEyeRegion =
            QRectF(m_anchor, point).normalized().intersected(QRectF(QPointF{}, m_size));
        emit redEyeRegionChanged();
    } else if (m_tool == "quickSelection") {
        const qreal  step = qMax(2., m_brushSize * .5);
        const QLineF line(m_last, point);
        const int    count = qMax(1, qCeil(line.length() / step));
        for (int i = 1; i <= count; ++i)
            continueQuickSelection(line.pointAt(qreal(i) / count));
    } else if (m_tool == "lasso")
        continueLasso(point.x(), point.y());
    else if (m_tool == "select" || m_tool == "ellipticalSelect") {
        if (m_selectionMoving) {
            const QPoint offset = (point - m_anchor).toPoint();
            m_selectionRegion =
                m_selectionRegionBeforeStroke.translated(offset) & QRegion(QRect(QPoint{}, m_size));
            m_selection     = m_selectionRegion.boundingRect();
            m_selectionPath = m_selectionPathBeforeStroke.translated(offset);
            emit selectionChanged();
            m_last = point;
            return;
        }
        const QRectF rect =
            toolRectangle(m_anchor, point, m_toolAlternate && !m_selectionInitialAlternate);
        // Round size independently of position so fractional Space-drag offsets
        // preserve the marquee dimensions at non-integer zoom levels.
        m_selection       = QRect(qRound(rect.left()), qRound(rect.top()), qRound(rect.width()) + 1,
                                  qRound(rect.height()) + 1)
                                .intersected(QRect(QPoint{}, m_size));
        m_selectionRegion = m_tool == "ellipticalSelect" ? QRegion(m_selection, QRegion::Ellipse)
                                                         : QRegion(m_selection);
        m_selectionCombined = false;
        combineGestureSelection();
        emit selectionChanged();
    } else if (m_tool == "singleRowSelect" || m_tool == "singleColumnSelect") {
        if (m_selectionMoving) {
            const QPoint delta = (point - m_anchor).toPoint();
            m_selectionRegion =
                m_selectionRegionBeforeStroke.translated(
                    m_tool == "singleRowSelect" ? QPoint(0, delta.y()) : QPoint(delta.x(), 0)) &
                QRegion(QRect(QPoint{}, m_size));
            m_selection = m_selectionRegion.boundingRect();
        } else {
            m_selection = m_tool == "singleRowSelect"
                              ? QRect(0, qBound(0, qRound(point.y()), height() - 1), width(), 1)
                              : QRect(qBound(0, qRound(point.x()), width() - 1), 0, 1, height());
            m_selectionRegion = QRegion(m_selection);
        }
        if (!m_selectionMoving) {
            m_selectionCombined = false;
            combineGestureSelection();
        }
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
    if (m_sliceDragging) {
        endSliceGesture();
        return;
    }
    if (m_cropDragging) {
        endCropGesture();
        return;
    }
    if (!m_drawing)
        return;
    if (m_tool == "remove") {
        finishRemove();
        return;
    }
    if (m_tool == "contentAwareMove") {
        finishContentMove(false);
        return;
    }
    if (m_tool == "patch") {
        finishPatch(false);
        return;
    }
    if (isPaintStrokeTool(m_tool) && QLineF(m_smoothedLast, m_last).length() > .01) {
        const qreal smoothing = m_brushSmoothing;
        m_brushSmoothing      = 0;
        paintSegment(m_smoothedLast, m_last);
        m_brushSmoothing = smoothing;
    }
    if (m_tool == "redEye") {
        correctRedEye();
        m_redEyeRegion = {};
        emit redEyeRegionChanged();
    }
    m_drawing = false;
    if (m_tool == "quickSelection") {
        finishQuickSelection();
        return;
    }
    if (m_tool == "lasso") {
        if (m_toolAlternate && !m_selectionInitialAlternate && m_selectionPath.size() >= 2) {
            m_polygonLassoActive = true;
            m_selectionShape     = 5;
            emit selectionChanged();
        } else
            finishLasso();
        return;
    }
    if (m_tool == "select" || m_tool == "ellipticalSelect" || m_tool == "singleRowSelect" ||
        m_tool == "singleColumnSelect") {
        if (!m_selectionMoving)
            combineGestureSelection();
        return;
    }
    if (m_strokeChanged) {
        if (m_tool == "move" && hasSelection()) {
            const QPoint offset = (m_last - m_anchor).toPoint();
            m_selection = m_selection.translated(offset).intersected(QRect(QPoint{}, m_size));
            m_selectionRegion =
                m_selectionRegion.translated(offset) & QRegion(QRect(QPoint{}, m_size));
            m_selectionPath.translate(offset);
            emit selectionChanged();
        }
        commit(m_beforeStroke, toolLabel(m_tool), !isPaintStrokeTool(m_tool) && m_tool != "redEye");
        if (isPaintStrokeTool(m_tool) || m_tool == "redEye") {
            ++m_thumbnailRevision;
            emit thumbnailsChanged();
        }
    }
    if (isPaintStrokeTool(m_tool) && m_strokeChanged) {
        m_paintLastEnd     = m_last;
        m_paintLastState   = m_stateId;
        m_paintLastSession = m_documentSession;
        m_paintLastTool    = m_tool;
        m_paintLastLayer   = m_active;
        m_paintLastMask    = m_editingMask;
    }
    if (m_tool == "mixerBrush")
        finishMixerStroke(false);
    if (m_tool == "healing")
        finishHealingStroke(false);
    if (m_tool == "spotHealing")
        finishSpotHealingStroke();
    m_historyStrokeSource = {};
    m_beforeStroke        = {};
    m_strokeBase          = {};
    m_strokeCoverage      = {};
    m_strokeBounds        = {};
    m_strokeInkColor      = {};
    m_stampSource         = {};
    m_smudgePickup        = {};
    m_smudgeMask          = {};
    m_smudgeSample        = {};
}
void Document::cancelStroke()
{
    if (m_sliceDragging) {
        endSliceGesture(true);
        return;
    }
    if (m_cropDragging) {
        m_cropPoints = m_cropGesturePoints;
        endCropGesture();
        return;
    }
    invalidateContentMove(true);
    if (m_tool == "remove") {
        cancelRemoval();
        return;
    }
    if (!m_drawing)
        return;
    if (m_tool == "contentAwareMove") {
        finishContentMove(true);
        return;
    }
    if (m_tool == "patch") {
        finishPatch(true);
        return;
    }
    if (m_tool == "redEye") {
        m_redEyeRegion = {};
        emit redEyeRegionChanged();
    }
    m_drawing = false;
    if (m_tool == "quickSelection") {
        cancelQuickSelection();
        return;
    }
    if (m_tool == "lasso") {
        restoreGestureSelection();
        return;
    }
    if (m_tool == "select" || m_tool == "ellipticalSelect" || m_tool == "singleRowSelect" ||
        m_tool == "singleColumnSelect")
        restoreGestureSelection();
    else if (m_tool != "redEye")
        restore(m_beforeStroke);
    if (m_tool == "mixerBrush")
        finishMixerStroke(true);
    if (m_tool == "healing")
        finishHealingStroke(true);
    if (m_tool == "spotHealing")
        finishSpotHealingStroke();
    m_historyStrokeSource = {};
    m_beforeStroke        = {};
    m_strokeBase          = {};
    m_strokeCoverage      = {};
    m_strokeBounds        = {};
    m_strokeInkColor      = {};
    m_stampSource         = {};
    m_smudgePickup        = {};
    m_smudgeMask          = {};
    m_smudgeSample        = {};
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
