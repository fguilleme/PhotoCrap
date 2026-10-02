#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

class RecentFilesManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
public:
    explicit RecentFilesManager(QObject *_parent = nullptr, bool _persist = true);
    QVariantList entries() const;

    Q_INVOKABLE void add(const QUrl &_url);
    Q_INVOKABLE void remove(const QUrl &_url);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void refresh();
signals:
    void entriesChanged();

private:
    static constexpr int MaximumEntries = 10;
    QStringList          m_paths;
    bool                 m_persist = true;
    static QString       existingLocalPath(const QUrl &_url);
    void                 load();
    void                 save() const;
};
