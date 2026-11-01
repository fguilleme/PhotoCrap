#include "TextEditorController.h"
#include "TextLayer.h"
#include <QAbstractTextDocumentLayout>
#include <QFontDatabase>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>

QStringList TextEditorController::fontFamilies() const
{
    return QFontDatabase::families();
}
QVariantMap TextEditorController::format(int _position) const
{
    if (!m_document)
        return {};
    auto       *doc = m_document->textDocument();
    QTextCursor cursor(doc);
    cursor.setPosition(qBound(0, _position, doc->characterCount() - 1));
    auto f = cursor.charFormat();
    if (f.hasProperty(QTextFormat::FontPixelSize) &&
        f.property(QTextFormat::FontPixelSize).toDouble() <= 0)
        f.clearProperty(QTextFormat::FontPixelSize);
    const auto b        = cursor.blockFormat();
    auto       fallback = doc->defaultFont();
    if (fallback.pixelSize() <= 0 && fallback.pointSizeF() <= 0)
        fallback.setPixelSize(16);
    const auto  font = f.font().resolve(fallback);
    const qreal size = f.hasProperty(QTextFormat::FontPixelSize)
                           ? f.property(QTextFormat::FontPixelSize).toDouble()
                       : font.pointSizeF() > 0 ? font.pointSizeF() * 96. / 72.
                                               : doc->defaultFont().pixelSize();
    QVariantMap result{
        {"family", font.family()},
        {"style", font.styleName().isEmpty() ? QFontDatabase::styleString(font) : font.styleName()},
        {"size", size},
        {"bold", font.bold()},
        {"italic", font.italic()},
        {"underline", font.underline()},
        {"strike", font.strikeOut()},
        {"caps", font.capitalization() == QFont::AllUppercase ? 1
                 : font.capitalization() == QFont::SmallCaps  ? 2
                                                              : 0},
        {"script", f.verticalAlignment() == QTextCharFormat::AlignSuperScript ? 1
                   : f.verticalAlignment() == QTextCharFormat::AlignSubScript ? 2
                                                                              : 0},
        {"kerning", font.kerning()},
        {"rtl", b.layoutDirection() == Qt::RightToLeft},
        {"alignment", int(b.alignment())},
        {"leading", b.lineHeightType() == QTextBlockFormat::FixedHeight ? b.lineHeight() : 0.},
        {"tracking", font.letterSpacingType() == QFont::AbsoluteSpacing
                         ? font.letterSpacing() * 1000. / qMax(1., size)
                         : 0.},
        {"stretch", font.stretch()},
        {"baseline", f.baselineOffset()},
        {"before", b.topMargin()},
        {"after", b.bottomMargin()},
        {"left", b.leftMargin()},
        {"right", b.rightMargin()},
        {"indent", b.textIndent()}};
    if (f.foreground().style() != Qt::NoBrush)
        result["color"] = f.foreground().color();
    return result;
}
void TextEditorController::apply(const QVariantMap &_values, int _start, int _end)
{
    if (!m_document)
        return;
    auto       *doc = m_document->textDocument();
    QTextCursor cursor(doc);
    if (_start == _end)
        cursor.select(QTextCursor::Document);
    else {
        cursor.setPosition(qBound(0, _start, doc->characterCount() - 1));
        cursor.setPosition(qBound(0, _end, doc->characterCount() - 1), QTextCursor::KeepAnchor);
    }
    auto f = QTextCharFormat{};
    auto b = QTextBlockFormat{};
    for (auto it = _values.begin(); it != _values.end(); ++it) {
        const auto &v   = it.value();
        const auto &key = it.key();
        if (key == "family")
            f.setFontFamilies({v.toString()});
        else if (key == "style") {
            const auto family = format(cursor.position()).value("family").toString();
            if (QFontDatabase::styles(family).isEmpty()) {
                f.setFontWeight(v.toString().contains("Bold") ? QFont::Bold : QFont::Normal);
                f.setFontItalic(v.toString().contains("Italic"));
                f.setFontStretch(100);
            } else {
                const auto font = QFontDatabase::font(family, v.toString(), 12);
                f.setFontWeight(font.weight());
                f.setFontItalic(font.italic());
                f.setFontStretch(font.stretch() > 0 ? font.stretch() : 100);
            }
        } else if (key == "size")
            f.setProperty(QTextFormat::FontPixelSize, qBound(1, qRound(v.toDouble()), 1024));
        else if (key == "bold")
            f.setFontWeight(v.toBool() ? QFont::Bold : QFont::Normal);
        else if (key == "italic")
            f.setFontItalic(v.toBool());
        else if (key == "underline")
            f.setFontUnderline(v.toBool());
        else if (key == "strike")
            f.setFontStrikeOut(v.toBool());
        else if (key == "color" && QColor(v.toString()).isValid())
            f.setForeground(QColor(v.toString()));
        else if (key == "tracking") {
            const auto state = format(cursor.position());
            f.setFontLetterSpacingType(QFont::AbsoluteSpacing);
            f.setFontLetterSpacing(qBound(-1000., v.toDouble(), 1000.) *
                                   state.value("size").toDouble() / 1000.);
        } else if (key == "stretch")
            f.setFontStretch(qBound(1, v.toInt(), 4000));
        else if (key == "baseline")
            f.setBaselineOffset(qBound(-1000., v.toDouble(), 1000.));
        else if (key == "caps")
            f.setFontCapitalization(v.toInt() == 1   ? QFont::AllUppercase
                                    : v.toInt() == 2 ? QFont::SmallCaps
                                                     : QFont::MixedCase);
        else if (key == "script")
            f.setVerticalAlignment(v.toInt() == 1   ? QTextCharFormat::AlignSuperScript
                                   : v.toInt() == 2 ? QTextCharFormat::AlignSubScript
                                                    : QTextCharFormat::AlignNormal);
        else if (key == "kerning")
            f.setFontKerning(v.toBool());
        else if (key == "rtl")
            b.setLayoutDirection(v.toBool() ? Qt::RightToLeft : Qt::LeftToRight);
        else if (key == "alignment")
            b.setAlignment(Qt::Alignment(v.toInt()));
        else if (key == "leading")
            b.setLineHeight(v.toDouble() > 0 ? qBound(1., v.toDouble(), 4096.) : 100.,
                            v.toDouble() > 0 ? QTextBlockFormat::FixedHeight
                                             : QTextBlockFormat::ProportionalHeight);
        else if (key == "before")
            b.setTopMargin(qBound(0., v.toDouble(), 4096.));
        else if (key == "after")
            b.setBottomMargin(qBound(0., v.toDouble(), 4096.));
        else if (key == "left")
            b.setLeftMargin(qBound(0., v.toDouble(), 4096.));
        else if (key == "right")
            b.setRightMargin(qBound(0., v.toDouble(), 4096.));
        else if (key == "indent")
            b.setTextIndent(qBound(-4096., v.toDouble(), 4096.));
    }
    cursor.beginEditBlock();
    if (!f.isEmpty())
        cursor.mergeCharFormat(f);
    if (!b.isEmpty())
        cursor.mergeBlockFormat(b);
    cursor.endEditBlock();
}
QVariantList TextEditorController::selectionRects(int _start, int _end) const
{
    QVariantList result;
    if (!m_document || _start == _end)
        return result;
    auto *doc = m_document->textDocument();
    if (_start > _end)
        qSwap(_start, _end);
    for (auto block = doc->findBlock(_start); block.isValid() && block.position() < _end;
         block      = block.next()) {
        const auto *layout = block.layout();
        const auto  bounds = doc->documentLayout()->blockBoundingRect(block);
        for (int i = 0; i < layout->lineCount(); ++i) {
            const auto line = layout->lineAt(i);
            const int  a    = qMax(_start - block.position(), line.textStart());
            const int  b    = qMin(_end - block.position(), line.textStart() + line.textLength());
            if (a >= b)
                continue;
            const auto x1 = line.cursorToX(a), x2 = line.cursorToX(b);
            result.append(QVariantMap{{"x", bounds.x() + qMin(x1, x2)},
                                      {"y", bounds.y() + line.y()},
                                      {"width", qMax(1., qAbs(x2 - x1))},
                                      {"height", line.height()}});
        }
    }
    return result;
}

QVariantMap TextEditorController::snapshot() const
{
    return m_document ? PhotoCrap::Text::formatting(*m_document->textDocument()) : QVariantMap{};
}
void TextEditorController::restoreFormatting(const QVariantMap &_parameters)
{
    if (m_document && PhotoCrap::Text::validFormatting(
                          _parameters, m_document->textDocument()->characterCount() - 1))
        PhotoCrap::Text::applyFormatting(*m_document->textDocument(), _parameters);
    if (m_document)
        m_document->textDocument()->clearUndoRedoStacks();
}

QStringList TextEditorController::fontStyles(const QString &_family) const
{
    const auto styles = QFontDatabase::styles(_family);
    return styles.isEmpty() ? QStringList{"Regular", "Italic", "Bold", "Bold Italic"} : styles;
}
bool TextEditorController::fontAvailable(const QString &_family) const
{
    return QStringList{"sans-serif", "Sans Serif", "serif", "monospace"}.contains(
               _family, Qt::CaseInsensitive) ||
           QFontDatabase::families().contains(_family, Qt::CaseInsensitive);
}
