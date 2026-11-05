#include "Document.h"
#include "ImageSampling.h"
#include "ImageUtils.h"
#include "filters/BlendMath.h"
#include "filters/NoiseMath.h"
#include "filters/ToneMappingTable.h"
#include "render/LayerVisibility.h"
#include <QDataStream>
#include <algorithm>

PhotoCrap::Render::LayerStack Document::gpuLayerStack() const
{
    using namespace PhotoCrap::Render;
    LayerStack stack;
    if (m_displayChannel || m_maskOnlyView || m_size.isEmpty())
        return stack;
    stack.size = m_size;
    stack.bits = m_channelBits;
    QHash<QString, bool> groups;
    for (const auto &layer : m_layers)
        if (layer.kind == "group")
            groups.insert(layer.groupId, layer.visible);
    bool hasClippingBase = false, opaqueBackdrop = false;
    for (int index = LayerVisibility::opaqueCoverIndex(m_layers, m_size); index < m_layers.size();
         ++index) {
        const auto &layer = m_layers[index];
        if (layer.kind == "group" || !LayerVisibility::contributes(layer) ||
            (!layer.groupId.isEmpty() && !groups.value(layer.groupId, true))) {
            if (layer.kind == "group" || !layer.clippingMask)
                hasClippingBase = false;
            continue;
        }
        // A clipped layer with no base has no effect. Skip it before checking
        // GPU eligibility, so a hidden base needs no separate GPU reset pass.
        if (layer.clippingMask && !hasClippingBase)
            continue;
        // The CPU compositor treats an untouched frequency pair as a single
        // normal draw of its source. Use the same native pixels on the GPU.
        if (LayerVisibility::reconstructsFrequencySource(m_layers, index) &&
            layer.source.size() == m_size && layer.source.devicePixelRatio() == 1.) {
            StackLayer entry;
            entry.image = layer.source;
            stack.layers.append(std::move(entry));
            ++index;
            continue;
        }
        // Unsupported operations use the authoritative CPU compositor. No
        // partially composed result may be presented as the complete stack.
        const bool solid = layer.kind == "adjustment" && layer.adjustmentType == "solidColor";
        const bool noise = layer.kind == "adjustment" && layer.adjustmentType == "noise";
        const bool mosaic = layer.kind == "adjustment" && layer.adjustmentType == "mosaic";
        if (noise && !opaqueBackdrop)
            return {}; // Match Qt's full-image straight-alpha conversion on translucent piles.
        const bool tonal =
            noise || mosaic ||
            (layer.kind == "adjustment" &&
             PhotoCrap::Filters::gpuTonePreviewTypes().contains(layer.adjustmentType) &&
             (layer.adjustmentType != "selectiveColor" ||
              layer.parameters.contains("redsCyanRed")));
        const bool                                 masked = LayerVisibility::needsMask(layer);
        static const QHash<QString, SpatialFilter> spatialTypes{
            {"boxBlur", SpatialFilter::Box},
            {"gaussianBlur", SpatialFilter::Gaussian},
            {"motionBlur", SpatialFilter::Motion},
            {"unsharpMask", SpatialFilter::Unsharp},
            {"highPass", SpatialFilter::HighPass}};
        const auto spatial = layer.kind == "adjustment"
                                 ? spatialTypes.value(layer.adjustmentType, SpatialFilter::None)
                                 : SpatialFilter::None;
        if ((layer.kind != "pixel" && !solid && !tonal && layer.kind != "text" &&
             layer.kind != "shape" && spatial == SpatialFilter::None) ||
            (!solid && !tonal && spatial == SpatialFilter::None && layer.image.isNull()))
            return {};
        // Discontinuous decisions must not amplify an approximate earlier pass.
        // Pixel/text/shape sources have exact stored native alpha; analytic and
        // spatial adjustments retain their existing CPU/isolated-preview path.
        if ((layer.blend == 1 || layer.blend == 18) && (tonal || spatial != SpatialFilter::None))
            return {};
        if ((layer.blend == 1 || layer.blend == 18) && layer.clippingMask)
            return {};
        if (layer.blend == 1 && masked &&
            ((m_channelBits == 16 &&
              layer.parameters.value("_maskDensity", 100.).toDouble() != 100.) ||
             (!layer.mask.isUniform() &&
              layer.parameters.value("_maskFeather", 0.).toDouble() >= .5)))
            return {};
        if (layer.blend == 18) {
            if (masked || (!solid && !layer.image.knownOpaque()))
                return {};
            if (stack.layers.size() > 1)
                return {};
            if (!stack.layers.isEmpty()) {
                const auto &base = stack.layers.first();
                if (!base.image.knownOpaque() || base.opacity != 1 || base.blend != 0 ||
                    base.clipped || !base.mask.isNull() || base.tonal ||
                    base.spatial.type != SpatialFilter::None)
                    return {};
            }
        }
        StackLayer entry;
        if (noise) {
            using namespace PhotoCrap::Filters;
            // Existing projects use the CPU build's argument evaluation order
            // for RGB. Read that order once from the actual native filter.
            const auto readOrder = [](bool wide) {
                constexpr double amount = 50.;
                const QPoint     origin(1, 2);
                QImage           gray(1, 1,
                                      wide ? QImage::Format_RGBA64_Premultiplied
                                           : QImage::Format_ARGB32_Premultiplied);
                gray.fill(QColor(128, 128, 128));
                const auto               pixel = noiseImage(gray, amount, origin).pixelColor(0, 0);
                const int                amplitude = noiseAmplitude(amount, wide);
                auto                     hash      = noiseSeed(origin.x(), origin.y());
                const std::array<int, 3> deltas{noiseDelta(hash, amplitude),
                                                noiseDelta(hash, amplitude),
                                                noiseDelta(hash, amplitude)};
                const auto               index = [&](int channel) {
                    const auto it = std::find(deltas.cbegin(), deltas.cend(), channel - 128);
                    return it == deltas.cend() ? -1.f : float(it - deltas.cbegin());
                };
                return QVector3D(index(pixel.red()), index(pixel.green()), index(pixel.blue()));
            };
            static const auto order8 = readOrder(false), order16 = readOrder(true);
            const auto        order = m_channelBits == 16 ? order16 : order8;
            if (order.x() < 0 || order.y() < 0 || order.z() < 0)
                return {};
            const auto amount    = layer.parameters.value("amount", 8.).toDouble();
            entry.tonal          = true;
            entry.tone.mode      = ToneMappingMode::StackNoise;
            const auto amplitude = noiseAmplitude(amount, m_channelBits == 16);
            entry.tone.color     = QVector4D(float(amplitude), order.x(), order.y(), order.z());
        } else if (mosaic) {
            using namespace PhotoCrap::ImageSampling;
            const int cell  = qBound(1, layer.parameters.value("cellSize", 12).toInt(), 200);
            entry.tonal     = true;
            entry.tone.mode = PhotoCrap::Filters::ToneMappingMode::StackMosaic;
            entry.tone.color.setX(float(cell));
            if (cell > 1) {
                stack.halo += cell + 2;
                QByteArray  key;
                QDataStream stream(&key, QIODevice::WriteOnly);
                stream << QStringLiteral("stack-mosaic") << m_size << m_channelBits << cell;
                const auto cached = m_gpuChannelTables.object(key);
                QImage     table  = cached ? *cached : QImage{};
                if (table.isNull()) {
                    const auto axes = mosaicSampleAxes(m_size, mosaicReducedSize(m_size, cell),
                                                       m_channelBits == 16);
                    if (axes[0].size() != m_size.width() || axes[1].size() != m_size.height())
                        return {};
                    // Alternate rows for X/Y, with each coordinate packed in
                    // two color channels to preserve the full native index.
                    constexpr int columns = 256;
                    const int     span    = qMax(m_size.width(), m_size.height());
                    table                 = QImage(columns, 2 * ((span + columns - 1) / columns),
                                                   QImage::Format_ARGB32);
                    if (table.isNull())
                        return {};
                    table.fill(Qt::black);
                    for (int axis = 0; axis < 2; ++axis)
                        for (int i = 0; i < axes[axis].size(); ++i) {
                            auto *row =
                                reinterpret_cast<QRgb *>(table.scanLine(2 * (i / columns) + axis));
                            const int value  = axes[axis][i];
                            row[i % columns] = qRgb(value & 255, value >> 8, 0);
                        }
                    m_gpuChannelTables.insert(key, new QImage(table), int(table.sizeInBytes()));
                }
                entry.toneTable = table;
            }
        } else if (tonal) {
            using PhotoCrap::Filters::ToneMappingMode;
            entry.tonal = true;
            entry.tone =
                PhotoCrap::Filters::toneMappingSettings(layer.adjustmentType, layer.parameters);
            const auto mode = entry.tone.mode;
            const bool lookup =
                mode == ToneMappingMode::Channels || mode == ToneMappingMode::Luminance ||
                mode == ToneMappingMode::GradientMap || mode == ToneMappingMode::BlackWhite ||
                mode == ToneMappingMode::Threshold;
            if (lookup) {
                auto parameters = layer.parameters;
                for (auto it = parameters.begin(); it != parameters.end();)
                    if (it.key().startsWith('_'))
                        it = parameters.erase(it);
                    else
                        ++it;
                QByteArray  key;
                QDataStream stream(&key, QIODevice::WriteOnly);
                stream << m_channelBits << layer.adjustmentType << parameters;
                const auto cached = m_gpuChannelTables.object(key);
                auto       table  = cached ? *cached : QImage{};
                if (table.isNull()) {
                    QImage ramp(256, m_channelBits == 16 ? 256 : 1, workingFormat());
                    if (ramp.isNull())
                        return {};
                    for (int y = 0; y < ramp.height(); ++y)
                        for (int x = 0; x < 256; ++x) {
                            const int value = x + y * 256;
                            if (m_channelBits == 16)
                                reinterpret_cast<QRgba64 *>(ramp.scanLine(y))[x] =
                                    QRgba64::fromRgba64(value, value, value, 65535);
                            else
                                reinterpret_cast<QRgb *>(ramp.scanLine(y))[x] =
                                    qRgb(value, value, value);
                        }
                    table = adjustedImage(ramp, layer.adjustmentType, parameters);
                    if (table.isNull())
                        return {};
                    m_gpuChannelTables.insert(key, new QImage(table), int(table.sizeInBytes()));
                }
                entry.toneTable = table;
            }
        }
        if (layer.blend == 1 || layer.blend == 18) {
            QByteArray  key;
            QDataStream stream(&key, QIODevice::WriteOnly);
            stream << QStringLiteral("stack-blend") << m_channelBits << layer.blend;
            if (layer.blend == 1)
                stream << layer.opacity;
            const auto cached = m_gpuChannelTables.object(key);
            QImage     table  = cached ? *cached : QImage{};
            if (table.isNull()) {
                table = QImage(256, m_channelBits == 16 ? 256 : 1, QImage::Format_RGBA32FPx4);
                if (table.isNull())
                    return {};
                const int maximum = m_channelBits == 16 ? 65535 : 255;
                for (int i = 0; i <= maximum; ++i) {
                    const double source = (m_channelBits == 16 ? i / 257. : double(i)) / 255.;
                    const double base =
                        (m_channelBits == 16 ? (maximum - i) / 257. : double(maximum - i)) / 255.;
                    const quint16 value =
                        layer.blend == 1 ? quint16(quint32(source * layer.opacity * 65535.))
                        : PhotoCrap::Filters::blendChannel(base, source, 18) > 0 ? 65535
                                                                                 : 0;
                    const quint64 reciprocal =
                        i == 0 ? 0
                        : m_channelBits == 16
                            ? (Q_UINT64_C(0xffff00008000) + quint64(i / 2)) / quint64(i)
                            : qt_inv_premul_factor[i];
                    auto *pixel =
                        reinterpret_cast<float *>(table.scanLine(i / 256)) + 4 * (i % 256);
                    pixel[0] = float(value) / 65535.f;
                    pixel[1] = float(quint16(reciprocal)) / 65535.f;
                    pixel[2] = float(quint16(reciprocal >> 16)) / 65535.f;
                    pixel[3] = float(quint16(reciprocal >> 32)) / 65535.f;
                }
                m_gpuChannelTables.insert(key, new QImage(table), int(table.sizeInBytes()));
            }
            entry.blendTable = table;
        }
        entry.spatial.type = spatial;
        if (spatial != SpatialFilter::None) {
            const double radius  = layer.parameters
                                       .value("radius", spatial == SpatialFilter::Unsharp    ? 2.
                                                        : spatial == SpatialFilter::HighPass ? 4.
                                                                                             : 8.)
                                       .toDouble();
            entry.spatial.radius = qBound(
                spatial == SpatialFilter::Box || (spatial == SpatialFilter::Gaussian && radius <= 0)
                    ? 0
                    : 1,
                qRound(radius), 100);
            entry.spatial.distance =
                float(qBound(0., layer.parameters.value("distance", 20.).toDouble(), 200.));
            entry.spatial.angle = float(layer.parameters.value("angle", 0.).toDouble());
            entry.spatial.amount =
                float(qBound(0., layer.parameters.value("amount", 100.).toDouble(), 300.));
            entry.spatial.threshold =
                qBound(0, layer.parameters.value("threshold", 0).toInt(), 255);
#ifdef PHOTOCRAP_USE_ACCELERATE
            entry.spatial.roundBox2D = m_channelBits == 8;
#endif
            stack.halo += entry.spatial.halo();
        }
        entry.image = layer.image;
        if (solid) {
            QImage color(1, 1, workingFormat());
            color.fill(QColor(layer.parameters.value("color", "#000000").toString()));
            entry.image = color;
        }
        if (layer.blend == 18 && !entry.image.knownOpaque())
            return {};
        entry.mask = masked ? layer.mask : PixelPlane{};
        if (masked && !layer.mask.isUniform()) {
            const int radius =
                qBound(0, qRound(layer.parameters.value("_maskFeather", 0.).toDouble()), 100);
            if (radius > 0) {
                entry.maskFilter.type   = SpatialFilter::Box;
                entry.maskFilter.radius = radius;
#ifdef PHOTOCRAP_USE_ACCELERATE
                entry.maskFilter.roundBox2D = true;
#endif
                stack.halo += radius;
            }
        }
        entry.opacity = float(qBound(0., layer.opacity, 1.));
        entry.density =
            float(qBound(0., layer.parameters.value("_maskDensity", 100.).toDouble() / 100., 1.));
        entry.blend   = layer.blend;
        entry.clipped = layer.clippingMask;
        opaqueBackdrop =
            opaqueBackdrop || (!entry.clipped && !entry.tonal && spatial == SpatialFilter::None &&
                               layer.opacity == 1 && entry.blend == 0 && !masked &&
                               (entry.image.size() == m_size || ImageUtils::compact(entry.image)) &&
                               entry.image.devicePixelRatio() == 1. && entry.image.knownOpaque());
        stack.layers.append(std::move(entry));
        if (!layer.clippingMask)
            hasClippingBase = true;
    }
    if (stack.layers.isEmpty())
        stack.layers.append(StackLayer{.reset = true});
    stack.valid = true;
    return stack;
}

