#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class WorkspaceSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool useShiftToCycleTools READ useShiftToCycleTools WRITE setUseShiftToCycleTools
                   NOTIFY toolBehaviorChanged)
    Q_PROPERTY(bool springLoadedTools READ springLoadedTools WRITE setSpringLoadedTools NOTIFY
                   toolBehaviorChanged)
    Q_PROPERTY(QVariantMap layout READ layout NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList toolLayout READ toolLayout NOTIFY toolLayoutChanged)

public:
    explicit WorkspaceSettings(QObject *_parent = nullptr, bool _persist = true);

    QVariantMap layout() const
    {
        return m_layout;
    }
    Q_INVOKABLE void saveLayout(const QVariantMap &_layout);
    QVariantList     toolLayout() const
    {
        return m_toolLayout;
    }
    Q_INVOKABLE void saveToolLayout(const QVariantList &_layout);
    Q_INVOKABLE void resetToolLayout();
    bool             useShiftToCycleTools() const
    {
        return m_useShiftToCycleTools;
    }
    bool springLoadedTools() const
    {
        return m_springLoadedTools;
    }
    void setUseShiftToCycleTools(bool _value);
    void setSpringLoadedTools(bool _value);

signals:
    void toolBehaviorChanged();
    void layoutChanged();
    void toolLayoutChanged();

private:
    static QVariantList defaultToolLayout();
    static QVariantList normalizedToolLayout(const QVariantList &_layout);

    QVariantMap  m_layout;
    QVariantList m_toolLayout;
    bool         m_persist              = true;
    bool         m_useShiftToCycleTools = true, m_springLoadedTools = true;
};
