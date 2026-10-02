#include "LanguageManager.h"

#include <QCoreApplication>
#include <QLocale>
#include <QSettings>

LanguageManager::LanguageManager(QObject *_parent, bool _persist)
    : QObject(_parent), m_persist(_persist)
{
    const QString saved =
        _persist ? QSettings()
                       .value(QStringLiteral("interface/language"), QStringLiteral("fr"))
                       .toString()
                 : QStringLiteral("fr");
    install(saved == QStringLiteral("en") ? saved : QStringLiteral("fr"));
}

QVariantList LanguageManager::availableLanguages() const
{
    return {QVariantMap{{"code", QStringLiteral("fr")}, {"name", tr("Français")}},
            QVariantMap{{"code", QStringLiteral("en")}, {"name", tr("English")}}};
}

bool LanguageManager::install(const QString &_language)
{
    if (_language != QStringLiteral("fr") && _language != QStringLiteral("en"))
        return false;
    QCoreApplication::removeTranslator(&m_translator);
    if (_language == QStringLiteral("en") &&
        !m_translator.load(QStringLiteral(":/i18n/PhotoCrap_en.qm")))
        return false;
    if (_language == QStringLiteral("en"))
        QCoreApplication::installTranslator(&m_translator);
    m_language = _language;
    QLocale::setDefault(QLocale(_language));
    return true;
}

bool LanguageManager::setLanguage(const QString &_language)
{
    if (_language == m_language)
        return true;
    if (!install(_language)) {
        emit errorOccurred(tr("Impossible de charger la langue sélectionnée."));
        return false;
    }
    if (m_persist)
        QSettings().setValue(QStringLiteral("interface/language"), m_language);
    emit languageChanged();
    return true;
}
