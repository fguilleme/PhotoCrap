#pragma once

#include <QObject>
#include <QVariantList>

class ShortcutManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY shortcutsChanged)
    Q_PROPERTY(int revision READ revision NOTIFY shortcutsChanged)

public:
    explicit ShortcutManager(QObject *_parent = nullptr, bool _persist = true);

    QVariantList entries() const;
    int          revision() const
    {
        return m_revision;
    }

    Q_INVOKABLE QVariantList filteredEntries(const QString &_query, int _filter = 0) const;
    Q_INVOKABLE QString      sequence(const QString &_id) const;
    Q_INVOKABLE QString      displaySequence(const QString &_id) const;
    Q_INVOKABLE bool         setSequence(const QString &_id, const QString &_sequence);
    Q_INVOKABLE void         resetDefaults();

signals:
    void shortcutsChanged();
    void errorOccurred(const QString &_message);

private:
    struct Entry {
        QString id;
        QString category;
        QString label;
        QString defaultSequence;
        QString sequence;
        QString context     = QStringLiteral("Document");
        bool    implemented = true;
        bool    editable    = true;
        QString gesture;
    };

    QVector<Entry> m_entries;
    bool           m_persist  = true;
    int            m_revision = 0;

    int            indexOf(const QString &_id) const;
    static QString normalized(const QString &_sequence);
    void           load();
    void           save(const Entry &_entry);
    void           notifyChanged();
};
