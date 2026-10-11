#include "Document.h"
#include "ImageUtils.h"

#include <QPainter>
#include <QtMath>
#include <cmath>
#include <cstring>

void Document::setStampAligned(bool _value)
{
    endStroke();
    m_stampAligned       = _value;
    m_cloneOffsetValid   = false;
    m_patternOriginValid = false;
    emit settingsChanged();
}

void Document::setCloneSampleMode(int _value)
{
    endStroke();
    m_cloneSampleMode = qBound(0, _value, 2);
    emit settingsChanged();
}

void Document::setStampPattern(int _value)
{
    if (_value < 0 || _value > 3 || (_value == 3 && m_customStampPattern.isNull()))
        return;
    endStroke();
    m_stampPattern       = _value;
    m_patternOriginValid = false;
    emit settingsChanged();
}

void Document::setCloneSource(qreal _x, qreal _y)
{
    if (!std::isfinite(_x) || !std::isfinite(_y) ||
        !QRect(QPoint{}, m_size).contains(QPoint(qFloor(_x), qFloor(_y))))
        return;
    endStroke();
    m_cloneSource      = QPointF(qFloor(_x), qFloor(_y));
    m_cloneSourceValid = true;
    m_cloneOffsetValid = false;
    emit settingsChanged();
}

bool Document::defineStampPattern()
{
    endStroke();
    if (!hasSelection())
        return fail(tr("Sélectionnez une zone rectangulaire pour définir un motif."));
    if (m_selection.width() > 1024 || m_selection.height() > 1024)
        return fail(tr("Un motif peut mesurer jusqu’à 1 024 × 1 024 pixels."));
    m_customStampPattern = composite().copy(m_selection);
    m_stampPattern       = 3;
    m_patternOriginValid = false;
    emit settingsChanged();
    return true;
}

QImage Document::stampPatternImage() const
{
    if (m_stampPattern == 3)
        return m_customStampPattern;
    QImage tile = ImageUtils::blank(QSize(24, 24));
    tile.fill(QColor("#eeeeee"));
    QPainter painter(&tile);
    if (m_stampPattern == 0) {
        painter.fillRect(0, 0, 12, 12, QColor("#777777"));
        painter.fillRect(12, 12, 12, 12, QColor("#777777"));
    } else if (m_stampPattern == 1) {
        painter.setBrush(QColor("#666666"));
        painter.setPen(Qt::NoPen);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawEllipse(QPointF(12, 12), 4, 4);
    } else {
        painter.setPen(QPen(QColor("#777777"), 5));
        for (int offset = -24; offset <= 24; offset += 12)
            painter.drawLine(offset, 0, offset + 24, 24);
    }
    return tile;
}

bool Document::prepareStampStroke()
{
    if (m_tool == "patternStamp") {
        m_stampSource = stampPatternImage();
        if (!m_stampAligned || !m_patternOriginValid) {
            m_patternOrigin      = m_anchor;
            m_patternOriginValid = true;
        }
        return !m_stampSource.isNull();
    }
    if (!m_cloneSourceValid)
        return fail(tr("Option/Alt-clic sur l’image pour définir la source du tampon."));
    if (!m_stampAligned || !m_cloneOffsetValid) {
        m_cloneOffset      = m_cloneSource - m_anchor;
        m_cloneOffsetValid = true;
        emit settingsChanged();
    }
    // Hold the source fixed for the gesture, including overlapping source and
    // destination. QImage shares pixels until the destination is changed.
    if (m_editingMask || m_cloneSampleMode == 0)
        m_stampSource = *paintTarget();
    else if (m_cloneSampleMode == 2)
        m_stampSource = composite();
    else {
        auto layers = m_layers;
        for (int i = m_active + 1; i < layers.size(); ++i)
            if (layers[i].kind != "group")
                layers[i].visible = false;
        m_stampSource = renderPreviewSnapshot(std::move(layers), m_size, m_size, false);
    }
    return !m_stampSource.isNull();
}

void Document::renderStampStroke()
{
    QImage *target = paintTarget();
    if (!target || m_stampSource.isNull() || m_strokeBase.isNull())
        return;
    QRect region = m_strokeBounds.toAlignedRect().intersected(QRect(QPoint{}, m_size));
    if (hasSelection())
        region = region.intersected(m_selection);
    if (region.isEmpty())
        return;
    if (!m_fullCompositeDirty)
        m_dirtyRegion = m_dirtyRegion.isEmpty() ? region : m_dirtyRegion.united(region);

    // All temporary images, blending and copies cover only the touched pixels.
    QImage   overlay = ImageUtils::blank(region.size());
    QPainter sampler(&overlay);
    if (hasSelection())
        sampler.setClipRegion(m_selectionRegion.translated(-region.topLeft()));
    if (m_tool == "cloneStamp")
        sampler.drawImage(-QPointF(region.topLeft()) - m_cloneOffset, m_stampSource);
    else {
        sampler.setBrushOrigin(m_patternOrigin - QPointF(region.topLeft()));
        sampler.fillRect(overlay.rect(), QBrush(m_stampSource));
    }
    sampler.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    sampler.drawImage(0, 0, m_strokeCoverage.copy(region));
    sampler.end();

    QImage canvas = m_strokeBase.copy(region).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    canvas        = blendImages(canvas, overlay, m_brushBlend, m_brushOpacity);
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
