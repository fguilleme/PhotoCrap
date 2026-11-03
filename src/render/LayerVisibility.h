#pragma once
#include "ImageUtils.h"
#include "Layer.h"
#include <QHash>
#include <QVector>

namespace PhotoCrap::Render::LayerVisibility
{
inline bool uniformMaskValue(const PixelPlane &_mask, int _value)
{
    return _mask.isUniform() && _mask.format() == QImage::Format_Grayscale8 &&
           _mask.tileImage(0, 0).constScanLine(0)[0] == _value;
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
            return _layer.image.pixelColor(0, 0).alpha() != 0;
        if (_layer.image.isUniform())
            return _layer.image.tileImage(0, 0).pixelColor(0, 0).alphaF() != 0;
    }
    return true;
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
            !layer.image.isUniform() ||
            (layer.image.size() != _size && !ImageUtils::compact(layer.image)))
            continue;
        const auto pixel = layer.image.tileImage(0, 0);
        // A device-scaled image may cover only part of its native dimensions.
        // Fractional opacity/alpha must keep every underlying layer, too.
        if (pixel.devicePixelRatio() == 1. && pixel.pixelColor(0, 0).alphaF() == 1.)
            return i;
    }
    return 0;
}

} // namespace PhotoCrap::Render::LayerVisibility
