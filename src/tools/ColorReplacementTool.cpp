#include "Document.h"
#include "ImageUtils.h"

#include <QBitArray>
#include <QPainter>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
using Color = std::array<qreal, 3>;
Color channels(QRgb _pixel)
{
    return {qRed(_pixel) / 255., qGreen(_pixel) / 255., qBlue(_pixel) / 255.};
}
qreal luminosity(const Color &_color)
{
    return .3 * _color[0] + .59 * _color[1] + .11 * _color[2];
}
qreal saturation(const Color &_color)
{
    return *std::max_element(_color.begin(), _color.end()) -
           *std::min_element(_color.begin(), _color.end());
}
Color withLuminosity(Color _color, qreal _luminosity)
{
    const qreal delta = _luminosity - luminosity(_color);
    for (auto &channel : _color)
        channel += delta;
    const qreal low  = *std::min_element(_color.begin(), _color.end());
    const qreal high = *std::max_element(_color.begin(), _color.end());
    if (low < 0)
        for (auto &channel : _color)
            channel = _luminosity + (channel - _luminosity) * _luminosity / (_luminosity - low);
    if (high > 1)
        for (auto &channel : _color)
            channel =
                _luminosity + (channel - _luminosity) * (1 - _luminosity) / (high - _luminosity);
    return _color;
}
Color withSaturation(Color _color, qreal _saturation)
{
    const qreal low   = *std::min_element(_color.begin(), _color.end());
    const qreal range = saturation(_color);
    for (auto &channel : _color)
        channel = range > 0 ? (channel - low) * _saturation / range : 0;
    return _color;
}
int difference(QRgb _first, QRgb _second)
{
    return qMax(qAbs(qRed(_first) - qRed(_second)),
                qMax(qAbs(qGreen(_first) - qGreen(_second)), qAbs(qBlue(_first) - qBlue(_second))));
}
} // namespace

void Document::setReplacementMode(int _value)
{
    _value = qBound(0, _value, 3);
    if (_value == m_replacementMode)
        return;
    endStroke();
    m_replacementMode = _value;
    emit settingsChanged();
}
void Document::setReplacementSampling(int _value)
{
    _value = qBound(0, _value, 2);
    if (_value == m_replacementSampling)
        return;
    endStroke();
    m_replacementSampling = _value;
    emit settingsChanged();
}
void Document::setReplacementLimits(int _value)
{
    _value = qBound(0, _value, 2);
    if (_value == m_replacementLimits)
        return;
    endStroke();
    m_replacementLimits = _value;
    emit settingsChanged();
}
void Document::setReplacementTolerance(int _value)
{
    _value = qBound(0, _value, 100);
    if (_value == m_replacementTolerance)
        return;
    endStroke();
    m_replacementTolerance = _value;
    emit settingsChanged();
}
void Document::setReplacementAntialias(bool _value)
{
    if (_value == m_replacementAntialias)
        return;
    endStroke();
    m_replacementAntialias = _value;
    emit settingsChanged();
}