void Document::setGpuTonePreviewEnabled(bool _enabled)
{
    if (m_gpuToneEnabled == _enabled)
        return;
    m_gpuToneEnabled = _enabled;
    if (!_enabled) {
        invalidateGpuTonePreview();
        if (m_previewingLayer && m_layerPreviewChanged)
            requestPreviewRender(true);
    }
}

void Document::setGpuStackDisplayActive(bool active)
{
    if (m_gpuStackDisplayActive == active)
        return;
    m_gpuStackDisplayActive = active;
    if (!active && m_gpuStackAuxTimer.isActive()) {
        m_gpuStackAuxTimer.stop();
        QTimer::singleShot(0, this, &Document::startPendingPreviewRender);
    }
}

void Document::setGpuToneNearestSampling(bool _nearest)
{
    if (m_gpuToneNearestSampling == _nearest)
        return;
    m_gpuToneNearestSampling = _nearest;
    if (m_active >= 0 && m_active < m_layers.size() &&
        (m_layers[m_active].blend == 1 || m_layers[m_active].blend == 18)) {
        invalidateGpuTonePreview();
        if (m_previewingLayer)
            requestPreviewRender(true);
    }
}

bool Document::gpuToneEligible() const
{
    if (!m_gpuToneEnabled || m_displayChannel != 0 || m_maskOnlyView || m_active < 0 ||
        m_active != m_layers.size() - 1)
        return false;
    const auto &layer = m_layers[m_active];
    const bool  fullCoverage =
        !layer.maskEnabled || layer.mask.isNull() ||
        layer.parameters.value("_maskDensity", 100.).toDouble() <= 0 ||
        (ImageUtils::compact(layer.mask) && layer.mask.constScanLine(0)[0] == 255);
    return layer.kind == QStringLiteral("adjustment") && layer.visible &&
           ((layer.blend != 1 && layer.blend != 18) || m_gpuToneNearestSampling) &&
           PhotoCrap::Filters::gpuToneBlendSupported(layer.adjustmentType, layer.blend,
                                                     fullCoverage) &&
           layer.groupId.isEmpty() && !layer.clippingMask &&
           PhotoCrap::Filters::gpuTonePreviewTypes().contains(layer.adjustmentType) &&
           (layer.adjustmentType != QStringLiteral("selectiveColor") ||
            layer.parameters.contains(QStringLiteral("redsCyanRed")));
}

