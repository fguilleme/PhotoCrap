#include "ShapeLayer.h"
#include "ImageUtils.h"
#include <QJsonArray>
#include <QPainter>
#include <QPainterPathStroker>
#include <cmath>

namespace
{
QPen stroke(const QVariantMap &_parameters)
{
    if (!_parameters.value("strokeEnabled").toBool())
        return QPen(Qt::NoPen);
    const int style = _parameters.value("strokeStyle").toInt();
    return QPen(QColor(_parameters.value("strokeColor").toString()),
                _parameters.value("strokeWidth").toDouble(),
                style == 1   ? Qt::DashLine
                : style == 2 ? Qt::DotLine
                             : Qt::SolidLine,
                Qt::SquareCap, Qt::MiterJoin);
}
} // namespace
QVariantMap PhotoCrap::Shape::defaults()
{
    return {{"fillEnabled", true},        {"fillColor", "#ff000000"}, {"strokeEnabled", false},
            {"strokeColor", "#ff000000"}, {"strokeWidth", 1.},        {"strokeStyle", 0}};
}
bool PhotoCrap::Shape::normalize(QVariantMap &_parameters)
{
    QVector<PenPath> paths;
    if (!PenGeometry::decode(QJsonArray::fromVariantList(_parameters.value("path").toList()),
                             paths) ||
        paths.size() != 1)
        return false;
    const auto values = defaults();
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        if (!_parameters.contains(it.key()))
            _parameters[it.key()] = it.value();
    for (const auto &key : {"fillColor", "strokeColor"}) {
        const QColor color(_parameters.value(key).toString());
        if (!color.isValid())
            return false;
        _parameters[key] = color.name(QColor::HexArgb);
    }
    const auto width = _parameters.value("strokeWidth").toDouble();
    const auto style = _parameters.value("strokeStyle").toInt();
    return std::isfinite(width) && width >= .5 && width <= 500 && style >= 0 && style <= 2 &&
           _parameters.value("fillEnabled").metaType().id() == QMetaType::Bool &&
           _parameters.value("strokeEnabled").metaType().id() == QMetaType::Bool;
}
PenPath PhotoCrap::Shape::path(const QVariantMap &_parameters)
{
    QVector<PenPath> paths;
    PenGeometry::decode(QJsonArray::fromVariantList(_parameters.value("path").toList()), paths);
    return paths.size() == 1 ? paths.first() : PenPath{};
}
void PhotoCrap::Shape::setPath(QVariantMap &_parameters, const PenPath &_path)
{
    auto path = _path;
    path.shapeLayerId.clear();
    path.name           = QStringLiteral("Shape");
    _parameters["path"] = PenGeometry::encode({path}).toVariantList();
}
bool PhotoCrap::Shape::transform(QVariantMap &_parameters, const QTransform &_transform)
{
    QVector<PenPath> paths{path(_parameters)};
    if (!PenGeometry::transform(paths, _transform))
        return false;
    setPath(_parameters, paths.first());
    return true;
}
QRect PhotoCrap::Shape::bounds(const QVariantMap &_parameters, const QSize &_canvas)
{
    const auto curve = path(_parameters).painterPath();
    if (curve.isEmpty())
        return {};
    QRectF bounds;
    if (_parameters.value("fillEnabled").toBool())
        bounds = curve.boundingRect();
    const auto pen = stroke(_parameters);
    if (pen.style() != Qt::NoPen) {
        QPainterPathStroker stroker(pen);
        bounds = bounds.united(stroker.createStroke(curve).boundingRect());
    }
    if (bounds.isEmpty())
        return {};
    return bounds.toAlignedRect().adjusted(-1, -1, 1, 1).intersected(QRect(QPoint{}, _canvas));
}
PixelPlane PhotoCrap::Shape::renderPlane(const QVariantMap &_parameters, const QSize &_canvas,
                                         QImage::Format _format)
{
    auto       plane  = PixelPlane::uniform(_canvas, ImageUtils::blank({1, 1}, _format));
    const auto region = bounds(_parameters, _canvas);
    if (region.isEmpty())
        return plane;
    auto image = ImageUtils::blank(region.size(), _format);
    if (image.isNull())
        return {};
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(-region.topLeft());
    painter.setPen(stroke(_parameters));
    painter.setBrush(_parameters.value("fillEnabled").toBool()
                         ? QBrush(QColor(_parameters.value("fillColor").toString()))
                         : QBrush(Qt::NoBrush));
    painter.drawPath(path(_parameters).painterPath());
    painter.end();
    if (!plane.writeRegion(region.topLeft(), image, _canvas))
        return {};
    return plane;
}
