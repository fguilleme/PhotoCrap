#include "TextLayer.h"
#include "ImageUtils.h"
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
QRect bounds(const QVariantMap &_p, const QSize &_canvas)
{
    Document doc;
    configure(doc, _p);
    qreal margin = _p.value("fontSize", 64).toDouble() * 2;
    if (!_p.value("paragraph").toBool())
        for (auto block = doc.begin(); block.isValid(); block = block.next())
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
            : QRectF(QPointF{}, doc.size()).adjusted(-margin, -margin, margin, margin);
    const auto mapped = transform(_p).mapRect(local);
    if (!std::isfinite(mapped.x()) || !std::isfinite(mapped.y()) ||
        !std::isfinite(mapped.width()) || !std::isfinite(mapped.height()))
        return QRect(QPoint{}, _canvas);
    return mapped.intersected(QRectF(QPointF{}, _canvas))
        .toAlignedRect()
        .intersected(QRect(QPoint{}, _canvas));
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
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setWorldTransform(transform(_p));
    if (_p.value("paragraph").toBool())
        painter.setClipRect(QRectF(QPointF{}, size(_p)));
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, QColor(_p.value("color").toString()));
    doc.documentLayout()->draw(&painter, context);
    return result;
}
} // namespace PhotoCrap::Text