bool Document::gpuTonePreviewActive() const
{
    return m_gpuToneActive && gpuToneEligible();
}

PhotoCrap::Filters::ToneMappingSettings Document::gpuTonePreviewSettings() const
{
    if (!gpuTonePreviewActive())
        return {};
    const auto &layer = m_layers[m_active];
    auto settings = PhotoCrap::Filters::toneMappingSettings(layer.adjustmentType, layer.parameters);
    settings.blendMode     = layer.blend;
    settings.effectOpacity = float(qBound(0., layer.opacity, 1.));
    if (layer.maskEnabled && !layer.mask.isNull()) {
        const double density =
            qBound(0., layer.parameters.value("_maskDensity", 100.).toDouble() / 100., 1.);
        settings.maskCoverage =
            ImageUtils::compact(layer.mask)
                ? float(qRound(255 - (255 - layer.mask.constScanLine(0)[0]) * density))
                : float(density);
    }
    if (layer.blend == 1)
        settings.maskCoverage = float(quint32(qBound(0., layer.opacity, 1.) * 65535.));
    return settings;
}

void Document::invalidateGpuTonePreview()
{
    ++m_gpuToneEpoch;
    m_gpuToneAuxTimer.stop();
    m_gpuToneBase          = {};
    m_gpuToneTable         = {};
    m_gpuToneMask          = {};
    m_gpuToneMaskSourceKey = 0;
    m_gpuToneMaskFeather   = 0;
    m_gpuToneRejected      = false;
    const bool active      = m_gpuToneActive;
    m_gpuToneActive        = false;
    if (active)
        emit gpuTonePreviewChanged();
}

