#pragma once
#include <QImage>
#include <QTextDocument>
#include <QTransform>
#include <QVariantMap>

namespace PhotoCrap::Text
{
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
void        transform(QVariantMap &_parameters, const QTransform &_transform);
QImage      render(const QVariantMap &_parameters, const QSize &_size, QImage::Format _format);
QVariantMap formatting(const QTextDocument &_document);
void        applyFormatting(QTextDocument &_document, const QVariantMap &_parameters);
bool        validFormatting(const QVariantMap &_parameters, int _length);
QString     plainText(const QVariantMap &_parameters);
QRect       bounds(const QVariantMap &_parameters, const QSize &_canvas);
} // namespace PhotoCrap::Text
