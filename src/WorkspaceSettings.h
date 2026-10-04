#pragma once

#include <QObject>
#include <QVariantMap>

class WorkspaceSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap layout READ layout NOTIFY layoutChanged)

public:
    explicit WorkspaceSettings(QObject *_parent = nullptr, bool _persist = true);

    QVariantMap layout() const
    {
        return m_layout;
    }
    Q_INVOKABLE void saveLayout(const QVariantMap &_layout);

signals:
    void layoutChanged();

private:
    QVariantMap m_layout;
    bool        m_persist = true;
};
