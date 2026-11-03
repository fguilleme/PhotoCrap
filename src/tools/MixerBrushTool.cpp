#include "Document.h"
#include "ImageUtils.h"

#include <QPainter>
#include <QtMath>
#include <cmath>

namespace
{
QImage patch(const PixelPlane &_source, const QRect &_rect, const QSize &_size,
             const Document *_composite = nullptr)
{
    QImage image = ImageUtils::blank(_rect.size());
    if (image.isNull())
        return {};
    const QRect region = _rect.intersected(QRect(QPoint{}, _size));
    if (!region.isEmpty()) {
        const QImage pixels =
            _composite ? _composite->compositeRegion(region) : _source.region(region);
        if (pixels.isNull())
            return {};
        QPainter painter(&image);
        painter.drawImage(region.topLeft() - _rect.topLeft(), pixels);
    }
    return image;
}
QRgb interpolate(QRgb _a, QRgb _b, qreal _weight)
{
    const auto mix = [_weight](int _first, int _second) {
        return qRound(_first + (_second - _first) * _weight);
    };
    return qRgba(mix(qRed(_a), qRed(_b)), mix(qGreen(_a), qGreen(_b)), mix(qBlue(_a), qBlue(_b)),
                 mix(qAlpha(_a), qAlpha(_b)));
}
QRgb attenuate(QRgb _pixel, qreal _amount)
{
    return interpolate(0, _pixel, _amount);
}
QColor averageColor(const QImage &_image)
{
    qint64 red = 0, green = 0, blue = 0, alpha = 0;
    for (int y = 0; y < _image.height(); ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(_image.constScanLine(y));
        for (int x = 0; x < _image.width(); ++x) {
            red += qRed(row[x]);
            green += qGreen(row[x]);
            blue += qBlue(row[x]);
            alpha += qAlpha(row[x]);
        }
    }
    return alpha ? QColor(int(red * 255 / alpha), int(green * 255 / alpha), int(blue * 255 / alpha))
                 : QColor(Qt::transparent);
}
} // namespace

void Document::setMixerWet(qreal _value)
{
    if (!std::isfinite(_value))
        return;
    _value = qBound(0., _value, 1.);
    if (qFuzzyCompare(m_mixerWet + 1., _value + 1.))
        return;
    endStroke();
    m_mixerWet = _value;
    emit settingsChanged();
}

void Document::setMixerLoad(qreal _value)
{
    if (!std::isfinite(_value))
        return;
    _value = qBound(0., _value, 1.);
    if (qFuzzyCompare(m_mixerLoad + 1., _value + 1.))
        return;
    endStroke();
    m_mixerLoad = _value;
    reloadMixerPaint();
    emit settingsChanged();
}

void Document::setMixerMix(qreal _value)
{
    if (!std::isfinite(_value))
        return;
    _value = qBound(0., _value, 1.);
    if (qFuzzyCompare(m_mixerMix + 1., _value + 1.))
        return;
    endStroke();
    m_mixerMix = _value;
    emit settingsChanged();
}

void Document::setMixerFlow(qreal _value)
{
    if (!std::isfinite(_value))
        return;
    _value = qBound(0., _value, 1.);
    if (qFuzzyCompare(m_mixerFlow + 1., _value + 1.))
        return;
    endStroke();
    m_mixerFlow = _value;
    emit settingsChanged();
}

void Document::setMixerSampleAllLayers(bool _value)
{
    if (m_mixerSampleAllLayers == _value)
        return;
    endStroke();
    m_mixerSampleAllLayers = _value;
    emit settingsChanged();
}

void Document::setMixerAutoLoad(bool _value)
{
    if (m_mixerAutoLoad == _value)
        return;
    endStroke();
    m_mixerAutoLoad = _value;
    emit settingsChanged();
}

void Document::setMixerAutoClean(bool _value)
{
    if (m_mixerAutoClean == _value)
        return;
    endStroke();
    m_mixerAutoClean = _value;
    emit settingsChanged();
}

void Document::setMixerSolidOnly(bool _value)
{
    if (m_mixerSolidOnly == _value)
        return;
    endStroke();
    m_mixerSolidOnly = _value;
    emit settingsChanged();
}

