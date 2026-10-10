#include "Document.h"

#include <QPainter>
#include <QtMath>

namespace
{
int mixedChannel(int _base, int _effect, qreal _amount)
{
    return qBound(0, qRound(_base + (_effect - _base) * _amount), 255);
}
} // namespace

void Document::renderRetouchStroke()
{
    QImage *target = paintTarget();
    if (!target || m_strokeBase.isNull() || m_strokeCoverage.isNull())
        return;
    QRect region = m_strokeBounds.toAlignedRect().intersected(QRect(QPoint{}, m_size));
    if (hasSelection())
        region = region.intersected(m_selection);
    if (region.isEmpty())
        return;
    if (!m_fullCompositeDirty)
        m_dirtyRegion = m_dirtyRegion.isEmpty() ? region : m_dirtyRegion.united(region);

    QRect sampleRegion = region;
    if (m_tool == "blur" || m_tool == "sharpen")
        sampleRegion = region.adjusted(-4, -4, 4, 4).intersected(m_strokeBase.rect());
    // Keep every expensive conversion and convolution inside the pixels touched by
    // this stroke segment. Blur and sharpen only need a small source halo so their
    // result at the brush edge still samples the real neighbouring pixels.
    const QImage sourcePatch =
        m_strokeBase.copy(sampleRegion).convertToFormat(QImage::Format_ARGB32);
    const QImage smooth =
        (m_tool == "blur" || m_tool == "sharpen") ? blurImage(sourcePatch, 2) : QImage{};
    const QPoint outputOffset = region.topLeft() - sampleRegion.topLeft();
    QImage       output       = sourcePatch.copy(QRect(outputOffset, region.size()));

    for (int y = 0; y < region.height(); ++y) {
        auto       *dst = reinterpret_cast<QRgb *>(output.scanLine(y));
        const auto *coverage =
            reinterpret_cast<const QRgb *>(m_strokeCoverage.constScanLine(region.y() + y));
        const auto *src =
            reinterpret_cast<const QRgb *>(sourcePatch.constScanLine(outputOffset.y() + y));
        for (int x = 0; x < region.width(); ++x) {
            const int globalX = region.x() + x;
            const int globalY = region.y() + y;
            if (hasSelection() && !m_selectionRegion.contains(QPoint(globalX, globalY)))
                continue;
            qreal amount = qAlpha(coverage[globalX]) / 255. * m_brushOpacity * m_toolStrength;
            if (amount <= 0)
                continue;
            const QRgb pixel = src[outputOffset.x() + x];
            int        red = qRed(pixel), green = qGreen(pixel), blue = qBlue(pixel);
            int        effectRed = red, effectGreen = green, effectBlue = blue;
            if (m_tool == "blur" || m_tool == "sharpen") {
                const QRgb soft = reinterpret_cast<const QRgb *>(
                    smooth.constScanLine(outputOffset.y() + y))[outputOffset.x() + x];
                if (m_tool == "blur") {
                    effectRed   = qRed(soft);
                    effectGreen = qGreen(soft);
                    effectBlue  = qBlue(soft);
                } else {
                    effectRed   = qBound(0, 2 * red - qRed(soft), 255);
                    effectGreen = qBound(0, 2 * green - qGreen(soft), 255);
                    effectBlue  = qBound(0, 2 * blue - qBlue(soft), 255);
                }
            } else if (m_tool == "dodge" || m_tool == "burn") {
                const qreal luminance = (.2126 * red + .7152 * green + .0722 * blue) / 255.;
                const qreal range     = m_retouchRange == 0   ? 1. - luminance
                                        : m_retouchRange == 2 ? luminance
                                                              : 1. - qAbs(2. * luminance - 1.);
                amount *= qBound(.08, range, 1.);
                if (m_protectTones)
                    amount *= .75;
                if (m_tool == "dodge") {
                    effectRed   = qRound(red + (255 - red) * amount);
                    effectGreen = qRound(green + (255 - green) * amount);
                    effectBlue  = qRound(blue + (255 - blue) * amount);
                } else {
                    effectRed   = qRound(red * (1. - amount));
                    effectGreen = qRound(green * (1. - amount));
                    effectBlue  = qRound(blue * (1. - amount));
                }
                amount = 1.;
            } else if (m_tool == "sponge") {
                QColor color(red, green, blue);
                float  hue = 0, saturation = 0, lightness = 0;
                color.getHslF(&hue, &saturation, &lightness);
                saturation = m_spongeSaturate ? saturation + (1. - saturation) * amount
                                              : saturation * (1. - amount);
                color.setHslF(hue, qBound(0., saturation, 1.), lightness);
                effectRed   = color.red();
                effectGreen = color.green();
                effectBlue  = color.blue();
                amount      = 1.;
            }
            dst[x] = qRgba(mixedChannel(red, effectRed, amount),
                           mixedChannel(green, effectGreen, amount),
                           mixedChannel(blue, effectBlue, amount), qAlpha(pixel));
        }
    }
    QPainter painter(target);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(region.topLeft(), output);
}
