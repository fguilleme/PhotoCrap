#include "WorkspaceSettings.h"

#include <QMap>
#include <QSet>
#include <QSettings>

WorkspaceSettings::WorkspaceSettings(QObject *_parent, bool _persist)
    : QObject(_parent), m_persist(_persist)
{
    if (m_persist) {
        m_layout     = QSettings().value(QStringLiteral("workspace/layout")).toMap();
        m_toolLayout = QSettings().value(QStringLiteral("workspace/toolLayout")).toList();
    }
    m_toolLayout = normalizedToolLayout(m_toolLayout);
}

QVariantList WorkspaceSettings::defaultToolLayout()
{
    const auto group = [](const char *_id, const char *_shortcut, const char *_key,
                          const char *_active, std::initializer_list<const char *> _tools) {
        QStringList tools;
        for (const char *tool : _tools)
            tools.append(QString::fromLatin1(tool));
        return QVariantMap{{QStringLiteral("id"), QString::fromLatin1(_id)},
                           {QStringLiteral("shortcut"), QString::fromLatin1(_shortcut)},
                           {QStringLiteral("key"), QString::fromLatin1(_key)},
                           {QStringLiteral("active"), QString::fromLatin1(_active)},
                           {QStringLiteral("tools"), tools},
                           {QStringLiteral("hidden"), QStringList{}}};
    };
    return {
        group("move", "toolMove", "V", "move", {"move", "artboard"}),
        group("selection", "toolSelect", "M", "select",
              {"select", "ellipticalSelect", "singleRowSelect", "singleColumnSelect"}),
        group("lasso", "", "L", "lasso",
              {"selectionBrush", "lasso", "polygonalLasso", "magneticLasso"}),
        group("smartSelection", "", "W", "objectSelection",
              {"objectSelection", "quickSelection", "magicWand"}),
        group("crop", "toolCrop", "C", "crop", {"crop", "perspectiveCrop", "slice", "sliceSelect"}),
        group("frame", "", "K", "frame", {"frame"}),
        group("measure", "toolPicker", "I", "picker",
              {"picker", "colorSampler", "ruler", "note", "count"}),
        group("healing", "", "J", "spotHealing",
              {"spotHealing", "remove", "healing", "patch", "contentAwareMove", "redEye"}),
        group("brush", "toolBrush", "B", "brush",
              {"brush", "pencil", "colorReplacement", "mixerBrush"}),
        group("stamp", "", "S", "cloneStamp", {"cloneStamp", "patternStamp"}),
        group("historyBrush", "", "Y", "historyBrush", {"historyBrush", "artHistoryBrush"}),
        group("eraser", "toolEraser", "E", "eraser", {"eraser", "backgroundEraser", "magicEraser"}),
        group("paint", "toolFill", "G", "gradient", {"gradient", "fill"}),
        group("blur", "", "", "blur", {"blur", "sharpen", "smudge"}),
        group("adjustmentBrush", "", "", "adjustmentBrush", {"adjustmentBrush"}),
        group("dodge", "", "O", "dodge", {"dodge", "burn", "sponge"}),
        group("pen", "", "P", "pen",
              {"pen", "freeformPen", "curvaturePen", "addAnchor", "deleteAnchor", "convertPoint"}),
        group("type", "toolText", "T", "text",
              {"text", "verticalText", "verticalTypeMask", "horizontalTypeMask"}),
        group("pathSelection", "", "A", "pathSelection", {"pathSelection", "directSelection"}),
        group("shape", "toolRectangle", "U", "rectangle",
              {"rectangle", "ellipse", "triangle", "polygon", "star", "line", "customShape"}),
        group("hand", "toolHand", "H", "hand", {"hand", "rotateView"}),
        group("zoom", "", "Z", "zoomTool", {"zoomTool"})};
}

QVariantList WorkspaceSettings::normalizedToolLayout(const QVariantList &_layout)
{
    const QVariantList defaults = defaultToolLayout();
    if (_layout.isEmpty())
        return defaults;

    QMap<QString, QVariantMap> defaultsById;
    for (const QVariant &entry : defaults) {
        const QVariantMap group = entry.toMap();
        defaultsById.insert(group.value(QStringLiteral("id")).toString(), group);
    }

    QVariantList  result;
    QSet<QString> seen;
    for (const QVariant &entry : _layout) {
        const QVariantMap saved = entry.toMap();
        const QString     id    = saved.value(QStringLiteral("id")).toString();
        if (!defaultsById.contains(id) || seen.contains(id))
            continue;
        QVariantMap       group     = defaultsById.value(id);
        const QStringList available = group.value(QStringLiteral("tools")).toStringList();
        QStringList       ordered;
        for (const QString &tool : saved.value(QStringLiteral("tools")).toStringList())
            if (available.contains(tool) && !ordered.contains(tool))
                ordered.append(tool);
        for (const QString &tool : available)
            if (!ordered.contains(tool))
                ordered.append(tool);
        QStringList hidden;
        for (const QString &tool : saved.value(QStringLiteral("hidden")).toStringList())
            if (available.contains(tool))
                hidden.append(tool);
        const QString active = saved.value(QStringLiteral("active")).toString();
        group.insert(QStringLiteral("tools"), ordered);
        group.insert(QStringLiteral("hidden"), hidden);
        if (available.contains(active))
            group.insert(QStringLiteral("active"), active);
        result.append(group);
        seen.insert(id);
    }
    for (const QVariant &entry : defaults) {
        const QVariantMap group = entry.toMap();
        if (!seen.contains(group.value(QStringLiteral("id")).toString()))
            result.append(group);
    }
    return result;
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

void WorkspaceSettings::saveToolLayout(const QVariantList &_layout)
{
    const QVariantList normalized = normalizedToolLayout(_layout);
    if (m_toolLayout == normalized)
        return;
    m_toolLayout = normalized;
    if (m_persist)
        QSettings().setValue(QStringLiteral("workspace/toolLayout"), m_toolLayout);
    emit toolLayoutChanged();
}

void WorkspaceSettings::resetToolLayout()
{
    const QVariantList defaults = defaultToolLayout();
    if (m_toolLayout == defaults)
        return;
    m_toolLayout = defaults;
    if (m_persist)
        QSettings().remove(QStringLiteral("workspace/toolLayout"));
    emit toolLayoutChanged();
}
