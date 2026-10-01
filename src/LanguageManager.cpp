#include "LanguageManager.h"

#include <QCoreApplication>
#include <QLocale>
#include <QSettings>

LanguageManager::LanguageManager(QObject *parent, bool persist)
    : QObject(parent), m_persist(persist) {
    const QString saved=persist?QSettings().value(QStringLiteral("interface/language"),QStringLiteral("fr")).toString():QStringLiteral("fr");
    install(saved==QStringLiteral("en")?saved:QStringLiteral("fr"));
}

QVariantList LanguageManager::availableLanguages() const {
    return {QVariantMap{{"code",QStringLiteral("fr")},{"name",tr("Français")}},
        QVariantMap{{"code",QStringLiteral("en")},{"name",tr("English")}}};
}

bool LanguageManager::install(const QString &language) {
    if(language!=QStringLiteral("fr")&&language!=QStringLiteral("en"))return false;
    QCoreApplication::removeTranslator(&m_translator);
    if(language==QStringLiteral("en")&&!m_translator.load(QStringLiteral(":/i18n/PhotoCrap_en.qm")))return false;
    if(language==QStringLiteral("en"))QCoreApplication::installTranslator(&m_translator);
    m_language=language;QLocale::setDefault(QLocale(language));return true;
}

bool LanguageManager::setLanguage(const QString &language) {
    if(language==m_language)return true;
    if(!install(language)) {emit errorOccurred(tr("Impossible de charger la langue sélectionnée."));return false;}
    if(m_persist)QSettings().setValue(QStringLiteral("interface/language"),m_language);
    emit languageChanged();return true;
}
