#include "Document.h"
#include "ImageUtils.h"

#include <QCoreApplication>
#include <QFont>
#include <QMap>
#include <QPainter>
#include <QtMath>
#include <algorithm>
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
QColor maskColor(const QColor &_color)
{
    const int value = qGray(_color.rgb());
    return QColor(value, value, value);
}
} // namespace

void Document::setTool(const QString &_value)
{
    static const QStringList tools{"brush",     "eraser",  "fill", "picker",   "move",
                                   "rectangle", "ellipse", "line", "gradient", "select",
                                   "crop",      "text",    "hand"};
    if (!tools.contains(_value))
        return;
    endStroke();
    m_tool = _value;
    emit settingsChanged();
}
void Document::setColor(const QColor &_value)
{
    if (_value.isValid()) {
        m_color = _value;
        emit settingsChanged();
    }
}
void Document::setSecondaryColor(const QColor &_value)
{
    if (_value.isValid()) {
        m_secondary = _value;
        emit settingsChanged();
    }
}
void Document::resetColors()
{
    if (m_color == Qt::black && m_secondary == Qt::white)
        return;
    m_color     = Qt::black;
    m_secondary = Qt::white;
    emit settingsChanged();
}
void Document::swapColors()
{
    qSwap(m_color, m_secondary);
    emit settingsChanged();
}
void Document::setBrushSize(qreal _value)
{
    if (std::isfinite(_value)) {
        m_brushSize = qBound(1., _value, 500.);
        emit settingsChanged();
    }
}
void Document::setBrushOpacity(qreal _value)
{
    if (std::isfinite(_value)) {
        m_brushOpacity = qBound(0., _value, 1.);
        emit settingsChanged();
    }
}
void Document::setBrushHardness(qreal _value)
{
    if (std::isfinite(_value)) {
        m_brushHardness = qBound(0., _value, 1.);
        emit settingsChanged();
    }
}
void Document::setBrushFlow(qreal _value)
{
    if (std::isfinite(_value)) {
        m_brushFlow = qBound(.01, _value, 1.);
        emit settingsChanged();
    }
}
void Document::setBrushSpacing(qreal _value)
{
    if (std::isfinite(_value)) {
        m_brushSpacing = qBound(.01, _value, 10.);
        emit settingsChanged();
    }
}
void Document::setBrushAngle(qreal _value)
{
    if (std::isfinite(_value)) {
        m_brushAngle = std::fmod(_value, 360.);
        emit settingsChanged();
    }
}
void Document::setBrushRoundness(qreal _value)
{
    if (std::isfinite(_value)) {
        m_brushRoundness = qBound(.01, _value, 1.);
        emit settingsChanged();
    }
}
void Document::setBrushSmoothing(qreal _value)
{
    if (std::isfinite(_value)) {
        m_brushSmoothing = qBound(0., _value, 1.);
        emit settingsChanged();
    }
}
void Document::setBrushBlend(int _value)
{
    if (_value >= 0 && _value <= 26 && m_brushBlend != _value) {
        m_brushBlend = _value;
        emit settingsChanged();
    }
}
void Document::setText(const QString &_value)
{
    m_text = _value.left(4096);
    emit settingsChanged();
}
void Document::setTextSize(int _value)
{
    m_textSize = qBound(6, _value, 512);
    emit settingsChanged();
}
void Document::setFilledShapes(bool _value)
{
    if (m_filledShapes == _value)
        return;
    m_filledShapes = _value;
    emit settingsChanged();
}
void Document::setShapeFillColor(const QColor &_value)
{
    if (!_value.isValid() || (m_shapeFillColor.isValid() && m_shapeFillColor == _value))
        return;
    m_shapeFillColor = _value;
    emit settingsChanged();
}
void Document::setShapeStrokeColor(const QColor &_value)
{
    if (!_value.isValid() || (m_shapeStrokeColor.isValid() && m_shapeStrokeColor == _value))
        return;
    m_shapeStrokeColor = _value;
    emit settingsChanged();
}
void Document::setShapeStrokeEnabled(bool _value)
{
    if (m_shapeStrokeEnabled == _value)
        return;
    m_shapeStrokeEnabled = _value;
    emit settingsChanged();
}
void Document::setShapeStrokeWidth(qreal _value)
{
    if (!std::isfinite(_value))
        return;
    const qreal width = qBound(0.5, _value, 500.);
    if (qFuzzyCompare(m_shapeStrokeWidth, width))
        return;
    m_shapeStrokeWidth = width;
    emit settingsChanged();
}
void Document::setShapeStrokeStyle(int _value)
{
    const int style = qBound(0, _value, 2);
    if (m_shapeStrokeStyle == style)
        return;
    m_shapeStrokeStyle = style;
    emit settingsChanged();
}
void Document::setShapeCornerRadius(qreal _value)
{
    if (!std::isfinite(_value))
        return;
    const qreal radius = qBound(0., _value, 500.);
    if (qFuzzyCompare(m_shapeCornerRadius + 1., radius + 1.))
        return;
    m_shapeCornerRadius = radius;
    emit settingsChanged();
}
void Document::setShapeAlignPixels(bool _value)
{
    if (m_shapeAlignPixels == _value)
        return;
    m_shapeAlignPixels = _value;
    emit settingsChanged();
}

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
void Document::paintSegment(const QPointF &_from, const QPointF &_to)
{
    Q_UNUSED(_from)
    if (m_strokeCoverage.isNull())
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
    m_strokeChanged = true;
    changed(false);
}
void Document::stampBrush(const QPointF &_point)
{
    QPainter painter(&m_strokeCoverage);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(_point);
    painter.rotate(m_brushAngle);
    painter.scale(1., m_brushRoundness);
    const qreal  radius = m_brushSize / 2.;
    const QColor paint(255, 255, 255, qRound(m_brushFlow * 255));
    if (m_brushHardness >= .999)
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
    QImage       overlay = ImageUtils::blank(region.size());
    const QColor ink =
        m_editingMask ? maskColor(m_tool == "eraser" ? m_secondary : m_color) : m_color;
    overlay.fill(ink);
    QPainter maskPainter(&overlay);
    maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    maskPainter.drawImage(0, 0, m_strokeCoverage.copy(region));
    maskPainter.end();
    QImage canvas = m_strokeBase.copy(region).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (m_tool == "brush" && !m_editingMask) {
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
void Document::floodFill(QPoint _point)
{
    QImage *paintImage = paintTarget();
    if (!paintImage)
        return;
    auto       &image  = *paintImage;
    const QRect bounds = hasSelection() ? m_selection : image.rect();
    if (!bounds.contains(_point))
        return;
    const QRgb targetColor = image.pixel(_point);
    QImage     solid(1, 1, QImage::Format_ARGB32_Premultiplied);
    solid.fill(m_editingMask ? maskColor(m_color) : m_color);
    const QRgb replacement = solid.pixel(0, 0);
    if (targetColor == replacement && qFuzzyCompare(m_brushOpacity, 1.))
        return;
    // Scanline flood fill: visited pixels are kept separate so semi-transparent
    // replacement colors cannot cause pixels to be queued repeatedly.
    QImage mask(image.size(), QImage::Format_Grayscale8);
    mask.fill(0);
    QVector<QPoint> stack{_point};
    auto            matches = [&](int _x, int _y) {
        return !mask.constScanLine(_y)[_x] && image.pixel(_x, _y) == targetColor;
    };
    while (!stack.isEmpty()) {
        auto seed = stack.takeLast();
        if (!matches(seed.x(), seed.y()))
            continue;
        int left = seed.x(), right = seed.x();
        while (left > bounds.left() && matches(left - 1, seed.y()))
            --left;
        while (right < bounds.right() && matches(right + 1, seed.y()))
            ++right;
        auto row = mask.scanLine(seed.y());
        for (int x = left; x <= right; ++x)
            row[x] = 255;
        for (int y : {seed.y() - 1, seed.y() + 1})
            if (y >= bounds.top() && y <= bounds.bottom()) {
                bool run = false;
                for (int x = left; x <= right; ++x) {
                    bool match = matches(x, y);
                    if (match && !run)
                        stack.append({x, y});
                    run = match;
                }
            }
    }
    QImage overlay = ImageUtils::blank(m_size);
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        auto row      = reinterpret_cast<QRgb *>(overlay.scanLine(y));
        auto selected = mask.constScanLine(y);
        for (int x = bounds.left(); x <= bounds.right(); ++x)
            if (selected[x])
                row[x] = replacement;
    }
    QPainter painter(&image);
    painter.setOpacity(m_brushOpacity);
    painter.drawImage(0, 0, overlay);
    painter.end();
    m_strokeChanged = true;
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
