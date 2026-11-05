#pragma once
#include <QByteArray>
#include <QtMath>
namespace PhotoCrap::Render
{
enum class SpatialFilter { None, Box, Gaussian, Motion, Unsharp, HighPass };
struct SpatialFilterSettings {
    SpatialFilter type   = SpatialFilter::None;
    int           radius = 8, threshold = 0;
    float         distance = 20, angle = 0, amount = 100;
    bool          roundBox2D = false;
    int           passCount() const
    {
        switch (type) {
        case SpatialFilter::None:
            return 0;
        case SpatialFilter::Motion:
            return 1;
        case SpatialFilter::Box:
            return 2;
        case SpatialFilter::Gaussian:
            return 6;
        case SpatialFilter::Unsharp:
        case SpatialFilter::HighPass:
            return 7;
        }
        return 0;
    }
    int           halo() const
    {
        if (type == SpatialFilter::None)
            return 0;
        if (type == SpatialFilter::Motion)
            return qCeil(distance / 2.) + 2;
        return radius * (type == SpatialFilter::Box ? 1 : 3);
    }
    QByteArray key() const
    {
        QByteArray result;
        for (int value : {int(type), radius, threshold})
            result.append(reinterpret_cast<const char *>(&value), sizeof(value));
        for (float value : {distance, angle, amount})
            result.append(reinterpret_cast<const char *>(&value), sizeof(value));
        result.append(char(roundBox2D));
        return result;
    }
};
} // namespace PhotoCrap::Render
