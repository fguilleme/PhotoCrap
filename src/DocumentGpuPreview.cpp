#include "Document.h"
#include "ImageUtils.h"
#include "filters/ToneMappingTable.h"

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
