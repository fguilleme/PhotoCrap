#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class WorkspaceSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool useShiftToCycleTools READ useShiftToCycleTools WRITE setUseShiftToCycleTools
                   NOTIFY toolBehaviorChanged)
    Q_PROPERTY(bool springLoadedTools READ springLoadedTools WRITE setSpringLoadedTools NOTIFY
                   toolBehaviorChanged)
    Q_PROPERTY(bool autoCreatePaintLayer READ autoCreatePaintLayer WRITE setAutoCreatePaintLayer
                   NOTIFY toolBehaviorChanged)
    Q_PROPERTY(QVariantMap layout READ layout NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList toolLayout READ toolLayout NOTIFY toolLayoutChanged)
    Q_PROPERTY(
        int scratchDiskGiB READ scratchDiskGiB WRITE setScratchDiskGiB NOTIFY scratchCacheChanged)
    Q_PROPERTY(QString scratchDirectory READ scratchDirectory WRITE setScratchDirectory NOTIFY
                   scratchCacheChanged)
    Q_PROPERTY(int textureCacheMiB READ textureCacheMiB WRITE setTextureCacheMiB NOTIFY
                   textureCacheChanged)

    Q_PROPERTY(int planeRamMiB READ planeRamMiB WRITE setPlaneRamMiB NOTIFY planeRamChanged)

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
    bool autoCreatePaintLayer() const
    {
        return m_autoCreatePaintLayer;
    }
    void setAutoCreatePaintLayer(bool _value);
    void setUseShiftToCycleTools(bool _value);
    void setSpringLoadedTools(bool _value);
    int  scratchDiskGiB() const
    {
        return m_scratchDiskGiB;
    }
    QString scratchDirectory() const
    {
        return m_scratchDirectory;
    }
    void             setScratchDiskGiB(int _value);
    void             setScratchDirectory(const QString &_value);
    Q_INVOKABLE void setScratchDirectoryUrl(const QUrl &_url);
    int              textureCacheMiB() const
    {
        return m_textureCacheMiB;
    }
    void setTextureCacheMiB(int _value);
    int  planeRamMiB() const
    {
        return m_planeRamMiB;
    }
    void setPlaneRamMiB(int value);

signals:
    void toolBehaviorChanged();
    void layoutChanged();
    void toolLayoutChanged();
    void scratchCacheChanged();
    void textureCacheChanged();
    void planeRamChanged();

private:
    static QVariantList defaultToolLayout();
    static QVariantList normalizedToolLayout(const QVariantList &_layout);

    QVariantMap  m_layout;
    QVariantList m_toolLayout;
    bool         m_persist              = true;
    bool         m_useShiftToCycleTools = true, m_springLoadedTools = true;
    bool         m_autoCreatePaintLayer = true;
    int          m_planeRamMiB          = 256;
    int          m_scratchDiskGiB       = 4;
    int          m_textureCacheMiB      = 64;
    QString      m_scratchDirectory;
};
