#include "WorkspaceSettings.h"

#include <QSettings>

WorkspaceSettings::WorkspaceSettings(QObject *_parent, bool _persist)
    : QObject(_parent), m_persist(_persist)
{
    if (m_persist)
        m_layout = QSettings().value(QStringLiteral("workspace/layout")).toMap();
}

void WorkspaceSettings::saveLayout(const QVariantMap &_layout)
{
    if (m_layout == _layout)
        return;
    m_layout = _layout;
    if (m_persist)
        QSettings().setValue(QStringLiteral("workspace/layout"), m_layout);
    emit layoutChanged();
}