bool Document::updateGpuTonePreview()
{
    if (!m_previewingLayer || m_gpuToneRejected || !gpuToneEligible() || m_gpuToneBase.isNull())
        return false;
    const auto &layer = m_layers[m_active];
    if (layer.maskEnabled && !layer.mask.isNull() && !ImageUtils::compact(layer.mask)) {
        const int feather =
            qRound(qBound(0., layer.parameters.value("_maskFeather", 0.).toDouble(), 100.));
        if (m_gpuToneMask.isNull() || m_gpuToneMask.size() != m_gpuToneBase.size() ||
            m_gpuToneMaskSourceKey != layer.mask.cacheKey() || m_gpuToneMaskFeather != feather)
            return false;
    } else
        m_gpuToneMask = {};
    const auto table = PhotoCrap::Filters::toneMappingTable(layer.adjustmentType, layer.parameters);
    // Mask density and feathering do not change the color lookup.
    if (m_gpuToneTable != table)
        m_gpuToneTable = table;
    m_gpuToneActive = !m_gpuToneTable.isNull();
    if (!m_gpuToneActive)
        return false;
    ++m_previewRequestGeneration;
    m_previewRenderPending = false;
    m_previewRenderActive.store(true);
    emit gpuTonePreviewChanged();
    if (!m_gpuToneAuxTimer.isActive())
        m_gpuToneAuxTimer.start();
    return true;
}