int Document::mixerPreset() const
{
    if (m_mixerFlow != 1.)
        return 0;
    if (m_mixerWet == 0 && m_mixerLoad == 1 && m_mixerMix == 0)
        return 1;
    if (m_mixerWet == .1 && m_mixerLoad == .5 && m_mixerMix == .5)
        return 2;
    if (m_mixerWet == .5 && m_mixerLoad == .5 && m_mixerMix == .5)
        return 3;
    if (m_mixerWet == 1 && m_mixerLoad == .5 && m_mixerMix == 1)
        return 4;
    return 0;
}
void Document::applyMixerPreset(int _preset)
{
    if (_preset < 1 || _preset > 4)
        return;
    endStroke();
    m_mixerWet  = _preset == 1 ? 0. : _preset == 2 ? .1 : _preset == 3 ? .5 : 1.;
    m_mixerLoad = _preset == 1 ? 1. : .5;
    m_mixerMix  = _preset == 1 ? 0. : _preset == 4 ? 1. : .5;
    m_mixerFlow = 1.;
    reloadMixerPaint();
    emit settingsChanged();
}
void Document::reloadMixerPaint()
{
    m_mixerRemaining = m_mixerLoad > 0 ? 1. : 0.;
}
void Document::loadMixerBrush()
{
    endStroke();
    if (m_mixerPaint.isNull()) {
        m_mixerColor = m_color;
        m_mixerPaint = ImageUtils::blank(QSize(1, 1));
        m_mixerPaint.fill(m_color);
    }
    reloadMixerPaint();
    emit settingsChanged();
}
void Document::cleanMixerBrush()
{
    endStroke();
    m_mixerRemaining = 0.;
    m_mixerPickup    = {};
    emit settingsChanged();
}
void Document::sampleMixerBrush(qreal _x, qreal _y)
{
    if (!std::isfinite(_x) || !std::isfinite(_y))
        return;
    endStroke();
    const QPoint center(qFloor(_x), qFloor(_y));
    if (!QRect(QPoint{}, m_size).contains(center))
        return;
    PixelPlane source;
    bool       composed = m_mixerSampleAllLayers && !m_editingMask;
    if (!composed) {
        if (m_active >= 0 && m_active < m_layers.size() && m_editingMask &&
            !m_layers[m_active].mask.isNull())
            source = m_layers[m_active].mask;
        else if (m_active >= 0 && m_active < m_layers.size() && m_layers[m_active].kind == "pixel")
            source = m_layers[m_active].image;
        else
            composed = true;
    }
    if (!composed && source.isNull())
        return;
    const int size = qCeil(m_brushSize) + 4;
    if (m_mixerSolidOnly) {
        QColor color;
        if (composed) {
            const QImage sample = compositeRegion(QRect(center, QSize(1, 1)));
            if (sample.isNull())
                return;
            color = sample.pixelColor(0, 0);
        } else {
            const QPoint stored = source.size() == QSize(1, 1) ? QPoint{} : center;
            color               = source.pixelColor(stored.x(), stored.y());
        }
        m_mixerPaint = ImageUtils::blank(QSize(1, 1));
        m_mixerPaint.fill(color);
    } else
        m_mixerPaint =
            patch(source, QRect(center.x() - size / 2, center.y() - size / 2, size, size), m_size,
                  composed ? this : nullptr);
    if (m_mixerPaint.isNull())
        return;
    m_mixerColor  = averageColor(m_mixerPaint);
    m_mixerPickup = {};
    reloadMixerPaint();
    emit settingsChanged();
}
void Document::prepareMixerStroke()
{
    m_mixerRemainingBeforeStroke = m_mixerRemaining;
    m_mixerPickupBeforeStroke    = m_mixerPickup;
    const int size               = qCeil(m_brushSize) + 4;
    if (m_mixerPickup.isNull())
        m_mixerPickup = ImageUtils::blank(QSize(size, size));
    else if (m_mixerPickup.size() != QSize(size, size))
        m_mixerPickup =
            m_mixerPickup.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_mixerReservoirTip =
        m_mixerPaint.isNull()
            ? ImageUtils::blank(QSize(size, size))
            : m_mixerPaint.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_mixerTip = ImageUtils::blank(QSize(size, size));
    if (m_mixerPickup.isNull() || m_mixerReservoirTip.isNull() || m_mixerTip.isNull()) {
        m_strokeCoverageFailed = true;
        return;
    }
    QPainter painter(&m_mixerTip);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(size / 2., size / 2.);
    painter.rotate(m_brushAngle);
    painter.scale(1., m_brushRoundness);
    painter.setPen(Qt::NoPen);
    if (m_brushHardness >= .999)
        painter.setBrush(Qt::white);
    else {
        QRadialGradient gradient(QPointF{}, m_brushSize / 2.);
        gradient.setColorAt(m_brushHardness, Qt::white);
        gradient.setColorAt(1., Qt::transparent);
        painter.setBrush(gradient);
    }
    painter.drawEllipse(QPointF{}, m_brushSize / 2., m_brushSize / 2.);
    painter.end();
    // Hold source tile versions stable without preparing a full-sized image.
    m_mixerCompositeSource =
        m_mixerSampleAllLayers && !m_editingMask ? nativeCompositeSource(m_layers) : nullptr;
}
void Document::finishMixerStroke(bool _cancel)
{
    if (_cancel) {
        m_mixerRemaining = m_mixerRemainingBeforeStroke;
        m_mixerPickup    = m_mixerPickupBeforeStroke;
    } else {
        m_mixerLastEnd      = m_last;
        m_mixerLastEndValid = true;
        if (m_mixerAutoClean)
            m_mixerPickup = {};
        if (m_mixerAutoLoad)
            reloadMixerPaint();
        else if (m_mixerAutoClean)
            m_mixerRemaining = 0.;
    }
    m_mixerPickupBeforeStroke = {};
    m_mixerTip                = {};
    m_mixerReservoirTip       = {};
    m_mixerCompositeSource.reset();
    emit settingsChanged();
}
void Document::mixerDab(const QPointF &_point)
{
    if (m_mixerTip.isNull() || m_mixerFlow <= 0)
        return;
    PixelPlane &target = m_editingMask ? m_layers[m_active].mask : m_layers[m_active].image;
    const int   size   = m_mixerTip.width();
    const QRect dab(qFloor(_point.x()) - size / 2, qFloor(_point.y()) - size / 2, size, size);
    QRect       region = dab.intersected(QRect(QPoint{}, m_size));
    if (hasSelection())
        region = region.intersected(m_selection);
    if (region.isEmpty())
        return;
    const QImage sampled = patch(m_strokePlaneBase, dab, m_size, m_mixerCompositeSource.get());
    QImage       paint   = ImageUtils::blank(region.size());
    if (sampled.isNull() || paint.isNull()) {
        m_strokeCoverageFailed = true;
        return;
    }
    const QPoint offset = region.topLeft() - dab.topLeft();
    for (int y = 0; y < region.height(); ++y) {
        auto       *pickup = reinterpret_cast<QRgb *>(m_mixerPickup.scanLine(offset.y() + y));
        const auto *src    = reinterpret_cast<const QRgb *>(sampled.constScanLine(offset.y() + y));
        const auto *tip = reinterpret_cast<const QRgb *>(m_mixerTip.constScanLine(offset.y() + y));
        const auto *loaded =
            reinterpret_cast<const QRgb *>(m_mixerReservoirTip.constScanLine(offset.y() + y));
        auto *dst = reinterpret_cast<QRgb *>(paint.scanLine(y));
        for (int x = 0; x < region.width(); ++x) {
            const int   local    = offset.x() + x;
            const qreal coverage = qAlpha(tip[local]) / 255.;
            if (coverage <= 0 ||
                (hasSelection() && !m_selectionRegion.contains(region.topLeft() + QPoint(x, y))))
                continue;
            if (m_mixerWet > 0 && qAlpha(src[local])) {
                // A wet tip retains paint along its path. A dry tip neither
                // picks up image pixels nor contributes canvas paint.
                const qreal pickupRate = .05 + (1. - m_mixerWet) * .25;
                pickup[local] = qAlpha(pickup[local])
                                    ? interpolate(pickup[local], src[local], pickupRate * coverage)
                                    : src[local];
            }
            QRgb ink = attenuate(loaded[local], m_mixerRemaining);
            if (m_mixerWet > 0)
                ink = interpolate(ink, attenuate(pickup[local], m_mixerWet), m_mixerMix);
            if (m_editingMask) {
                const int alpha = qAlpha(ink);
                const int gray  = qGray(ink);
                ink             = qRgba(gray, gray, gray, alpha);
            }
            dst[x] = attenuate(ink, coverage * m_mixerFlow *
                                        selectionCoverage(region.x() + x, region.y() + y) / 255.);
        }
    }
    const QImage before = patch(target, region, m_size);
    if (before.isNull()) {
        m_strokeCoverageFailed = true;
        return;
    }
    QImage   output = before;
    QPainter painter(&output);
    painter.drawImage(0, 0, paint);
    painter.end();
    if (output != before) {
        const QImage pixels =
            m_editingMask ? output.convertToFormat(QImage::Format_Grayscale8) : output;
        if (!target.writeRegion(region.topLeft(), pixels, m_size)) {
            m_strokeCoverageFailed = true;
            return;
        }
        m_strokeBounds = m_strokeBounds.isEmpty() ? QRectF(region) : m_strokeBounds.united(region);
        if (!m_fullCompositeDirty)
            m_dirtyRegion = m_dirtyRegion.isEmpty() ? region : m_dirtyRegion.united(region);
    }
    // Charge determines the distance before the reservoir runs dry. Flow and
    // dab spacing determine consumption, rather than mouse event frequency.
    const qreal step = qMax(1., m_brushSize * m_brushSpacing) / m_brushSize;
    m_mixerRemaining =
        qMax(0., m_mixerRemaining - m_mixerFlow * step / (1. + 100. * m_mixerLoad * m_mixerLoad));
}
