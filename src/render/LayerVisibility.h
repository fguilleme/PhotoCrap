#pragma once
#include "ImageUtils.h"
#include "Layer.h"
#include <QHash>
#include <QVector>

namespace PhotoCrap::Render::LayerVisibility
{
inline bool uniformMaskValue(const PixelPlane &_mask, int _value)
{
    if (_mask.format() != QImage::Format_Grayscale8)
        return false;
    const QImage pixel = _mask.uniformPixel();
    return !pixel.isNull() && pixel.constBits()[0] == _value;
}

inline bool needsMask(const Layer &_layer)
{
    return _layer.maskEnabled && !_layer.mask.isNull() &&
           _layer.parameters.value("_maskDensity", 100.).toDouble() > 0 &&
           !uniformMaskValue(_layer.mask, 255);
}

inline bool contributes(const Layer &_layer)
{
    if (!_layer.visible || _layer.opacity <= 0)
        return false;
    if (_layer.maskEnabled && uniformMaskValue(_layer.mask, 0) &&
        _layer.parameters.value("_maskDensity", 100.).toDouble() >= 100.)
        return false;
    // A tiled uniform background can have document dimensions without a single
    // explicit tile. Do not expand or filter its transparent pixels. Keep the
    // previous compact-plane rule, including its eight-bit alpha rounding.
    if (_layer.kind == QStringLiteral("pixel")) {
        if (ImageUtils::compact(_layer.image))
            return _layer.image.uniformPixel().pixelColor(0, 0).alpha() != 0;
        const QImage pixel = _layer.image.uniformPixel();
        if (!pixel.isNull())
            return pixel.pixelColor(0, 0).alphaF() != 0;
    }
    return true;
}

inline bool reconstructsFrequencySource(const QVector<Layer> &_layers, qsizetype _index)
{
    const auto &low = _layers[_index];
    if (low.kind != QStringLiteral("frequencyLow") || _index + 1 >= _layers.size())
        return false;
    const auto &high   = _layers[_index + 1];
    const auto  pairId = low.parameters.value("pairId").toString();
    return !low.clippingMask && !high.clippingMask &&
           (_index + 2 == _layers.size() || !_layers[_index + 2].clippingMask) && high.visible &&
           high.kind == QStringLiteral("frequencyHigh") && !pairId.isEmpty() &&
           high.parameters.value("pairId").toString() == pairId &&
           low.source.cacheKey() == high.source.cacheKey() && qFuzzyCompare(low.opacity, 1.) &&
           qFuzzyCompare(high.opacity, 1.) && low.blend == 0 && high.blend == 16 &&
           (!low.maskEnabled || low.mask.isNull()) && (!high.maskEnabled || high.mask.isNull());
}

inline int opaqueCoverIndex(const QVector<Layer> &_layers, const QSize &_size)
{
    QHash<QString, bool> groups;
    for (const auto &layer : _layers)
        if (layer.kind == QStringLiteral("group"))
            groups.insert(layer.groupId, layer.visible);
    for (int i = int(_layers.size()) - 1; i > 0; --i) {
        const auto &layer = _layers[i];
        if (layer.kind != QStringLiteral("pixel") || !layer.visible || layer.opacity < 1. ||
            layer.blend != 0 || layer.clippingMask || needsMask(layer) ||
            (!layer.groupId.isEmpty() && !groups.value(layer.groupId, true)) ||
            !layer.image.knownOpaque() ||
            (layer.image.size() != _size && !ImageUtils::compact(layer.image)))
            continue;
        // Opacity and DPR are cached with the immutable native versions; no
        // tile reconstruction or mapped-file access belongs in this query.
        return i;
    }
    return 0;
}

} // namespace PhotoCrap::Render::LayerVisibility
