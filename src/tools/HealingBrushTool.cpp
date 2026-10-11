#include "Document.h"
#include "ImageUtils.h"

#include <QPainter>
#include <QtMath>
#include <array>
#include <cmath>

namespace
{
using Tone = std::array<qreal, 3>;
struct ToneField {
    int           side;
    QVector<Tone> values;
};

QImage patchAt(const QImage &_image, const QRect &_rect)
{
    QImage   patch = ImageUtils::blank(_rect.size());
    QPainter painter(&patch);
    painter.drawImage(-_rect.topLeft(), _image);
    return patch;
}

// Extend the destination-minus-source colour difference from the brush border
// into the interior. The defect itself supplies no boundary values, so copied
// texture replaces it rather than bringing the defect back through a local blur.
// The harmonic field is small; full-resolution texture is never downsampled.
ToneField boundaryTone(const QImage &_source, const QImage &_reference, const QRegion &_selection,
                       bool _selected, int _diffusion, qreal _angle, qreal _roundness)
{
    const int     side = qBound(8, qCeil(_source.width() / (1. + .4 * _diffusion)), 28);
    ToneField     field{side, QVector<Tone>(side * side)};
    QVector<bool> free(side * side, false), valid(side * side, false);
    Tone          mean{};
    int           count  = 0;
    const qreal   cosine = std::cos(qDegreesToRadians(_angle));
    const qreal   sine   = std::sin(qDegreesToRadians(_angle));
    for (int gy = 0; gy < side; ++gy)
        for (int gx = 0; gx < side; ++gx) {
            const int    index = gy * side + gx;
            const QRect  cell(gx * _source.width() / side, gy * _source.height() / side,
                              (gx + 1) * _source.width() / side - gx * _source.width() / side,
                              (gy + 1) * _source.height() / side - gy * _source.height() / side);
            const QPoint centre = cell.center();
            const qreal  dx = (centre.x() + .5 - _source.width() / 2.) / (_source.width() / 2. - 2);
            const qreal  dy =
                (centre.y() + .5 - _source.height() / 2.) / (_source.height() / 2. - 2);
            const qreal u             = cosine * dx + sine * dy;
            const qreal v             = (-sine * dx + cosine * dy) / _roundness;
            const bool  selectionEdge = _selected && (!_selection.contains(cell.topLeft()) ||
                                                      !_selection.contains(cell.bottomRight()));
            free[index]               = gx > 0 && gy > 0 && gx + 1 < side && gy + 1 < side &&
                                        u * u + v * v < .8 && !selectionEdge &&
                                        (!_selected || _selection.contains(centre));
            if (free[index])
                continue;
            int samples = 0;
            for (int y = cell.top(); y <= cell.bottom(); ++y) {
                const auto *source    = reinterpret_cast<const QRgb *>(_source.constScanLine(y));
                const auto *reference = reinterpret_cast<const QRgb *>(_reference.constScanLine(y));
                for (int x = cell.left(); x <= cell.right(); ++x) {
                    if ((_selected && !_selection.contains(QPoint(x, y))) ||
                        qAlpha(source[x]) == 0 || qAlpha(reference[x]) == 0)
                        continue;
                    const QRgb a = qUnpremultiply(source[x]), b = qUnpremultiply(reference[x]);
                    field.values[index][0] += qRed(b) - qRed(a);
                    field.values[index][1] += qGreen(b) - qGreen(a);
                    field.values[index][2] += qBlue(b) - qBlue(a);
                    ++samples;
                }
            }
            if (samples) {
                for (int c = 0; c < 3; ++c) {
                    field.values[index][c] /= samples;
                    mean[c] += field.values[index][c];
                }
                ++count;
                valid[index] = true;
            }
        }
    if (count)
        for (auto &channel : mean)
            channel /= count;
    for (int i = 0; i < side * side; ++i)
        if (!valid[i])
            field.values[i] = mean;
    // Successive over-relaxation converges on this bounded grid independently
    // of document size. Diffusion smooths the boundary at a coarser grid scale.
    for (int pass = 0; pass < 40 + _diffusion * 8; ++pass)
        for (int y = 1; y + 1 < side; ++y)
            for (int x = 1; x + 1 < side; ++x) {
                const int index = y * side + x;
                if (!free[index])
                    continue;
                for (int c = 0; c < 3; ++c) {
                    const qreal average =
                        (field.values[index - 1][c] + field.values[index + 1][c] +
                         field.values[index - side][c] + field.values[index + side][c]) /
                        4.;
                    field.values[index][c] += 1.5 * (average - field.values[index][c]);
                }
            }
    return field;
}

Tone toneAt(const ToneField &_field, qreal _x, qreal _y)
{
    _x            = qBound(0., _x, qreal(_field.side - 1));
    _y            = qBound(0., _y, qreal(_field.side - 1));
    const int   x = qFloor(_x), y = qFloor(_y), right = qMin(x + 1, _field.side - 1),
                bottom = qMin(y + 1, _field.side - 1);
    const qreal tx = _x - x, ty = _y - y;
    Tone        result{};
    for (int c = 0; c < 3; ++c) {
        const qreal top = _field.values[y * _field.side + x][c] * (1 - tx) +
                          _field.values[y * _field.side + right][c] * tx;
        const qreal low = _field.values[bottom * _field.side + x][c] * (1 - tx) +
                          _field.values[bottom * _field.side + right][c] * tx;
        result[c]       = top * (1 - ty) + low * ty;
    }
    return result;
}
} // namespace

