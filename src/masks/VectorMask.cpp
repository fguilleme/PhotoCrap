#include "VectorMask.h"
#include "ImageUtils.h"
#include "shapes/ShapeLayer.h"
#include <QJsonArray>
#include <QPainter>
#include <cmath>

namespace PhotoCrap::Masks
{
bool hasVector(const Layer &_layer)
{
    return _layer.parameters.contains("_vectorMask");
}
bool vectorEnabled(const Layer &_layer)
{
    return hasVector(_layer) && _layer.parameters.value("_vectorEnabled", true).toBool() &&
           _layer.parameters.value("_vectorDensity", 100.).toDouble() > 0;
}
PenPath vectorPath(const Layer &_layer)
{
    return Shape::path(_layer.parameters.value("_vectorMask").toMap());
}
void setVectorPath(Layer &_layer, const PenPath &_path, const QSize &_size)
{
    auto data = _layer.parameters.value("_vectorMask").toMap();
    Shape::setPath(data, _path);
    data["width"]                    = _size.width();
    data["height"]                   = _size.height();
    _layer.parameters["_vectorMask"] = data;
}
bool validVector(const Layer &_layer)
{
    if (!hasVector(_layer))
        return true;
    const auto       data = _layer.parameters.value("_vectorMask").toMap();
    QVector<PenPath> paths;
    const double     density = _layer.parameters.value("_vectorDensity", 100.).toDouble();
    const double     feather = _layer.parameters.value("_vectorFeather", 0.).toDouble();
    return data.value("width").toInt() > 0 && data.value("height").toInt() > 0 &&
           PenGeometry::decode(QJsonArray::fromVariantList(data.value("path").toList()), paths,
                               true) &&
           paths.size() == 1 && std::isfinite(density) && density >= 0 && density <= 100 &&
           std::isfinite(feather) && feather >= 0 && feather <= 100;
}
bool transformVector(Layer &_layer, const QTransform &_transform, const QSize &_size)
{
    if (!hasVector(_layer))
        return true;
    QVector<PenPath> paths{vectorPath(_layer)};
    if (!PenGeometry::transform(paths, _transform))
        return false;
    setVectorPath(_layer, paths.first(), _size);
    return true;
}
QImage vectorCoverage(const Layer &_layer, const QSize &_size, const QRect &_region)
{
    if (!hasVector(_layer) || _region.isEmpty())
        return {};
    const auto   data = _layer.parameters.value("_vectorMask").toMap();
    const double sx   = double(_size.width()) / qMax(1, data.value("width").toInt());
    const double sy   = double(_size.height()) / qMax(1, data.value("height").toInt());
    const auto   path = QTransform::fromScale(sx, sy).map(vectorPath(_layer).painterPath());
    QImage       image(_region.size(), QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return {};
    image.fill(Qt::black);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(-_region.topLeft());
    painter.fillPath(path, Qt::white);
    painter.end();
    return image.convertToFormat(QImage::Format_Grayscale8);
}
} // namespace PhotoCrap::Masks
