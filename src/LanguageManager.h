#pragma once

#include <QObject>
#include <QTranslator>
#include <QVariantList>

class LanguageManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages NOTIFY languageChanged)

public:
    explicit LanguageManager(QObject *_parent = nullptr, bool _persist = true);

    QString language() const
    {
        return m_language;
    }
    QVariantList     availableLanguages() const;
    Q_INVOKABLE bool setLanguage(const QString &_language);

signals:
    void languageChanged();
    void errorOccurred(const QString &_message);

private:
    QString     m_language = QStringLiteral("fr");
    QTranslator m_translator;
    bool        m_persist = true;

    bool install(const QString &_language);
};
