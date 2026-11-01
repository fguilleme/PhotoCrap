#include "Document.h"
#include "ImageUtils.h"

bool Document::toolNeedsNewLayer() const
{
    if (m_tool == "text")
        return true;
    if (m_tool == "rectangle" || m_tool == "ellipse" || m_tool == "triangle" ||
        m_tool == "polygon" || m_tool == "star" || m_tool == "line")
        return m_shapeNewLayer;
    return m_autoCreatePaintLayer && (m_tool == "brush" || m_tool == "pencil") && !m_editingMask &&
           m_active >= 0 && m_active < m_layers.size() && m_layers[m_active].kind != "pixel";
}

bool Document::startToolLayer(const QString &_label)
{
    if (!validSize(width(), height(), int(m_layers.size()) + 1) ||
        !validLayerStorage(width(), height(), 1))
        return false;
    const QImage blank = ImageUtils::blank(QSize(1, 1), workingFormat());
    if (blank.isNull())
        return fail(tr("Mémoire insuffisante pour créer le calque."));
    // A properties session must not later restore a snapshot that predates the new content.
    finishLayerPreview(tr("Modifier les propriétés du calque"));
    acceptLayerPropertiesSession();
    m_beforeStroke    = snapshot(_label);
    int     insertion = m_active + 1;
    QString groupId;
    if (m_active >= 0 && m_active < m_layers.size()) {
        groupId = m_layers[m_active].groupId;
        for (int i = 0; i < m_layers.size(); ++i) {
            auto &group = m_layers[i];
            if (group.kind != "group" || group.groupId != groupId || groupId.isEmpty())
                continue;
            if (group.locked || !group.visible) {
                // Leave protected/hidden folders intact and create a visible sibling above them.
                insertion = i + 1;
                groupId.clear();
            } else {
                group.groupCollapsed = false;
                if (m_layers[m_active].kind == "group")
                    insertion = m_active; // Children precede their folder header in storage order.
            }
            break;
        }
    }
    Layer layer(m_tool == "text" ? m_text.simplified().left(64) : _label, blank);
    layer.groupId = groupId;
    m_layers.insert(insertion, layer);
    m_active      = insertion;
    m_editingMask = false;
    selectOnly(m_active);
    showCompositeView();
    m_strokeCreatedLayer = true;
    m_fullCompositeDirty = true;
    ++m_thumbnailRevision;
    emit thumbnailsChanged();
    emit layersChanged();
    emit toolLayerCreated();
    return true;
}

void Document::setShapeNewLayer(bool _value)
{
    if (m_shapeNewLayer == _value)
        return;
    endStroke();
    m_shapeNewLayer = _value;
    emit settingsChanged();
}

void Document::setAutoCreatePaintLayer(bool _value)
{
    if (m_autoCreatePaintLayer == _value)
        return;
    endStroke();
    m_autoCreatePaintLayer = _value;
    emit settingsChanged();
}
