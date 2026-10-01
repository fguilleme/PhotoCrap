#include "RecentFilesManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

RecentFilesManager::RecentFilesManager(QObject *parent, bool persist)
    : QObject(parent), m_persist(persist) {
    load();
}

QString RecentFilesManager::existingLocalPath(const QUrl &url) {
    if(!url.isLocalFile()) return {};
    const QFileInfo info(url.toLocalFile());
    if(!info.exists() || !info.isFile()) return {};
    const QString canonical=info.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty()?info.absoluteFilePath():canonical);
}

QVariantList RecentFilesManager::entries() const {
    QVariantList result;
    result.reserve(m_paths.size());
    for(const QString &path:m_paths) {
        const QFileInfo info(path);
        result.append(QVariantMap{{"url",QUrl::fromLocalFile(path)},
                                  {"name",info.fileName()},
                                  {"directory",QDir::toNativeSeparators(info.absolutePath())},
                                  {"path",QDir::toNativeSeparators(path)}});
    }
    return result;
}

void RecentFilesManager::load() {
    if(!m_persist) return;
    const QStringList stored=QSettings().value(QStringLiteral("files/recent")).toStringList();
    for(const QString &path:stored) {
        const QString valid=existingLocalPath(QUrl::fromLocalFile(path));
        if(!valid.isEmpty() && !m_paths.contains(valid)) m_paths.append(valid);
        if(m_paths.size()>=MaximumEntries) break;
    }
    if(m_paths!=stored) save();
}

void RecentFilesManager::save() const {
    if(m_persist) QSettings().setValue(QStringLiteral("files/recent"),m_paths);
}

void RecentFilesManager::add(const QUrl &url) {
    const QString path=existingLocalPath(url);
    if(path.isEmpty()) return;
    m_paths.removeAll(path);
    m_paths.prepend(path);
    while(m_paths.size()>MaximumEntries) m_paths.removeLast();
    save();emit entriesChanged();
}

void RecentFilesManager::remove(const QUrl &url) {
    if(!url.isLocalFile()) return;
    QString path=existingLocalPath(url);
    if(path.isEmpty()) path=QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath());
    bool changed=false;
    for(int i=m_paths.size()-1;i>=0;--i) {
        const QString candidate=QDir::cleanPath(QFileInfo(m_paths[i]).absoluteFilePath());
        if(candidate==path) {m_paths.removeAt(i);changed=true;}
    }
    if(changed) {save();emit entriesChanged();}
}

void RecentFilesManager::clear() {
    if(m_paths.isEmpty()) return;
    m_paths.clear();save();emit entriesChanged();
}

void RecentFilesManager::refresh() {
    QStringList valid;
    for(const QString &path:m_paths) {
        const QString existing=existingLocalPath(QUrl::fromLocalFile(path));
        if(!existing.isEmpty() && !valid.contains(existing)) valid.append(existing);
    }
    if(valid==m_paths) return;
    m_paths=valid;save();emit entriesChanged();
}
