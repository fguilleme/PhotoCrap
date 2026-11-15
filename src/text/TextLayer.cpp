#include "TextLayer.h"
#include "ImageUtils.h"
#include "Layer.h"
#include "ai/SurfaceGeometry.h"
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextFrame>
#include <cmath>

namespace PhotoCrap::Text
{
namespace
{
void storeTransform(QVariantMap &_p, const QTransform &_t)
{
    _p["transform"] = QVariantList{_t.m11(), _t.m12(), _t.m13(), _t.m21(), _t.m22(),
                                   _t.m23(), _t.dx(),  _t.dy(),  _t.m33()};
}
} // namespace
namespace
{
const QSet<int> characterProperties{
    QTextFormat::FontPixelSize,         QTextFormat::FontLetterSpacing,
    QTextFormat::FontLetterSpacingType, QTextFormat::FontStretch,
    QTextFormat::FontKerning,           QTextFormat::FontCapitalization,
    QTextFormat::TextVerticalAlignment, QTextFormat::TextBaselineOffset};
const QSet<int> blockProperties{
    QTextFormat::BlockAlignment,  QTextFormat::BlockTopMargin,   QTextFormat::BlockBottomMargin,
    QTextFormat::BlockLeftMargin, QTextFormat::BlockRightMargin, QTextFormat::TextIndent,
    QTextFormat::LineHeight,      QTextFormat::LineHeightType,   QTextFormat::LayoutDirection};
QVariant propertyValue(int _key, const QVariant &_value)
{
    static const QSet<int> integers{
        QTextFormat::FontPixelSize,         QTextFormat::FontStretch,
        QTextFormat::FontLetterSpacingType, QTextFormat::FontCapitalization,
        QTextFormat::TextVerticalAlignment, QTextFormat::BlockAlignment,
        QTextFormat::LineHeightType,        QTextFormat::LayoutDirection};
    if (_key == QTextFormat::FontKerning)
        return _value.toBool();
    if (integers.contains(_key))
        return _value.toInt();
    return _value.toDouble();
}
QVariantMap properties(const QTextFormat &_format, const QSet<int> &_allowed)
{
    QVariantMap p;
    for (auto it = _allowed.begin(); it != _allowed.end(); ++it)
        if (_format.hasProperty(*it) &&
            !(*it == QTextFormat::FontPixelSize && _format.property(*it).toDouble() <= 0))
            p[QString::number(*it)] = _format.property(*it);
    return p;
}
} // namespace
QVariantMap formatting(const QTextDocument &_doc)
{
    QVariantList runs, blocks;
    for (auto block = _doc.begin(); block.isValid(); block = block.next()) {
        const auto p = properties(block.blockFormat(), blockProperties);
        if (!p.isEmpty())
            blocks.append(QVariantMap{
                {"start", block.position()}, {"length", block.length() - 1}, {"properties", p}});
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto f  = it.fragment();
            const auto cp = properties(f.charFormat(), characterProperties);
            if (!cp.isEmpty())
                runs.append(QVariantMap{
                    {"start", f.position()}, {"length", f.length()}, {"properties", cp}});
        }
    }
    return {{"runs", runs}, {"blocks", blocks}};
}
bool validFormatting(const QVariantMap &_p, int _length)
{
    for (const auto key : {"runs", "blocks"}) {
        const auto data = _p.value(key).toList();
        if (data.size() > 20000)
            return false;
        const auto &allowed = QString(key) == "runs" ? characterProperties : blockProperties;
        for (const auto &v : data) {
            const auto run   = v.toMap();
            const int  start = run.value("start").toInt(), length = run.value("length").toInt();
            if (start < 0 || length < 0 || start > _length || length > _length - start)
                return false;
            const auto props = run.value("properties").toMap();
            for (auto it = props.begin(); it != props.end(); ++it) {
                const double n = it.value().toDouble();
                if (!allowed.contains(it.key().toInt()) || !std::isfinite(n) || qAbs(n) > 32768)
                    return false;
                if (it.key().toInt() == QTextFormat::FontPixelSize && (n < 1 || n > 1024))
                    return false;
            }
        }
    }
    return true;
}
void applyFormatting(QTextDocument &_doc, const QVariantMap &_p)
{
    for (const auto key : {"runs", "blocks"})
        for (const auto &v : _p.value(key).toList()) {
            const auto  run = v.toMap();
            QTextCursor cursor(&_doc);
            cursor.setPosition(run.value("start").toInt());
            cursor.setPosition(run.value("start").toInt() + run.value("length").toInt(),
                               QTextCursor::KeepAnchor);
            const auto props = run.value("properties").toMap();
            if (QString(key) == "runs") {
                QTextCharFormat f;
                for (auto it = props.begin(); it != props.end(); ++it)
                    f.setProperty(it.key().toInt(), propertyValue(it.key().toInt(), it.value()));
                cursor.mergeCharFormat(f);
            } else {
                QTextBlockFormat f;
                for (auto it = props.begin(); it != props.end(); ++it)
                    f.setProperty(it.key().toInt(), propertyValue(it.key().toInt(), it.value()));
                cursor.mergeBlockFormat(f);
            }
        }
}
QVariantMap create(qreal _x, qreal _y, int _size, const QColor &_color)
{
    QVariantMap p{{"html", QString{}},  {"fontFamily", QStringLiteral("sans-serif")},
                  {"fontSize", _size},  {"color", _color.name(QColor::HexArgb)},
                  {"paragraph", false}, {"width", 320.},
                  {"height", 180.},     {"autoName", true}};
    storeTransform(p, QTransform::fromTranslate(_x, _y));
    return p;
}
QTransform transform(const QVariantMap &_p)
{
    const auto t = _p.value("transform").toList();
    if (t.size() != 9)
        return {};
    return QTransform(t[0].toDouble(), t[1].toDouble(), t[2].toDouble(), t[3].toDouble(),
                      t[4].toDouble(), t[5].toDouble(), t[6].toDouble(), t[7].toDouble(),
                      t[8].toDouble());
}
QTransform displayTransform(const QVariantMap &parameters, const Layer *surface,
                            const QSizeF &extent)
{
    const auto base = transform(parameters);
    if (!surface || surface->surfaceNormals.isNull() || !parameters.contains("surfaceFocal"))
        return base;
    const auto projection =
        base * Ai::surfaceTextProjection(surface->surfaceNormals, surface->surfaceNormalCanvas,
                                         base.map(QPointF{}), surface->surfaceNormalTransform,
                                         parameters.value("surfaceAngle").toDouble(),
                                         parameters.value("surfaceFocal", .85).toDouble());
    if (projection.isAffine() || qFuzzyIsNull(projection.m33()))
        return projection;

    const auto  box     = extent.isValid() ? extent : size(parameters);
    const qreal padding = parameters.value("fontSize", 64.).toDouble() * 2.;
    const qreal span    = qAbs(projection.m13()) * (qMax(32., box.width()) + padding) +
                          qAbs(projection.m23()) * (qMax(64., box.height()) + padding);
    // Bound the perspective denominator across the editable text. Keep the
    // anchor and its local tangent frame, so long text cannot reach a pole.
    const qreal maximumSpan = .24 * qAbs(projection.m33());
    if (span <= maximumSpan)
        return projection;
    const qreal strength = maximumSpan / span;
    const auto  anchor   = projection.map(QPointF{});
    const qreal dx       = projection.m13() * (1. - strength);
    const qreal dy       = projection.m23() * (1. - strength);
    return {projection.m11() - anchor.x() * dx,
            projection.m12() - anchor.y() * dx,
            projection.m13() * strength,
            projection.m21() - anchor.x() * dy,
            projection.m22() - anchor.y() * dy,
            projection.m23() * strength,
            projection.m31(),
            projection.m32(),
            projection.m33()};
}
void transform(QVariantMap &_p, const QTransform &_t)
{
    storeTransform(_p, transform(_p) * _t);
}
namespace
{
void parse(QTextDocument &_doc, const QVariantMap &_p)
{
    _doc.setDocumentMargin(0);
    QFont font(_p.value("fontFamily", "sans-serif").toString());
    font.setPixelSize(_p.value("fontSize", 64).toInt());
    _doc.setDefaultFont(font);
    _doc.setHtml(_p.value("html").toString());
    if (validFormatting(_p, _doc.characterCount() - 1))
        applyFormatting(_doc, _p);
}
} // namespace
void configure(QTextDocument &_doc, const QVariantMap &_p)
{
    parse(_doc, _p);
    QTextOption option = _doc.defaultTextOption();
    option.setWrapMode(_p.value("paragraph").toBool() ? QTextOption::WrapAtWordBoundaryOrAnywhere
                                                      : QTextOption::NoWrap);
    _doc.setDefaultTextOption(option);
    if (_p.value("paragraph").toBool())
        _doc.setTextWidth(_p.value("width").toDouble());
    else {
        _doc.setTextWidth(-1);
        _doc.setTextWidth(qMax(32., std::ceil(_doc.idealWidth())));
    }
}
bool normalize(QVariantMap &_p)
{
    if (_p.value("html").toString().size() > 1024 * 1024)
        return false;
    if (_p.value("fontFamily").toString().size() > 256)
        return false;
    const auto t = _p.value("transform").toList();
    if (t.size() != 9)
        return false;
    for (const auto &v : t)
        if (!std::isfinite(v.toDouble()) || qAbs(v.toDouble()) > 1e9)
            return false;
    if (!transform(_p).isInvertible())
        return false;
    const qreal angle    = _p.value("surfaceAngle", 0.).toDouble();
    const qreal focal    = _p.value("surfaceFocal", .85).toDouble();
    const qreal strength = _p.value("surfaceStrength", 1.).toDouble();
    if (!std::isfinite(angle) || qAbs(angle) > 180. || !std::isfinite(focal) || focal < .35 ||
        focal > 3. || !std::isfinite(strength) || strength < 0. || strength > 3.)
        return false;
    for (const auto key : {"width", "height", "fontSize"}) {
        const auto n = _p.value(key).toDouble();
        if (!std::isfinite(n) || n < 1 || n > 32768)
            return false;
    }
    if (_p.value("fontSize").toDouble() > 1024)
        return false;
    if (!QColor(_p.value("color").toString()).isValid())
        return false;
    static const QRegularExpression objects(
        QStringLiteral("<\\s*(?:img|table|frame|iframe|object|embed|svg|video|audio)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    if (objects.match(_p.value("html").toString()).hasMatch())
        return false;
    // Imported or clipboard HTML may declare a zero CSS size. Retain the layer's
    // usable default rather than passing that invalid size to the Qt font parser.
    static const QRegularExpression zeroSize(
        QStringLiteral("font-size\\s*:\\s*0(?:\\.0+)?(?:px|pt)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    QString html = _p.value("html").toString();
    html.replace(zeroSize, QStringLiteral("font-size:%1px").arg(_p.value("fontSize").toInt()));
    _p["html"] = html;
    Document doc;
    parse(doc, _p);
    if (doc.characterCount() > 100000 || !validFormatting(_p, doc.characterCount() - 1))
        return false;
    // Stored text is self-contained: images, external objects and nested frames are not imported.
    for (auto block = doc.begin(); block.isValid(); block = block.next())
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto f = it.fragment();
            if (f.isValid() && f.charFormat().isImageFormat())
                return false;
            const auto  format    = f.charFormat();
            const qreal pixelSize = format.property(QTextFormat::FontPixelSize).toDouble();
            const qreal pointSize = format.fontPointSize();
            if (!std::isfinite(pixelSize) || !std::isfinite(pointSize) || pixelSize > 1024 ||
                pointSize > 768)
                return false;
            for (int key : {QTextFormat::FontLetterSpacing, QTextFormat::TextBaselineOffset}) {
                const qreal value = format.property(key).toDouble();
                if (!std::isfinite(value) || qAbs(value) > 32768)
                    return false;
            }
        }
    if (doc.rootFrame()->childFrames().size() > 0)
        return false;
    _p["html"] = doc.toHtml();
    return true;
}
QSizeF size(const QVariantMap &_p)
{
    if (_p.value("paragraph").toBool())
        return {_p.value("width").toDouble(), _p.value("height").toDouble()};
    Document doc;
    configure(doc, _p);
    return doc.size();
}
QString plainText(const QVariantMap &_p)
{
    Document doc;
    configure(doc, _p);
    return doc.toPlainText();
}
namespace
{
QRect documentBounds(QTextDocument &_doc, const QVariantMap &_p, const QSize &_canvas,
                     const QTransform &matrix, bool followRelief)
{
    qreal margin = _p.value("fontSize", 64).toDouble() * 2;
    if (!_p.value("paragraph").toBool())
        for (auto block = _doc.begin(); block.isValid(); block = block.next())
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const auto  f  = it.fragment().charFormat();
                const qreal px = f.hasProperty(QTextFormat::FontPixelSize)
                                     ? f.property(QTextFormat::FontPixelSize).toDouble()
                                     : qMax(1., f.fontPointSize() * 96. / 72.);
                margin         = qMax(margin, px * (2 + qAbs(f.baselineOffset()) / 100.));
            }
    const auto local =
        _p.value("paragraph").toBool()
            ? QRectF(QPointF{}, size(_p))
            : QRectF(QPointF{}, _doc.size()).adjusted(-margin, -margin, margin, margin);
    auto mapped = matrix.mapRect(local);
    if (!std::isfinite(mapped.x()) || !std::isfinite(mapped.y()) ||
        !std::isfinite(mapped.width()) || !std::isfinite(mapped.height()))
        return QRect(QPoint{}, _canvas);
    if (followRelief) {
        // The inverse warp can move glyphs outside their original projected box.
        const qreal extra = .55 * qMax(mapped.width(), mapped.height());
        mapped.adjust(-extra, -extra, extra, extra);
    }
    return mapped.intersected(QRectF(QPointF{}, _canvas))
        .toAlignedRect()
        .intersected(QRect(QPoint{}, _canvas));
}
void drawDocument(QPainter &_painter, QTextDocument &_doc, const QVariantMap &_p,
                  const QPoint &_origin, const QTransform &matrix)
{
    _painter.setRenderHint(QPainter::TextAntialiasing);
    _painter.setWorldTransform(matrix * QTransform::fromTranslate(-_origin.x(), -_origin.y()));
    if (_p.value("paragraph").toBool())
        _painter.setClipRect(QRectF(QPointF{}, size(_p)));
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, QColor(_p.value("color").toString()));
    _doc.documentLayout()->draw(&_painter, context);
}
} // namespace
QPolygonF surfaceOutline(const Layer &layer, const QSize &canvas, QPointF &bottomRight)
{
    const auto &parameters = layer.parameters;
    const qreal strength   = parameters.value("surfaceStrength", 1.).toDouble();
    if (layer.surfaceNormals.isNull() || !parameters.value("surfaceRelief", true).toBool() ||
        strength <= 0.)
        return {};
    Document doc;
    configure(doc, parameters);
    if (doc.toPlainText().trimmed().isEmpty())
        return {};
    const auto      extent = parameters.value("paragraph").toBool() ? size(parameters) : doc.size();
    const auto      matrix = displayTransform(parameters, &layer, extent);
    const auto      region = documentBounds(doc, parameters, canvas, matrix, true);
    const qreal     width  = qMax(32., extent.width());
    const qreal     height = qMax(32., extent.height());
    const QPolygonF corners{{0., 0.}, {width, 0.}, {width, height}, {0., height}};
    return Ai::projectSurfaceTextOutline(
        matrix.map(corners), region, matrix.map(QPointF{}), layer.surfaceNormals,
        layer.surfaceNormalCanvas, layer.surfaceNormalTransform,
        parameters.value("surfaceFocal", .85).toDouble(), strength, bottomRight);
}
QRect bounds(const QVariantMap &_p, const QSize &_canvas, const Layer *surface)
{
    Document doc;
    configure(doc, _p);
    const auto extent = _p.value("paragraph").toBool() ? size(_p) : doc.size();
    const bool relief = surface && !surface->surfaceNormals.isNull() &&
                        _p.value("surfaceRelief", true).toBool() &&
                        _p.value("surfaceStrength", 1.).toDouble() > 0.;
    return documentBounds(doc, _p, _canvas, displayTransform(_p, surface, extent), relief);
}
PixelPlane renderPlane(const QVariantMap &_p, const QSize &_size, QImage::Format _format,
                       const Layer *surface)
{
    if (_size.isEmpty() || _size.width() > 16384 || _size.height() > 16384 ||
        qint64(_size.width()) * _size.height() > 32LL * 1024 * 1024 ||
        (_format != QImage::Format_ARGB32_Premultiplied &&
         _format != QImage::Format_RGBA64_Premultiplied))
        return {};
    Document doc;
    configure(doc, _p);
    PixelPlane result(ImageUtils::blank(QSize(1, 1), _format));
    if (result.isNull())
        return {};
    if (doc.toPlainText().trimmed().isEmpty())
        return result;
    const auto  extent   = _p.value("paragraph").toBool() ? size(_p) : doc.size();
    const qreal strength = _p.value("surfaceStrength", 1.).toDouble();
    const bool  relief   = surface && !surface->surfaceNormals.isNull() &&
                           _p.value("surfaceRelief", true).toBool() && strength > 0.;
    const auto  matrix   = displayTransform(_p, surface, extent);
    const QRect region   = documentBounds(doc, _p, _size, matrix, relief);
    if (region.isEmpty())
        return result;
    QImage pixels = ImageUtils::blank(region.size(), _format);
    if (pixels.isNull())
        return {};
    QPainter painter(&pixels);
    drawDocument(painter, doc, _p, region.topLeft(), matrix);
    painter.end();
    if (relief)
        pixels = Ai::projectSurfaceText(pixels, region.topLeft(), matrix.map(QPointF{}),
                                        surface->surfaceNormals, surface->surfaceNormalCanvas,
                                        surface->surfaceNormalTransform,
                                        _p.value("surfaceFocal", .85).toDouble(), strength);
    if (pixels.isNull() || !result.writeRegion(region.topLeft(), pixels, _size))
        return {};
    return result;
}

QImage render(const QVariantMap &_p, const QSize &_size, QImage::Format _format)
{
    Document doc;
    configure(doc, _p);
    if (doc.toPlainText().trimmed().isEmpty())
        return ImageUtils::blank(QSize(1, 1), _format);
    QImage result = ImageUtils::blank(_size, _format);
    if (result.isNull())
        return {};
    QPainter painter(&result);
    drawDocument(painter, doc, _p, {}, transform(_p));
    return result;
}
} // namespace PhotoCrap::Text
