#pragma once

#include <QObject>
#include <QVariantList>

class ShortcutManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY shortcutsChanged)
    Q_PROPERTY(int revision READ revision NOTIFY shortcutsChanged)

public:
    explicit ShortcutManager(QObject *parent = nullptr, bool persist = true);

    QVariantList entries() const;
    int          revision() const
    {
        return m_revision;
    }

    Q_INVOKABLE QString sequence(const QString &id) const;
    Q_INVOKABLE QString displaySequence(const QString &id) const;
    Q_INVOKABLE bool    setSequence(const QString &id, const QString &sequence);
    Q_INVOKABLE void    resetDefaults();

signals:
    void shortcutsChanged();
    void errorOccurred(const QString &message);

private:
    struct Entry {
        QString id;
        QString category;
        QString label;
        QString defaultSequence;
        QString sequence;
    };

    QVector<Entry> m_entries;
    bool           m_persist  = true;
    int            m_revision = 0;

    int            indexOf(const QString &id) const;
    static QString normalized(const QString &sequence);
    void           load();
    void           save(const Entry &entry);
    void           notifyChanged();
};
