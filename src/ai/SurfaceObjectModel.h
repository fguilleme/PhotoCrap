#pragma once
#include "AiImageTypes.h"
#include <vector>

namespace PhotoCrap::Ai
{
struct SurfaceObjectEmbedding {
    QSize              imageSize;
    std::vector<float> values;
};

class SurfaceObjectModel
{
public:
    static bool                                          available();
    static std::shared_ptr<const SurfaceObjectEmbedding> encode(const QImage &source,
                                                                const Cancel &cancel);
    static QImage mask(const SurfaceObjectEmbedding &embedding, const QPointF &point,
                       const QSize &size, const Cancel &cancel);
};
} // namespace PhotoCrap::Ai