void Document::setHealingAligned(bool _value)
{
    endStroke();
    m_healingAligned     = _value;
    m_healingOffsetValid = false;
    emit settingsChanged();
}
void Document::setHealingSampleMode(int _value)
{
    endStroke();
    m_healingSampleMode = qBound(0, _value, 2);
    emit settingsChanged();
}
void Document::setHealingDiffusion(int _value)
{
    endStroke();
    m_healingDiffusion = qBound(1, _value, 7);
    emit settingsChanged();
}
void Document::setHealingMode(int _value)
{
    endStroke();
    m_healingMode = qBound(0, _value, 7);
    emit settingsChanged();
}
void Document::setHealingIgnoreAdjustments(bool _value)
{
    endStroke();
    m_healingIgnoreAdjustments = _value;
    emit settingsChanged();
}
void Document::setHealingSource(qreal _x, qreal _y)
{
    if (!std::isfinite(_x) || !std::isfinite(_y) ||
        !QRectF(QPointF{}, m_size).contains(QPointF(_x, _y)))
        return;
    endStroke();
    m_healingSource      = QPointF(qFloor(_x), qFloor(_y));
    m_healingSourceValid = true;
    m_healingOffsetValid = false;
    emit settingsChanged();
}

bool Document::prepareHealingStroke()
{
    if (!m_healingSourceValid)
        return fail(tr("Option/Alt-clic sur l’image pour définir la source du correcteur."));
    m_healingOffsetBeforeStroke      = m_healingOffset;
    m_healingOffsetBeforeStrokeValid = m_healingOffsetValid;
    if (!m_healingAligned || !m_healingOffsetValid) {
        m_healingOffset      = m_healingSource - m_anchor;
        m_healingOffsetValid = true;
        emit settingsChanged();
    }
    m_healingSample = sampleHealingLayers(m_healingSampleMode, m_healingIgnoreAdjustments);
    m_healingTip    = makeHealingTip();
    return true;
}

QImage Document::sampleHealingLayers(int _sampleMode, bool _ignoreAdjustments)
{
    // A shared, fixed source per gesture prevents overlapping edits feeding
    // back into the sample. All-layer composition occurs once, never per dab.
    if (_sampleMode == 0)
        return *paintTarget();
    else if (_sampleMode == 2 && !_ignoreAdjustments)
        return composite();
    else {
        auto layers = m_layers;
        for (int i = 0; i < layers.size(); ++i)
            if ((_sampleMode == 1 && i > m_active && layers[i].kind != "group") ||
                (_ignoreAdjustments && layers[i].kind == "adjustment"))
                layers[i].visible = false;
        return renderPreviewSnapshot(std::move(layers), m_size, m_size, false);
    }
}

QImage Document::makeHealingTip() const
{
    const int size = qCeil(m_brushSize) + 4;
    QImage    tip  = ImageUtils::blank(QSize(size, size));
    QPainter  painter(&tip);
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
    painter.end();
    return tip;
}

void Document::healingDab(const QPointF &_point)
{
    applyHealingDab(_point, m_healingSample, m_healingTip, m_healingOffset.toPoint(), m_healingMode,
                    m_healingDiffusion);
}

