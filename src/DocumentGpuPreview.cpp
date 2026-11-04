#include "Document.h"
#include "ImageUtils.h"
#include "filters/ToneMappingTable.h"
#include "render/LayerVisibility.h"
#include <QDataStream>

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
    for (int index = LayerVisibility::opaqueCoverIndex(m_layers, m_size); index < m_layers.size();
         ++index) {
        const auto &layer = m_layers[index];
        if (layer.kind == "group" || !LayerVisibility::contributes(layer) ||
            (!layer.groupId.isEmpty() && !groups.value(layer.groupId, true))) {
            if (!layer.clippingMask && !stack.layers.isEmpty() && !stack.layers.constLast().reset)
                stack.layers.append(StackLayer{.reset = true});
            continue;
        }
        // Unsupported operations use the authoritative CPU compositor. No
        // partially composed result may be presented as the complete stack.
        const bool solid = layer.kind == "adjustment" && layer.adjustmentType == "solidColor";
        const bool tonal =
            layer.kind == "adjustment" &&
            PhotoCrap::Filters::gpuTonePreviewTypes().contains(layer.adjustmentType) &&
            (layer.adjustmentType != "selectiveColor" || layer.parameters.contains("redsCyanRed"));
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
            (!solid && !tonal && spatial == SpatialFilter::None && layer.image.isNull()) ||
            layer.blend == 1 || layer.blend == 18 || layer.blend >= 23)
            return {};
        StackLayer entry;
        if (tonal) {
            entry.tone =
                PhotoCrap::Filters::toneMappingSettings(layer.adjustmentType, layer.parameters);
            const int  mode   = int(entry.tone.mode);
            const bool lookup = mode == 0 || mode == 1 || mode == 3 || mode == 4 || mode == 6;
            if (!lookup) {
                static const QImage unused = [] {
                    QImage pixel(1, 1, QImage::Format_ARGB32_Premultiplied);
                    pixel.fill(Qt::white);
                    return pixel;
                }();
                entry.toneTable = unused;
            } else {
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
        stack.layers.append(std::move(entry));
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
