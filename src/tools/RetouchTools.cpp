#include "Document.h"
#include "ImagePixels.h"

#include <QPainter>
#include <QtMath>

void Document::renderRetouchStroke()
{
    QImage *target = paintTarget();
    if (!target || m_strokeBase.isNull() || m_strokeCoverage.isNull())
        return;
    const QString effectTool = m_toolAlternate && m_tool == "dodge"     ? "burn"
                               : m_toolAlternate && m_tool == "burn"    ? "dodge"
                               : m_toolAlternate && m_tool == "blur"    ? "sharpen"
                               : m_toolAlternate && m_tool == "sharpen" ? "blur"
                                                                        : m_tool;
    QRect         region     = m_strokeBounds.toAlignedRect().intersected(QRect(QPoint{}, m_size));
    if (hasSelection())
        region = region.intersected(m_selection);
    if (region.isEmpty())
        return;
    if (!m_fullCompositeDirty)
        m_dirtyRegion = m_dirtyRegion.isEmpty() ? region : m_dirtyRegion.united(region);

    QRect sampleRegion = region;
    if (effectTool == "blur" || effectTool == "sharpen")
        sampleRegion = region.adjusted(-4, -4, 4, 4).intersected(m_strokeBase.rect());
    // Keep every expensive conversion and convolution inside the pixels touched by
    // this stroke segment. Blur and sharpen only need a small source halo so their
    // result at the brush edge still samples the real neighbouring pixels.
    const auto render = [&]<typename Pixel>() {
        using P                 = ImageUtils::PixelAccess<Pixel>;
        const auto mixedChannel = [](double _base, double _effect, qreal _amount) {
            return qBound(0., P::round(_base + (_effect - _base) * _amount), 255.);
        };
        const QImage sourcePatch  = m_strokeBase.copy(sampleRegion).convertToFormat(P::format);
        const QImage smooth       = (effectTool == "blur" || effectTool == "sharpen")
                                        ? blurImage(sourcePatch, 2)
                                        : QImage{};
        const QPoint outputOffset = region.topLeft() - sampleRegion.topLeft();
        QImage       output       = sourcePatch.copy(QRect(outputOffset, region.size()));

        for (int y = 0; y < region.height(); ++y) {
            auto       *dst = reinterpret_cast<Pixel *>(output.scanLine(y));
            const auto *coverage =
                reinterpret_cast<const QRgb *>(m_strokeCoverage.constScanLine(region.y() + y));
            const auto *src =
                reinterpret_cast<const Pixel *>(sourcePatch.constScanLine(outputOffset.y() + y));
            for (int x = 0; x < region.width(); ++x) {
                const int globalX = region.x() + x;
                const int globalY = region.y() + y;
                if (hasSelection() && !m_selectionRegion.contains(QPoint(globalX, globalY)))
                    continue;
                qreal amount = qAlpha(coverage[globalX]) / 255. * m_brushOpacity * m_toolStrength;
                if (amount <= 0)
                    continue;
                const Pixel pixel = src[outputOffset.x() + x];
                double      red = P::red(pixel), green = P::green(pixel), blue = P::blue(pixel);
                double      effectRed = red, effectGreen = green, effectBlue = blue;
                if (effectTool == "blur" || effectTool == "sharpen") {
                    const Pixel soft = reinterpret_cast<const Pixel *>(
                        smooth.constScanLine(outputOffset.y() + y))[outputOffset.x() + x];
                    if (effectTool == "blur") {
                        effectRed   = P::red(soft);
                        effectGreen = P::green(soft);
                        effectBlue  = P::blue(soft);
                    } else {
                        effectRed   = qBound(0., 2 * red - P::red(soft), 255.);
                        effectGreen = qBound(0., 2 * green - P::green(soft), 255.);
                        effectBlue  = qBound(0., 2 * blue - P::blue(soft), 255.);
                    }
                } else if (effectTool == "dodge" || effectTool == "burn") {
                    const qreal luminance = (.2126 * red + .7152 * green + .0722 * blue) / 255.;
                    const qreal range     = m_retouchRange == 0   ? 1. - luminance
                                            : m_retouchRange == 2 ? luminance
                                                                  : 1. - qAbs(2. * luminance - 1.);
                    amount *= qBound(.08, range, 1.);
                    if (m_protectTones)
                        amount *= .75;
                    if (effectTool == "dodge") {
                        effectRed   = P::round(red + (255 - red) * amount);
                        effectGreen = P::round(green + (255 - green) * amount);
                        effectBlue  = P::round(blue + (255 - blue) * amount);
                    } else {
                        effectRed   = P::round(red * (1. - amount));
                        effectGreen = P::round(green * (1. - amount));
                        effectBlue  = P::round(blue * (1. - amount));
                    }
                    amount = 1.;
                } else if (effectTool == "sponge") {
                    QColor color = QColor::fromRgbF(red / 255., green / 255., blue / 255.);
                    float  hue = 0, saturation = 0, lightness = 0;
                    color.getHslF(&hue, &saturation, &lightness);
                    saturation = m_spongeSaturate ? saturation + (1. - saturation) * amount
                                                  : saturation * (1. - amount);
                    color.setHslF(hue, qBound(0., saturation, 1.), lightness);
                    effectRed   = color.redF() * 255.;
                    effectGreen = color.greenF() * 255.;
                    effectBlue  = color.blueF() * 255.;
                    amount      = 1.;
                }
                dst[x] = P::rgba(mixedChannel(red, effectRed, amount),
                                 mixedChannel(green, effectGreen, amount),
                                 mixedChannel(blue, effectBlue, amount), P::alpha(pixel));
            }
        }
        QPainter painter(target);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(region.topLeft(), output);
    };
    if (ImageUtils::is16Bit(*target))
        render.template operator()<QRgba64>();
    else
        render.template operator()<QRgb>();
}
