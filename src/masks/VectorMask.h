#pragma once
#include "Layer.h"
#include "tools/PenPath.h"

namespace PhotoCrap::Masks
{
bool    hasVector(const Layer &_layer);
bool    vectorEnabled(const Layer &_layer);
PenPath vectorPath(const Layer &_layer);
void    setVectorPath(Layer &_layer, const PenPath &_path, const QSize &_size);
bool    validVector(const Layer &_layer);
bool    transformVector(Layer &_layer, const QTransform &_transform, const QSize &_size);
QImage  vectorCoverage(const Layer &_layer, const QSize &_size, const QRect &_region);
} // namespace PhotoCrap::Masks
