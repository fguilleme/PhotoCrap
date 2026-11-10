#include "Document.h"
#include "ImageUtils.h"
#include "masks/VectorMask.h"

bool Document::editingVectorMask() const
{
    return !m_editingMask && m_active >= 0 && m_active < m_layers.size() && m_activePenPath >= 0 &&
           m_activePenPath < m_penPaths.size() &&
           m_penPaths[m_activePenPath].maskLayerId == m_layers[m_active].historyId;
}
void Document::addVectorMask()
{
    endPenGesture();
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].kind == "group" ||
        !penShapeEditable(m_active) || m_activePenPath < 0 ||
        m_penPaths[m_activePenPath].points.size() < 2)
        return;
    const auto path = m_penPaths[m_activePenPath];
    edit(tr("Créer un masque vectoriel"), [&] {
        auto &layer = m_layers[m_active];
        PhotoCrap::Masks::setVectorPath(layer, path, m_size);
        layer.parameters["_vectorEnabled"] = true;
        layer.parameters["_vectorDensity"] = 100.;
        layer.parameters["_vectorFeather"] = 0.;
        m_editingMask                      = false;
    });
    editVectorMask();
    requestPreviewRender(true, true);
}
void Document::editVectorMask()
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        !PhotoCrap::Masks::hasVector(m_layers[m_active]))
        return;
    finishPenPath();
    m_editingMask = false;
    syncPenShapePath(false);
    const auto id = m_layers[m_active].historyId;
    for (int i = 0; i < m_penPaths.size(); ++i)
        if (m_penPaths[i].maskLayerId == id) {
            setActivePenPath(i);
            setTool("directSelection");
            emit layersChanged();
            break;
        }
}
void Document::removeVectorMask()
{
    if (m_active < 0 || m_active >= m_layers.size() || !penShapeEditable(m_active))
        return;
    edit(tr("Supprimer le masque vectoriel"), [&] {
        for (const auto &key :
             {"_vectorMask", "_vectorEnabled", "_vectorDensity", "_vectorFeather"})
            m_layers[m_active].parameters.remove(key);
    });
    syncPenShapePath();
    requestPreviewRender(true, true);
}
void Document::setVectorMaskEnabled(bool _enabled)
{
    if (m_active < 0 || m_active >= m_layers.size() || !penShapeEditable(m_active) ||
        !PhotoCrap::Masks::hasVector(m_layers[m_active]))
        return;
    edit(tr("Activer le masque vectoriel"),
         [&] { m_layers[m_active].parameters["_vectorEnabled"] = _enabled; });
    requestPreviewRender(true, true);
}
QImage Document::vectorMaskImage(int _index, const QSize &_requested) const
{
    if (_index < 0 || _index >= m_layers.size())
        return {};
    const QSize size =
        _requested.isValid() ? m_size.scaled(_requested, Qt::KeepAspectRatio) : m_size;
    return PhotoCrap::Masks::vectorCoverage(m_layers[_index], size, QRect(QPoint{}, size));
}
void Document::rasterizeVectorMask()
{
    if (m_active < 0 || m_active >= m_layers.size() || !penShapeEditable(m_active) ||
        !PhotoCrap::Masks::hasVector(m_layers[m_active]))
        return;
    if (!m_layers[m_active].mask.isNull()) {
        fail(tr("Ce calque possède déjà un masque de fusion. Conservez les deux masques "
                "séparés ou supprimez le masque pixel avant de pixelliser le masque vectoriel."));
        return;
    }
    if (deferEdit(tr("Pixelliser le masque vectoriel"), editCost(false),
                  [](Document &_document) { _document.rasterizeVectorMask(); }))
        return;
    edit(tr("Pixelliser le masque vectoriel"), [&] {
        auto &layer                      = m_layers[m_active];
        layer.mask                       = vectorMaskImage(m_active);
        layer.maskEnabled                = layer.parameters.value("_vectorEnabled", true).toBool();
        layer.parameters["_maskDensity"] = layer.parameters.value("_vectorDensity", 100.);
        layer.parameters["_maskFeather"] = layer.parameters.value("_vectorFeather", 0.);
        for (const auto &key :
             {"_vectorMask", "_vectorEnabled", "_vectorDensity", "_vectorFeather"})
            layer.parameters.remove(key);
        m_editingMask = true;
    });
    syncPenShapePath();
    requestPreviewRender(true, true);
}