void Document::finishGpuToneBase(const PreviewRenderResult &_result, quint64 _epoch)
{
    if (_epoch != m_gpuToneEpoch || !m_previewingLayer || !gpuToneEligible())
        return;
    if (_result.image.isNull()) {
        m_gpuToneRejected = true;
        QTimer::singleShot(0, this, [this, _epoch] {
            if (_epoch == m_gpuToneEpoch && m_previewingLayer)
                requestPreviewRender(true);
        });
        return;
    }
    m_gpuToneBase     = _result.image;
    const auto &layer = m_layers[m_active];
    const bool  painted =
        layer.maskEnabled && !layer.mask.isNull() && !ImageUtils::compact(layer.mask);
    const int feather =
        qRound(qBound(0., layer.parameters.value("_maskFeather", 0.).toDouble(), 100.));
    if (painted &&
        (_result.maskSourceKey != layer.mask.cacheKey() || _result.maskFeather != feather)) {
        // The user may have returned to the already displayed feather value,
        // cancelling the queued job while an older worker was still running.
        // Never replace that valid mask with the obsolete worker's output.
        if (!updateGpuTonePreview() && !m_previewRenderPending)
            QTimer::singleShot(0, this, [this, _epoch] {
                if (_epoch == m_gpuToneEpoch && m_previewingLayer)
                    requestPreviewRender(true);
            });
        return;
    }
    m_gpuToneMask          = _result.mask;
    m_gpuToneMaskSourceKey = _result.maskSourceKey;
    m_gpuToneMaskFeather   = _result.maskFeather;
    // Color/density requests arriving during this job can reuse its base and mask.
    // A newer feather request must keep its queued job instead of being discarded.
    if (updateGpuTonePreview() && m_pendingPreviewGpuBase)
        m_previewRenderPending = false;
}
