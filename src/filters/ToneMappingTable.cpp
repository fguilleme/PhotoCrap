#include "ToneMappingTable.h"
#include "CurvesFilter.h"
#include "LevelsFilter.h"
#include "LightFilter.h"

namespace PhotoCrap::Filters
{
QImage toneMappingTable(const QString &_type, const QVariantMap &_parameters)
{
    QImage ramp(256, 1, QImage::Format_ARGB32);
    for (int value = 0; value < 256; ++value)
        ramp.setPixel(value, 0, qRgb(value, value, value));
    if (_type == QStringLiteral("curves"))
        return applyCurves(ramp, _parameters);
    if (_type == QStringLiteral("levels"))
        return applyLevels(ramp, _parameters);
    if (_type == QStringLiteral("light"))
        return applyLight(ramp, _parameters);
    return {};
}
} // namespace PhotoCrap::Filters
