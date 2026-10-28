#pragma once
#include "SelectionGeometry.h"
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QVariantMap>

namespace SelectionCoverage
{
struct Result {
    QImage  mask;
    QRegion support, outline;
    bool    valid = false;
    QString error;
};
struct Sample {
    QColor  color;
    QPointF position;
    bool    subtract = false;
};
Result describe(QImage _mask, const std::shared_ptr<std::atomic_bool> &_cancel);
QImage raster(const QRegion &_region, const QSize &_size);
Result feather(const QRegion &_region, const QImage &_mask, const QSize &_size, int _radius,
               const std::shared_ptr<std::atomic_bool> &_cancel);
Result modify(const QImage &_mask, const QString &_operation, int _radius,
              const std::shared_ptr<std::atomic_bool> &_cancel);
Result colors(const QImage &_source, const QVector<Sample> &_samples, const QVariantMap &_options,
              const std::shared_ptr<std::atomic_bool> &_cancel);
void   attenuate(QImage &_image, const QImage &_mask, const QPoint &_origin = {});
void   blend(QImage &_image, const QImage &_before, const QImage &_mask, const QRect &_area);
} // namespace SelectionCoverage
