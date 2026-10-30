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

bool Document::gpuToneEligible() const
{
    if (!m_gpuToneEnabled || m_displayChannel != 0 || m_maskOnlyView || m_active < 0 ||
        m_active != m_layers.size() - 1)
        return false;
    const auto &layer = m_layers[m_active];
    return layer.kind == QStringLiteral("adjustment") && layer.visible && layer.blend == 0 &&
           qFuzzyCompare(layer.opacity, 1.) && layer.groupId.isEmpty() && !layer.clippingMask &&
           (layer.adjustmentType == QStringLiteral("curves") ||
            layer.adjustmentType == QStringLiteral("levels") ||
            layer.adjustmentType == QStringLiteral("light") ||
            layer.adjustmentType == QStringLiteral("color") ||
            layer.adjustmentType == QStringLiteral("colorBalance") ||
            layer.adjustmentType == QStringLiteral("blackWhite") ||
            layer.adjustmentType == QStringLiteral("gradientMap") ||
            layer.adjustmentType == QStringLiteral("photoFilter") ||
            layer.adjustmentType == QStringLiteral("channelMixer") ||
            layer.adjustmentType == QStringLiteral("posterize") ||
            layer.adjustmentType == QStringLiteral("threshold")) &&
           (!layer.maskEnabled || layer.mask.isNull() ||
            (ImageUtils::compact(layer.mask) && layer.mask.constScanLine(0)[0] == 255));
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
    return PhotoCrap::Filters::toneMappingSettings(layer.adjustmentType, layer.parameters);
}

void Document::invalidateGpuTonePreview()
{
    ++m_gpuToneEpoch;
    m_gpuToneAuxTimer.stop();
    m_gpuToneBase     = {};
    m_gpuToneTable    = {};
    m_gpuToneRejected = false;
    const bool active = m_gpuToneActive;
    m_gpuToneActive   = false;
    if (active)
        emit gpuTonePreviewChanged();
}

bool Document::updateGpuTonePreview()
{
    if (!m_previewingLayer || m_gpuToneRejected || !gpuToneEligible() || m_gpuToneBase.isNull())
        return false;
    const auto &layer = m_layers[m_active];
    m_gpuToneTable  = PhotoCrap::Filters::toneMappingTable(layer.adjustmentType, layer.parameters);
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

void Document::finishGpuToneBase(const QImage &_image, quint64 _epoch)
{
    if (_epoch != m_gpuToneEpoch || !m_previewingLayer || !gpuToneEligible())
        return;
    if (m_pendingPreviewGpuBase)
        m_previewRenderPending = false;
    if (_image.isNull()) {
        m_gpuToneRejected = true;
        QTimer::singleShot(0, this, [this, _epoch] {
            if (_epoch == m_gpuToneEpoch && m_previewingLayer)
                requestPreviewRender(true);
        });
        return;
    }
    m_gpuToneBase = _image;
    updateGpuTonePreview();
}
