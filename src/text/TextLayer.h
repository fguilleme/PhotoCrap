#pragma once
#include "storage/PixelPlane.h"
#include <QImage>
#include <QPolygonF>
#include <QTextDocument>
#include <QTransform>
#include <QVariantMap>
#include <optional>

struct Layer;

namespace PhotoCrap::Text
{
struct FaceProjection {
    QTransform primary, across;
    // Positive distances are on the secondary face.
    QLineF edge;

    qreal      distance(const QPointF &point) const;
    qreal      localDistance(const QPointF &point) const;
    QTransform face(bool secondary) const;
    QPointF    map(const QPointF &point) const;
    QPointF    unmap(const QPointF &point) const;
    QPolygonF  outline(const QRectF &rect, const QSize &canvas) const;
};

class Document : public QTextDocument
{
public:
    explicit Document(QObject *_parent = nullptr) : QTextDocument(_parent)
    {
        setUndoRedoEnabled(false);
    }

protected:
    QVariant loadResource(int, const QUrl &) override
    {
        return {};
    }
};
QVariantMap create(qreal _x, qreal _y, int _size, const QColor &_color);
bool        normalize(QVariantMap &_parameters);
void        configure(QTextDocument &_document, const QVariantMap &_parameters);
QSizeF      size(const QVariantMap &_parameters);
QTransform  transform(const QVariantMap &_parameters);
QTransform  displayTransform(const QVariantMap &parameters, const Layer *surface,
                             const QSizeF &extent = {});
std::optional<FaceProjection> faceProjection(const QVariantMap &parameters, const Layer &surface);
QPolygonF   surfaceOutline(const Layer &layer, const QSize &canvas, QPointF &bottomRight);
void        transform(QVariantMap &_parameters, const QTransform &_transform);
PixelPlane  renderPlane(const QVariantMap &_parameters, const QSize &_size, QImage::Format _format,
                        const Layer *surface = nullptr);
QImage      render(const QVariantMap &_parameters, const QSize &_size, QImage::Format _format);
QVariantMap formatting(const QTextDocument &_document);
void        applyFormatting(QTextDocument &_document, const QVariantMap &_parameters);
bool        validFormatting(const QVariantMap &_parameters, int _length);
QString     plainText(const QVariantMap &_parameters);
QRect bounds(const QVariantMap &_parameters, const QSize &_canvas, const Layer *surface = nullptr);
} // namespace PhotoCrap::Text