void Document::applyHealingDab(const QPointF &_point, const QImage &_sample, const QImage &_tip,
                               const QPoint &_sourceOffset, int _mode, int _diffusion,
                               bool _selectionBoundary)
{
    QImage *target = paintTarget();
    if (!target || _sample.isNull() || m_brushFlow <= 0 || m_brushOpacity <= 0)
        return;
    const int   size = _tip.width();
    const QRect dab(qFloor(_point.x()) - size / 2, qFloor(_point.y()) - size / 2, size, size);
    QRect       region = dab.intersected(target->rect());
    if (hasSelection())
        region = region.intersected(m_selectionRegion.boundingRect());
    if (region.isEmpty())
        return;
    const QRect     sourceRect(dab.topLeft() + _sourceOffset, dab.size());
    const QImage    sampled   = patchAt(_sample, sourceRect);
    const QImage    reference = patchAt(_sample, dab);
    const ToneField field =
        _mode == 1 ? ToneField{0, {}}
                   : boundaryTone(sampled, reference, m_selectionRegion.translated(-dab.topLeft()),
                                  hasSelection() && _selectionBoundary, _diffusion, m_brushAngle,
                                  m_brushRoundness);
    const QPoint offset = region.topLeft() - dab.topLeft();
    QImage       healed = ImageUtils::blank(region.size());
    for (int y = 0; y < region.height(); ++y) {
        const auto *source = reinterpret_cast<const QRgb *>(sampled.constScanLine(offset.y() + y));
        auto       *paint  = reinterpret_cast<QRgb *>(healed.scanLine(y));
        for (int x = 0; x < region.width(); ++x) {
            const QRgb colour = qUnpremultiply(source[offset.x() + x]);
            if (!qAlpha(colour))
                continue;
            const Tone correction =
                field.side ? toneAt(field, (offset.x() + x + .5) * field.side / size - .5,
                                    (offset.y() + y + .5) * field.side / size - .5)
                           : Tone{};
            paint[x] = qPremultiply(qRgba(qBound(0, qRound(qRed(colour) + correction[0]), 255),
                                          qBound(0, qRound(qGreen(colour) + correction[1]), 255),
                                          qBound(0, qRound(qBlue(colour) + correction[2]), 255),
                                          qAlpha(colour)));
        }
    }
    QImage output = target->copy(region).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    static constexpr int modes[]{0, 0, 3, 8, 2, 7, 25, 26};
    const QImage effect = _mode == 1 ? healed : blendImages(output, healed, modes[_mode], 1.);
    QRect        modified;
    for (int y = 0; y < region.height(); ++y) {
        auto       *destination = reinterpret_cast<QRgb *>(output.scanLine(y));
        const auto *paint       = reinterpret_cast<const QRgb *>(effect.constScanLine(y));
        const auto *source      = reinterpret_cast<const QRgb *>(healed.constScanLine(y));
        const auto *tip = reinterpret_cast<const QRgb *>(_tip.constScanLine(offset.y() + y));
        for (int x = 0; x < region.width(); ++x) {
            if (!qAlpha(source[x]) ||
                (hasSelection() && !m_selectionRegion.contains(region.topLeft() + QPoint(x, y))))
                continue;
            const qreal amount = qAlpha(tip[offset.x() + x]) / 255. * m_brushFlow * m_brushOpacity;
            if (amount <= 0)
                continue;
            const QRgb before = destination[x];
            const auto mix = [amount](int _a, int _b) { return qRound(_a + (_b - _a) * amount); };
            destination[x] =
                qRgba(mix(qRed(before), qRed(paint[x])), mix(qGreen(before), qGreen(paint[x])),
                      mix(qBlue(before), qBlue(paint[x])), mix(qAlpha(before), qAlpha(paint[x])));
            if (destination[x] != before)
                modified |= QRect(x, y, 1, 1);
        }
    }
    if (modified.isEmpty())
        return;
    const QRect dirty = modified.translated(region.topLeft());
    QPainter    painter(target);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(dirty.topLeft(), output, modified);
    painter.end();
    m_strokeBounds = m_strokeBounds.isEmpty() ? QRectF(dirty) : m_strokeBounds.united(dirty);
    m_dirtyRegion  = m_dirtyRegion.isEmpty() ? dirty : m_dirtyRegion.united(dirty);
}

void Document::finishHealingStroke(bool _cancel)
{
    if (_cancel) {
        m_healingOffset      = m_healingOffsetBeforeStroke;
        m_healingOffsetValid = m_healingOffsetBeforeStrokeValid;
        emit settingsChanged();
    }
    m_healingSample = {};
    m_healingTip    = {};
}
