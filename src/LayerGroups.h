#pragma once
#include "Layer.h"
#include <QHash>

// Group headers retain their own ID; all other rows refer to their immediate parent.
class LayerGroups
{
public:
    explicit LayerGroups(const QList<Layer> &layers) : m_layers(layers)
    {
        for (int i = 0; i < layers.size(); ++i)
            if (layers[i].kind == "group")
                m_headers.insert(layers[i].groupId, i);
    }

    QList<int> ancestors(const Layer &layer) const
    {
        QList<int> path;
        QString    id = layer.parentGroup();
        while (!id.isEmpty()) {
            const int header = m_headers.value(id, -1);
            if (header < 0 || path.contains(header))
                break;
            path.prepend(header);
            id = m_layers[header].parentGroupId;
        }
        return path;
    }

    bool visible(const Layer &layer) const
    {
        if (!layer.visible)
            return false;
        for (int i : ancestors(layer))
            if (!m_layers[i].visible)
                return false;
        return true;
    }

private:
    const QList<Layer> &m_layers;
    QHash<QString, int> m_headers;
};
