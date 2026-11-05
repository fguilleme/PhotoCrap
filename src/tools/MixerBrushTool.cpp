#include "Document.h"
#include "ImageUtils.h"

#include <QBuffer>
#include <QPainter>
#include <QtMath>
#include <array>
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
QRgb contactColor(const QImage &_sample, const QImage &_tip, const QRect &_region,
                  const QPoint &_offset, const Document &_document)
{
    qint64 red = 0, green = 0, blue = 0, alpha = 0, weight = 0;
    for (int y = 0; y < _region.height(); ++y) {
        const auto *sample = reinterpret_cast<const QRgb *>(_sample.constScanLine(_offset.y() + y));
        const auto *tip    = reinterpret_cast<const QRgb *>(_tip.constScanLine(_offset.y() + y));
        for (int x = 0; x < _region.width(); ++x) {
            const auto   pixel  = sample[_offset.x() + x];
            const qint64 amount = qAlpha(tip[_offset.x() + x]) *
                                  _document.selectionCoverage(_region.x() + x, _region.y() + y);
            red += qRed(pixel) * amount;
            green += qGreen(pixel) * amount;
            blue += qBlue(pixel) * amount;
            alpha += qAlpha(pixel) * amount;
            weight += amount;
        }
    }
    return weight ? qRgba(int(red / weight), int(green / weight), int(blue / weight),
                          int(alpha / weight))
                  : 0;
}
struct MixerPreset {
    int         id;
    const char *name;
    double      wet, load, mix;
};
// Stable IDs retain the four original base combinations. The menu follows
// Adobe's dry/moist/wet families; these are PhotoCrap's calibrated values.
constexpr std::array<MixerPreset, 12> Presets{
    {{1, QT_TRANSLATE_NOOP("Document", "Sec"), 0, .5, 0},
     {5, QT_TRANSLATE_NOOP("Document", "Sec, faible charge"), 0, .1, 0},
     {6, QT_TRANSLATE_NOOP("Document", "Sec, forte charge"), 0, 1, 0},
     {2, QT_TRANSLATE_NOOP("Document", "Humide"), .1, .5, .5},
     {7, QT_TRANSLATE_NOOP("Document", "Humide, faible mélange"), .1, .5, .1},
     {8, QT_TRANSLATE_NOOP("Document", "Humide, fort mélange"), .1, .5, .9},
     {3, QT_TRANSLATE_NOOP("Document", "Mouillé"), .5, .5, .5},
     {9, QT_TRANSLATE_NOOP("Document", "Mouillé, faible mélange"), .5, .5, .1},
     {10, QT_TRANSLATE_NOOP("Document", "Mouillé, fort mélange"), .5, .5, .9},
     {4, QT_TRANSLATE_NOOP("Document", "Très mouillé"), 1, .5, .5},
     {11, QT_TRANSLATE_NOOP("Document", "Très mouillé, faible mélange"), 1, .5, .1},
     {12, QT_TRANSLATE_NOOP("Document", "Très mouillé, fort mélange"), 1, .5, .9}}};
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
    for (const auto &preset : Presets)
        if (qAbs(m_mixerWet - preset.wet) < 1e-6 && qAbs(m_mixerLoad - preset.load) < 1e-6 &&
            qAbs(m_mixerMix - preset.mix) < 1e-6)
            return preset.id;
    return 0;
}
QVariantList Document::mixerPresets() const
{
    QVariantList result{QVariantMap{{"id", 0}, {"source", "Personnalisé"}}};
    for (const auto &preset : Presets)
        result.append(QVariantMap{{"id", preset.id}, {"source", preset.name}});
    return result;
}
void Document::applyMixerPreset(int _preset)
{
    for (const auto &preset : Presets) {
        if (preset.id != _preset)
            continue;
        endStroke();
        m_mixerWet  = preset.wet;
        m_mixerLoad = preset.load;
        m_mixerMix  = preset.mix;
        // A combination is Wet/Load/Mix. Flow is an independent paint setting.
        reloadMixerPaint();
        emit settingsChanged();
        return;
    }
}
QString Document::mixerLoadPreview() const
{
    if (!mixerLoaded())
        return {};
    if (m_mixerPreviewKey != m_mixerPaint.cacheKey()) {
        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        const auto image =
            m_mixerPaint.scaled(40, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (!image.save(&buffer, "PNG"))
            return {};
        m_mixerPreviewUrl =
            "data:image/png;base64," + QString::fromLatin1(buffer.data().toBase64());
        m_mixerPreviewKey = m_mixerPaint.cacheKey();
    }
    return m_mixerPreviewUrl;
}
void Document::setMixerAirbrush(bool _value)
{
    if (m_mixerAirbrush == _value)
        return;
    m_mixerAirbrush = _value;
    emit settingsChanged();
}
void Document::accumulateMixerBrush()
{
    if (!m_mixerAirbrush || m_tool != "mixerBrush" || !m_drawing || m_strokeCoverageFailed)
        return;
    m_strokeBounds = {};
    mixerDab(m_last);
    if (m_strokeCoverageFailed) {
        cancelStroke();
        fail(tr("Mémoire insuffisante pour la couverture du trait."));
        return;
    }
    if (!m_strokeBounds.isEmpty()) {
        m_strokeChanged = true;
        changed(false);
    }
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
    // Hold other layers stable without preparing a full-sized image. The active
    // plane is refreshed before each dab so pickup sees freshly deposited paint.
    if (m_mixerWet > 0 && m_mixerSampleAllLayers && !m_editingMask)
        m_mixerCompositeSource = nativeCompositeSource(m_layers);
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
    const bool wet = m_mixerWet > 0;
    QImage     sampled;
    if (wet) {
        // The pickup must see the paint left by earlier dabs. Keep the
        // other layers stable while updating the active plane.
        if (m_mixerCompositeSource)
            m_mixerCompositeSource->m_layers[m_active].image = target;
        sampled = patch(target, dab, m_size, m_mixerCompositeSource.get());
    }
    QImage paint = ImageUtils::blank(region.size());
    if ((wet && sampled.isNull()) || paint.isNull()) {
        m_strokeCoverageFailed = true;
        return;
    }
    const QPoint offset = region.topLeft() - dab.topLeft();
    const QRgb   contact = wet ? contactColor(sampled, m_mixerTip, region, offset, *this) : 0;
    for (int y = 0; y < region.height(); ++y) {
        auto       *pickup = reinterpret_cast<QRgb *>(m_mixerPickup.scanLine(offset.y() + y));
        const auto *src =
            wet ? reinterpret_cast<const QRgb *>(sampled.constScanLine(offset.y() + y)) : nullptr;
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
            const QRgb incoming = wet ? interpolate(src[local], contact, m_mixerWet * .25) : 0;
            if (qAlpha(incoming)) {
                // A wet tip retains paint along its path. A dry tip neither
                // picks up image pixels nor contributes canvas paint.
                const qreal pickupRate = .05 + (1. - m_mixerWet) * .25;
                // Exchange a small amount across the wet contact patch as well
                // as along each tip coordinate. The leading edge can then feed
                // fresh canvas colour to the well instead of endlessly picking
                // up only the opaque paint deposited by the previous dab.
                pickup[local] = qAlpha(pickup[local])
                                    ? interpolate(pickup[local], incoming, pickupRate * coverage)
                                    : incoming;
            }
            QRgb ink = attenuate(loaded[local], m_mixerRemaining);
            if (wet)
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
    const qreal step           = qMax(1., m_brushSize * m_brushSpacing) / m_brushSize;
    const qreal reservoirShare = wet ? 1. - m_mixerMix : 1.;
    m_mixerRemaining = qMax(0., m_mixerRemaining - m_mixerFlow * reservoirShare * step /
                                                       (1. + 100. * m_mixerLoad * m_mixerLoad));
}
