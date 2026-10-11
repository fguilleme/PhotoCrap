#include "Document.h"
#include "ImageUtils.h"

#include <QPainter>
#include <QtMath>
#include <cstring>

namespace
{
QImage samplePatch(const QImage &_image, const QRect &_rect)
{
    QImage   patch = ImageUtils::blank(_rect.size());
    QPainter painter(&patch);
    painter.drawImage(-_rect.topLeft(), _image);
    return patch;
}

QRgb mixPixel(QRgb _base, QRgb _paint, qreal _amount)
{
    const auto mix = [_amount](int _from, int _to) {
        return qRound(_from + (_to - _from) * _amount);
    };
    return qRgba(mix(qRed(_base), qRed(_paint)), mix(qGreen(_base), qGreen(_paint)),
                 mix(qBlue(_base), qBlue(_paint)), mix(qAlpha(_base), qAlpha(_paint)));
}
} // namespace

void Document::setSmudgeSampleAllLayers(bool _value)
{
    if (m_smudgeSampleAllLayers == _value)
        return;
    endStroke();
    m_smudgeSampleAllLayers = _value;
    emit settingsChanged();
}

void Document::setSmudgeFingerPainting(bool _value)
{
    if (m_smudgeFingerPainting == _value)
        return;
    endStroke();
    m_smudgeFingerPainting = _value;
    emit settingsChanged();
}

void Document::prepareSmudgeStroke(bool _alternate)
{
    const int   size = qCeil(m_brushSize) + 4;
    const QRect rect(qFloor(m_anchor.x()) - size / 2, qFloor(m_anchor.y()) - size / 2, size, size);
    // All-layer sampling takes one shared snapshot per gesture; motion never
    // recomposes the document. Pickup and brush work remain brush-sized.
    m_smudgeSample = m_smudgeSampleAllLayers && !m_editingMask ? composite() : QImage{};
    m_smudgePickup = samplePatch(m_smudgeSample.isNull() ? *paintTarget() : m_smudgeSample, rect);
    if (m_smudgeFingerPainting || _alternate) {
        QColor color = m_color;
        if (m_editingMask) {
            const int gray = qGray(color.rgb());
            color          = QColor(gray, gray, gray);
        }
        m_smudgePickup.fill(color);
    }
    m_smudgeMask = ImageUtils::blank(QSize(size, size));
    QPainter painter(&m_smudgeMask);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(size / 2., size / 2.);
    painter.rotate(m_brushAngle);
    painter.scale(1., m_brushRoundness);
    const qreal radius = m_brushSize / 2.;
    if (m_brushHardness >= .999)
        painter.setBrush(Qt::white);
    else {
        QRadialGradient gradient(QPointF{}, radius);
        gradient.setColorAt(m_brushHardness, Qt::white);
        gradient.setColorAt(1., Qt::transparent);
        painter.setBrush(gradient);
    }
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF{}, radius, radius);
}

void Document::smudgeDab(const QPointF &_point)
{
    QImage *target = paintTarget();
    if (!target || m_smudgePickup.isNull())
        return;
    const int   size = m_smudgePickup.width();
    const QRect dab(qFloor(_point.x()) - size / 2, qFloor(_point.y()) - size / 2, size, size);
    QRect       region = dab.intersected(QRect(QPoint{}, m_size));
    if (hasSelection())
        region = region.intersected(m_selection);
    if (region.isEmpty())
        return;

    const QImage sampled = samplePatch(m_smudgeSample.isNull() ? *target : m_smudgeSample, dab);
    QImage       output = target->copy(region).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QPoint offset = region.topLeft() - dab.topLeft();
    const QImage effect =
        m_brushBlend == 0 ? m_smudgePickup.copy(QRect(offset, region.size()))
                          : blendImages(output, m_smudgePickup.copy(QRect(offset, region.size())),
                                        m_brushBlend, 1.);
    for (int y = 0; y < region.height(); ++y) {
        auto       *dst    = reinterpret_cast<QRgb *>(output.scanLine(y));
        auto       *pickup = reinterpret_cast<QRgb *>(m_smudgePickup.scanLine(offset.y() + y));
        const auto *src    = reinterpret_cast<const QRgb *>(sampled.constScanLine(offset.y() + y));
        const auto *mask =
            reinterpret_cast<const QRgb *>(m_smudgeMask.constScanLine(offset.y() + y));
        const auto *paint = reinterpret_cast<const QRgb *>(effect.constScanLine(y));
        for (int x = 0; x < region.width(); ++x) {
            if (hasSelection() && !m_selectionRegion.contains(region.topLeft() + QPoint(x, y)))
                continue;
            const int   localX   = offset.x() + x;
            const qreal coverage = qAlpha(mask[localX]) / 255.;
            if (coverage <= 0)
                continue;
            dst[x] = mixPixel(dst[x], paint[x], coverage * m_toolStrength);
            // Pick up some of the paint encountered along the way so successive
            // dabs blend the colours rather than duplicate an unchanged patch.
            pickup[localX] =
                mixPixel(pickup[localX], src[localX], coverage * (1. - m_toolStrength));
        }
    }
    if (m_editingMask) {
        const QImage gray = output.convertToFormat(QImage::Format_Grayscale8);
        for (int y = 0; y < region.height(); ++y)
            memcpy(target->scanLine(region.y() + y) + region.x(), gray.constScanLine(y),
                   size_t(region.width()));
    } else {
        QPainter painter(target);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(region.topLeft(), output);
    }
    m_strokeBounds = m_strokeBounds.isEmpty() ? QRectF(region) : m_strokeBounds.united(region);
    if (!m_fullCompositeDirty)
        m_dirtyRegion = m_dirtyRegion.isEmpty() ? region : m_dirtyRegion.united(region);
}
