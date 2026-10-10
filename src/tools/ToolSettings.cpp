#include "Document.h"

#include <QtMath>
#include <cmath>

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
