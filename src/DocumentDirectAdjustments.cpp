#include "Document.h"
#include "ImageUtils.h"
#include <QJSValue>
#include <QMutexLocker>
#include <QtMath>

QVariantMap Document::directAdjustmentInfo() const
{
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].directType.isEmpty())
        return {};
    const auto &layer = m_layers[m_active];
    return {{"name", adjustmentName(layer.directType)},
            {"kind", "adjustment"},
            {"adjustmentType", layer.directType},
            {"parameters", layer.directParameters},
            {"editingMask", false},
            {"direct", true},
            {"maskTarget", layer.directMask}};
}
bool Document::beginDirectAdjustment(const QString &_type)
{
    if (operationBusy() || pixelEditBusy() || !directAdjustmentInfo().isEmpty() ||
        adjustmentName(_type).isEmpty() || _type == "solidColor" || !editable(false))
        return false;
    if (editingVectorMask())
        return fail(tr("Utilisez le contour progressif pour adoucir le masque vectoriel, ou "
                       "pixellisez-le avant d’appliquer un filtre."));
    beginLayerPreview();
    auto &layer                               = m_layers[m_active];
    layer.directType                          = _type;
    layer.directParameters                    = adjustmentDefaults(_type);
    layer.directParameters["_maskOnlyView"]   = m_maskOnlyView;
    layer.directParameters["_canvasSize"]     = m_size;
    layer.directParameters["_limitSelection"] = true;
    layer.directMask                          = m_editingMask;
    layer.directPreview                       = true;
    layer.directSelection                     = hasSelection() ? selectionMask() : QImage{};
    m_layerPreviewChanged                     = true;
    m_cacheDirty = m_fullCompositeDirty = true;
    if (m_maskOnlyView) {
        QMutexLocker locker(&m_previewImageMutex);
        m_previewComposite = {};
    }
    requestPreviewRender(true);
    emit layersChanged();
    return true;
}
void Document::setDirectAdjustmentPreview(bool _enabled)
{
    if (directAdjustmentInfo().isEmpty())
        return;
    m_layers[m_active].directPreview = _enabled;
    m_cacheDirty = m_fullCompositeDirty = true;
    requestPreviewRender(true);
}
void Document::finishDirectAdjustment(bool _apply)
{
    if (directAdjustmentInfo().isEmpty())
        return;
    const auto type       = m_layers[m_active].directType;
    const auto parameters = m_layers[m_active].directParameters;
    cancelLayerPreview();
    emit layersChanged();
    if (_apply)
        applyAdjustment(type, parameters);
}
QImage Document::directAdjustedImage(const QImage &_source, const Layer &_layer,
                                     const QRect &_region)
{
    if (_layer.directType.isEmpty() || !_layer.directPreview)
        return _source;
    QImage result = adjustedImage(
        _source.convertToFormat(ImageUtils::is16Bit(_source) ? QImage::Format_RGBA64_Premultiplied
                                                             : QImage::Format_ARGB32_Premultiplied),
        _layer.directType, _layer.directParameters, _region.topLeft(),
        _layer.directParameters.value("_canvasSize", _source.size()).toSize());
    result = result.convertToFormat(_source.format());
    if (!_layer.directSelection.isNull()) {
        const auto mask = _layer.directSelection.region(_region);
        SelectionCoverage::blend(result, _source, mask, result.rect());
    }
    return result;
}
void Document::applyAdjustment(const QString &_type, const QVariantMap &_parameters)
{
    if (adjustmentName(_type).isEmpty() || _type == "solidColor" || !editable(false))
        return;
    if (editingVectorMask()) {
        fail(tr("Pixellisez le masque vectoriel pour appliquer un filtre direct."));
        return;
    }
    // QML arrays must leave the JavaScript engine before a worker sees them.
    QVariantMap parameters;
    for (auto it = _parameters.cbegin(); it != _parameters.cend(); ++it)
        parameters[it.key()] =
            it.value().metaType() == QMetaType::fromType<QJSValue>()
                ? it.value().value<QJSValue>().toVariant(QJSValue::ConvertJSObjects)
                : it.value();
    parameters["_canvasSize"] = m_size;
    if (deferEdit(tr("Appliquer %1").arg(adjustmentName(_type)), editCost(false),
                  [_type, parameters](Document &_document) {
                      _document.applyAdjustment(_type, parameters);
                  }))
        return;
    edit(tr("Appliquer %1").arg(adjustmentName(_type)), [&] {
        if (!operationStep(0, 3))
            return;
        auto &      layer   = m_layers[m_active];
        auto &      target  = m_editingMask ? layer.mask : layer.image;
        const bool  limited = hasSelection() && parameters.value("_limitSelection", true).toBool();
        const QRect output =
            limited ? m_selection.intersected(QRect(QPoint{}, m_size)) : QRect(QPoint{}, m_size);
        const int   margin = qCeil(parameters.value("radius", 0.).toDouble() * 3 +
                                 parameters.value("distance", 0.).toDouble() +
                                 parameters.value("cellSize", 0.).toDouble());
        const QRect work =
            output.adjusted(-margin, -margin, margin, margin).intersected(QRect(QPoint{}, m_size));
        const QImage original = ImageUtils::compact(target)
                                    ? ImageUtils::expandedPlane(target, work.size())
                                    : target.region(work);
        Layer        effect;
        effect.directType       = _type;
        effect.directParameters = parameters;
        effect.directSelection  = limited ? selectionMask() : QImage{};
        QImage result           = directAdjustedImage(original, effect, work);
        if (!operationStep(2, 3) || result.isNull())
            return;
        if (work != output)
            result = result.copy(QRect(output.topLeft() - work.topLeft(), output.size()));
        if (!target.writeRegion(output.topLeft(), result, m_size)) {
            fail(tr("Mémoire insuffisante pour appliquer le réglage."));
            return;
        }
        // Retained AI pixels describe the old image after a destructive edit.
        if (!m_editingMask)
            layer.aiSource.reset();
        operationStep(3, 3);
    });
}
