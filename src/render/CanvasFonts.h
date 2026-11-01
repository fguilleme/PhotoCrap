#pragma once
#include <QCache>
#include <QFont>
#include <QFontDatabase>
#include <QObject>
#include <QPainterPath>

// Resolve CSS' generic sans-serif as Canvas does, rather than as a QML font name.
class CanvasFonts : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QFont ruler READ ruler CONSTANT)
public:
    explicit CanvasFonts(QObject *_parent = nullptr) : QObject(_parent)
    {
        QFont generic;
        generic.setStyleHint(QFont::SansSerif);
        m_ruler.setFamily(QFontDatabase::hasFamily(QStringLiteral("sans-serif"))
                              ? QStringLiteral("sans-serif")
                              : generic.defaultFamily());
        m_ruler.setPixelSize(9);
    }
    QFont ruler() const
    {
        return m_ruler;
    }

    Q_INVOKABLE QString rulerOutline(int _value) const
    {
        const QString text = QString::number(_value);
        if (const auto *cached = m_outlines.object(text))
            return *cached;
        QPainterPath outline;
        outline.addText(0, 0, m_ruler, text);
        QString    svg;
        const auto pair = [](const QPainterPath::Element &_element) {
            return QString::number(_element.x, 'g', 17) + QLatin1Char(' ') +
                   QString::number(_element.y, 'g', 17);
        };
        for (int i = 0; i < outline.elementCount(); ++i) {
            const auto element = outline.elementAt(i);
            if (element.isMoveTo())
                svg += QLatin1Char('M') + pair(element);
            else if (element.isLineTo())
                svg += QLatin1Char('L') + pair(element);
            else if (element.type == QPainterPath::CurveToElement &&
                     i + 2 < outline.elementCount()) {
                svg += QLatin1Char('C') + pair(element) + QLatin1Char(' ') +
                       pair(outline.elementAt(i + 1)) + QLatin1Char(' ') +
                       pair(outline.elementAt(i + 2));
                i += 2;
            }
        }
        m_outlines.insert(text, new QString(svg));
        return svg;
    }

private:
    QFont                            m_ruler;
    mutable QCache<QString, QString> m_outlines{128};
};