void Document::colorReplacementDab(const QPointF &_point)
{
    QImage *target = paintTarget();
    if (!target || m_strokeBase.isNull() || m_editingMask)
        return;
    const QPoint center(qFloor(_point.x()), qFloor(_point.y()));
    if (!m_strokeBase.rect().contains(center))
        return;
    const QColor sample = m_replacementSampling == 0   ? m_strokeBase.pixelColor(center)
                          : m_replacementSampling == 1 ? m_replacementSample
                                                       : m_secondary;
    if (!sample.isValid() || sample.alpha() == 0)
        return;
    const qreal radius = m_brushSize / 2.;
    QRect       region =
        QRectF(_point.x() - radius - 2, _point.y() - radius - 2, 2 * radius + 4, 2 * radius + 4)
            .toAlignedRect()
            .intersected(target->rect());
    if (hasSelection())
        region = region.intersected(m_selection);
    if (region.isEmpty())
        return;
    // Sampling and colour calculations read the original pixels throughout a
    // gesture. Only the brush-sized patch is copied or visited on each dab.
    const QImage base = m_strokeBase.copy(region).convertToFormat(QImage::Format_ARGB32);
    QImage       tip  = ImageUtils::blank(region.size());
    QPainter     brush(&tip);
    brush.setRenderHint(QPainter::Antialiasing, m_replacementAntialias);
    brush.translate(_point - region.topLeft());
    brush.rotate(m_brushAngle);
    brush.scale(1., m_brushRoundness);
    brush.setPen(Qt::NoPen);
    if (m_brushHardness >= .999)
        brush.setBrush(Qt::white);
    else {
        QRadialGradient gradient(QPointF{}, radius);
        gradient.setColorAt(m_brushHardness, Qt::white);
        gradient.setColorAt(1., Qt::transparent);
        brush.setBrush(gradient);
    }
    brush.drawEllipse(QPointF{}, radius, radius);
    brush.end();
    const int threshold = qRound(m_replacementTolerance * 2.55);
    const int width = region.width(), height = region.height();
    QBitArray eligible(width * height), connected(width * height);
    for (int y = 0; y < height; ++y) {
        const auto *src  = reinterpret_cast<const QRgb *>(base.constScanLine(y));
        const auto *mask = reinterpret_cast<const QRgb *>(tip.constScanLine(y));
        for (int x = 0; x < width; ++x)
            if (qAlpha(src[x]) && qAlpha(mask[x]) &&
                difference(src[x], sample.rgb()) <= threshold &&
                (!hasSelection() || m_selectionRegion.contains(region.topLeft() + QPoint(x, y))))
                eligible.setBit(y * width + x);
    }
    if (m_replacementLimits != 1) {
        const QPoint seed = center - region.topLeft();
        if (!base.rect().contains(seed) || !eligible.testBit(seed.y() * width + seed.x()))
            return;
        QVector<QPoint> pending{seed};
        connected.setBit(seed.y() * width + seed.x());
        for (qsizetype i = 0; i < pending.size(); ++i) {
            const QPoint p = pending[i];
            for (const QPoint delta : {QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(0, -1)}) {
                const QPoint next = p + delta;
                if (!base.rect().contains(next))
                    continue;
                const int index = next.y() * width + next.x();
                if (!eligible.testBit(index) || connected.testBit(index))
                    continue;
                // Find Edges also refuses abrupt local transitions, even when
                // both colours fall inside the broad sampled-colour tolerance.
                if (m_replacementLimits == 2 &&
                    difference(base.pixel(p), base.pixel(next)) > qMax(8, threshold / 2))
                    continue;
                connected.setBit(index);
                pending.append(next);
            }
        }
    } else
        connected = eligible;

    const Color ink = channels(m_strokeInkColor.rgb());
    QRect       touched;
    for (int y = 0; y < height; ++y) {
        const auto *src  = reinterpret_cast<const QRgb *>(base.constScanLine(y));
        const auto *mask = reinterpret_cast<const QRgb *>(tip.constScanLine(y));
        auto       *coverage =
            reinterpret_cast<QRgb *>(m_strokeCoverage.scanLine(region.y() + y)) + region.x();
        auto *dst = reinterpret_cast<QRgb *>(target->scanLine(region.y() + y)) + region.x();
        for (int x = 0; x < width; ++x) {
            if (!connected.testBit(y * width + x))
                continue;
            const qreal edge =
                m_replacementAntialias && threshold > 0
                    ? qMin(1., (threshold - difference(src[x], sample.rgb()) + 1) / 2.)
                    : 1.;
            const int amount     = qMax(qAlpha(coverage[x]), qRound(qAlpha(mask[x]) * edge));
            coverage[x]          = qRgba(255, 255, 255, amount);
            const Color original = channels(src[x]);
            Color       effect;
            switch (m_replacementMode) {
            case 1:
                effect =
                    withLuminosity(withSaturation(ink, saturation(original)), luminosity(original));
                break;
            case 2:
                effect =
                    withLuminosity(withSaturation(original, saturation(ink)), luminosity(original));
                break;
            case 3:
                effect = withLuminosity(original, luminosity(ink));
                break;
            default:
                effect = withLuminosity(ink, luminosity(original));
                break;
            }
            const qreal weight = amount / 255.;
            const auto  mix    = [&](int _channel) {
                return qBound(0,
                              qRound(255 * (original[_channel] +
                                            (effect[_channel] - original[_channel]) * weight)),
                              255);
            };
            const QRgb pixel = qPremultiply(qRgba(mix(0), mix(1), mix(2), qAlpha(src[x])));
            if (dst[x] == pixel)
                continue;
            dst[x]  = pixel;
            touched = touched.united(QRect(region.topLeft() + QPoint(x, y), QSize(1, 1)));
        }
    }
    if (touched.isEmpty())
        return;
    m_strokeBounds = m_strokeBounds.isEmpty() ? QRectF(touched) : m_strokeBounds.united(touched);
    if (!m_fullCompositeDirty)
        m_dirtyRegion = m_dirtyRegion.isEmpty() ? touched : m_dirtyRegion.united(touched);
}
