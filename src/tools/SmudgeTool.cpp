#include "Document.h"
#include "ImageUtils.h"

#include <QPainter>
#include <QtMath>

namespace
{
QImage samplePatch(const PixelPlane &_image, const QRect &_rect, const QSize &_size,
                   const Document *_composite)
{
    QImage patch = ImageUtils::blank(_rect.size());
    if (patch.isNull())
        return {};
    const QRect region = _rect.intersected(QRect(QPoint{}, _size));
    if (!region.isEmpty()) {
        const QImage pixels =
            _composite ? _composite->frozenCompositeRegion(region) : _image.region(region);
        if (pixels.isNull())
            return {};
        QPainter painter(&patch);
        painter.drawImage(region.topLeft() - _rect.topLeft(), pixels);
    }
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
    // Keep native tile versions fixed for all-layer pickup; render only the
    // brush-sized patches encountered during this gesture.
    m_smudgeCompositeSource =
        m_smudgeSampleAllLayers && !m_editingMask ? nativeCompositeSource(m_layers) : nullptr;
    const PixelPlane &target = m_editingMask ? m_layers[m_active].mask : m_layers[m_active].image;
    m_smudgePickup           = samplePatch(target, rect, m_size, m_smudgeCompositeSource.get());
    m_smudgeMask             = ImageUtils::blank(QSize(size, size));
    if (m_smudgePickup.isNull() || m_smudgeMask.isNull()) {
        m_strokeCoverageFailed = true;
        return;
    }
    if (m_smudgeFingerPainting || _alternate) {
        QColor color = m_color;
        if (m_editingMask) {
            const int gray = qGray(color.rgb());
            color          = QColor(gray, gray, gray);
        }
        m_smudgePickup.fill(color);
    }
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
    if (m_smudgePickup.isNull())
        return;
    PixelPlane &target = m_editingMask ? m_layers[m_active].mask : m_layers[m_active].image;
    const int   size   = m_smudgePickup.width();
    const QRect dab(qFloor(_point.x()) - size / 2, qFloor(_point.y()) - size / 2, size, size);
    QRect       region = dab.intersected(QRect(QPoint{}, m_size));
    if (hasSelection())
        region = region.intersected(m_selection);
    if (region.isEmpty())
        return;

    const QImage sampled = samplePatch(target, dab, m_size, m_smudgeCompositeSource.get());
    QImage output = target.region(region).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QPoint offset = region.topLeft() - dab.topLeft();
    const QImage effect =
        m_brushBlend == 0 ? m_smudgePickup.copy(QRect(offset, region.size()))
                          : blendImages(output, m_smudgePickup.copy(QRect(offset, region.size())),
                                        m_brushBlend, 1.);
    if (sampled.isNull() || output.isNull() || effect.isNull()) {
        m_strokeCoverageFailed = true;
        return;
    }
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
            dst[x] = mixPixel(dst[x], paint[x],
                              coverage * m_toolStrength *
                                  selectionCoverage(region.x() + x, region.y() + y) / 255.);
            // Pick up some of the paint encountered along the way so successive
            // dabs blend the colours rather than duplicate an unchanged patch.
            pickup[localX] =
                mixPixel(pickup[localX], src[localX], coverage * (1. - m_toolStrength));
        }
    }
    const QImage patch = m_editingMask ? output.convertToFormat(QImage::Format_Grayscale8) : output;
    if (!target.writeRegion(region.topLeft(), patch, m_size)) {
        m_strokeCoverageFailed = true;
        return;
    }
    m_strokeBounds = m_strokeBounds.isEmpty() ? QRectF(region) : m_strokeBounds.united(region);
    if (!m_fullCompositeDirty)
        m_dirtyRegion = m_dirtyRegion.isEmpty() ? region : m_dirtyRegion.united(region);
}
