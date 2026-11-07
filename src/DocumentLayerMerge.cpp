#include "Document.h"
#include "DocumentStorage.h"
#include "ImageUtils.h"
#include <QHash>
#include <QPainter>
#include <algorithm>
#include <utility>

bool Document::planLayerMerge(const QString &_requested, LayerMergePlan &_plan,
                              QString &_reason) const
{
    _plan             = {};
    const auto reject = [&_reason](const QString &_message) {
        _reason = _message;
        return false;
    };
    if (m_size.isEmpty() || m_active < 0 || m_active >= m_layers.size())
        return false;
    QString mode = _requested;
    if (mode == "selected" && m_selectedLayers.size() == 1)
        mode = m_layers[m_active].kind == "group" ? "group" : "down";
    _plan.stamp           = mode == "stampSelected" || mode == "stampVisible";
    _plan.white           = mode == "flatten";
    const bool allVisible = mode == "visible" || mode == "stampVisible" || mode == "flatten";
    QHash<QString, int> groups;
    for (int i = 0; i < m_layers.size(); ++i)
        if (m_layers[i].kind == "group")
            groups.insert(m_layers[i].groupId, i);
    const auto visible = [&](int _index) {
        const auto &layer = m_layers[_index];
        const int   group = groups.value(layer.groupId, -1);
        return layer.visible && (group < 0 || m_layers[group].visible);
    };
    const auto select = [&](int _index) {
        _plan.inputs.insert(_index);
        if (m_layers[_index].kind == "group")
            for (int i = 0; i < m_layers.size(); ++i)
                if (m_layers[i].groupId == m_layers[_index].groupId)
                    _plan.inputs.insert(i);
    };
    if (allVisible) {
        for (int i = 0; i < m_layers.size(); ++i)
            if (visible(i) && m_layers[i].kind != "group")
                _plan.inputs.insert(i);
    } else if (mode == "down") {
        if (m_selectedLayers.size() != 1 || m_active < 1 || m_layers[m_active].kind == "group")
            return false;
        const auto &lower = m_layers[m_active - 1];
        if (lower.kind != "pixel" || lower.groupId != m_layers[m_active].groupId ||
            !visible(m_active) || !visible(m_active - 1))
            return reject(tr("La fusion vers le bas nécessite un calque de pixels visible juste en "
                             "dessous, dans le même groupe."));
        _plan.inputs = {m_active - 1, m_active};
    } else if (mode == "clipping") {
        int base = m_active;
        while (base > 0 && m_layers[base].clippingMask &&
               m_layers[base - 1].groupId == m_layers[m_active].groupId &&
               m_layers[base - 1].kind != "group")
            --base;
        if (m_layers[base].kind != "pixel" || m_layers[base].clippingMask || !visible(base))
            return reject(tr("Sélectionnez la base de pixels visible d’un groupe d’écrêtage."));
        _plan.inputs.insert(base);
        for (int i = base + 1;
             i < m_layers.size() && m_layers[i].clippingMask &&
             m_layers[i].groupId == m_layers[base].groupId && m_layers[i].kind != "group";
             ++i)
            if (visible(i))
                _plan.inputs.insert(i);
        if (_plan.inputs.size() < 2)
            return false;
    } else if (mode == "group") {
        if (m_selectedLayers.size() != 1 || m_layers[m_active].kind != "group" ||
            !visible(m_active))
            return false;
        select(m_active);
    } else if (mode == "selected" || mode == "stampSelected") {
        for (int i : m_selectedLayers)
            if (i >= 0 && i < m_layers.size())
                select(i);
    } else
        return false;
    int visibleCount = 0, bottom = int(m_layers.size()), top = -1;
    for (int i : _plan.inputs) {
        bottom = qMin(bottom, i);
        top    = qMax(top, i);
        visibleCount += m_layers[i].kind != "group" && visible(i);
    }
    if (visibleCount == 0 && mode != "flatten")
        return false;
    if (!allVisible && mode != "group" && !_plan.stamp && _plan.inputs.size() < 2)
        return false;
    if (!allVisible && mode != "group" && !_plan.stamp &&
        (bottom >= m_layers.size() || m_layers[bottom].kind == "adjustment"))
        return reject(tr("La base de la fusion doit contenir des pixels, du texte ou une forme, et "
                         "non seulement des réglages."));
    // A clipped layer needs its original base. Never silently render it as blank
    // or reinterpret it against another selected layer after filtering the stack.
    if (!allVisible)
        for (int i : _plan.inputs) {
            if (!m_layers[i].clippingMask || !visible(i))
                continue;
            int base = i;
            while (base > 0 && m_layers[base].clippingMask && m_layers[base - 1].kind != "group" &&
                   m_layers[base - 1].groupId == m_layers[i].groupId)
                --base;
            if (!_plan.inputs.contains(base))
                return reject(tr("Incluez la base du masque d’écrêtage dans la fusion, ou utilisez "
                                 "Fusionner le masque d’écrêtage."));
        }
    if (!_plan.stamp) {
        _plan.removed = _plan.inputs;
        if (mode == "flatten") {
            for (int i = 0; i < m_layers.size(); ++i)
                _plan.removed.insert(i);
        } else if (allVisible) {
            // Keep folders needed by hidden children. Remove only empty visible folders.
            for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
                if (!m_layers[it.value()].visible)
                    continue;
                bool remains = false;
                for (int i = 0; i < m_layers.size(); ++i)
                    remains |= m_layers[i].kind != "group" && m_layers[i].groupId == it.key() &&
                               !_plan.removed.contains(i);
                if (!remains)
                    _plan.removed.insert(it.value());
            }
        }
        if (mode != "flatten")
            for (int i : _plan.removed) {
                const int group = groups.value(m_layers[i].groupId, -1);
                if (m_layers[i].locked || (group >= 0 && m_layers[group].locked))
                    return reject(tr("Déverrouillez les calques concernés avant de les fusionner. "
                                     "Une copie fusionnée conserve les originaux verrouillés."));
            }
    }
    if (!_plan.stamp && mode != "flatten" && _plan.removed.size() < 2 && bottom < m_layers.size() &&
        m_layers[bottom].kind == "pixel" && m_layers[bottom].opacity == 1 &&
        m_layers[bottom].blend == 0 && m_layers[bottom].mask.isNull())
        return false;
    _plan.insertion = qMax(0, top + 1);
    if (!allVisible && bottom < m_layers.size()) {
        const auto parent = m_layers[bottom].groupId;
        const bool same = std::all_of(_plan.inputs.cbegin(), _plan.inputs.cend(), [&](int _index) {
            return m_layers[_index].kind != "group" && m_layers[_index].groupId == parent;
        });
        if (same)
            _plan.groupId = parent;
    }
    // A root result must sit outside any partial folder, not split its children.
    if (_plan.groupId.isEmpty())
        for (int i : _plan.inputs) {
            const int group = groups.value(m_layers[i].groupId, -1);
            if (group >= 0)
                _plan.insertion = qMax(_plan.insertion, group + 1);
        }
    _plan.name =
        mode == "down" || mode == "clipping" ? m_layers[bottom].name : m_layers[m_active].name;
    if (mode == "flatten") {
        _plan.name  = tr("Arrière-plan");
        _plan.label = tr("Aplatir l’image");
    } else if (mode == "stampVisible") {
        _plan.name  = tr("Copie des calques visibles");
        _plan.label = tr("Créer un calque des visibles fusionnés");
    } else if (mode == "stampSelected") {
        _plan.name  = tr("Copie fusionnée");
        _plan.label = tr("Créer un calque des sélectionnés fusionnés");
    } else if (mode == "visible")
        _plan.label = tr("Fusionner les calques visibles");
    else if (mode == "down")
        _plan.label = tr("Fusionner vers le bas");
    else if (mode == "group")
        _plan.label = tr("Fusionner le groupe");
    else if (mode == "clipping")
        _plan.label = tr("Fusionner le masque d’écrêtage");
    else
        _plan.label = tr("Fusionner les calques sélectionnés");
    return true;
}

QVariantMap Document::layerMergeActions() const
{
    QVariantMap result;
    for (const QString mode : {"down", "selected", "visible", "stampSelected", "stampVisible",
                               "group", "clipping", "flatten"}) {
        LayerMergePlan plan;
        QString        reason;
        result.insert(mode, planLayerMerge(mode, plan, reason));
    }
    bool hidden = false;
    for (const auto &layer : m_layers)
        hidden |= !layer.visible;
    result.insert("hidden", hidden);
    return result;
}
void Document::mergeDown()
{
    mergeLayers("down");
}
void Document::mergeLayers(const QString &_mode)
{
    if (pixelEditBusy() || selectionBusy() || finishTextForLayer(m_active) < 0)
        return;
    endStroke();
    finishLayerPreview();
    acceptLayerPropertiesSession();
    LayerMergePlan plan;
    QString        reason;
    if (!planLayerMerge(_mode, plan, reason)) {
        if (!reason.isEmpty())
            fail(reason);
        return;
    }
    const int count = int(m_layers.size()) - plan.removed.size() + 1;
    if (!validSize(width(), height(), count))
        return;
    int colors = 1, masks = 0;
    for (int i = 0; i < m_layers.size(); ++i)
        if (!plan.removed.contains(i)) {
            const auto &layer = m_layers[i];
            colors += (!layer.image.isNull() && !ImageUtils::compact(layer.image)) +
                      !layer.source.isNull();
            masks += !layer.mask.isNull() && !ImageUtils::compact(layer.mask);
        }
    if (!validStorageBytes(
            DocumentStorage::estimateBytes(width(), height(), colors, masks, channelBits())))
        return;
    if (deferEdit(
            plan.label, editCost(false) * qMax(1, int(plan.inputs.size())),
            [plan](Document &_worker) { _worker.applyLayerMerge(plan); }, true))
        return;
    const auto before = snapshot({});
    if (applyLayerMerge(plan))
        commit(before, plan.label);
}
bool Document::applyLayerMerge(const LayerMergePlan &_plan)
{
    auto layers = m_layers;
    for (int i = 0; i < layers.size(); ++i)
        if (layers[i].kind != "group" && !_plan.inputs.contains(i))
            layers[i].visible = false;
    auto renderer                  = nativeCompositeSource(std::move(layers));
    renderer->m_backgroundCancel   = m_backgroundCancel;
    renderer->m_backgroundProgress = m_backgroundProgress;
    const int bands                = (height() + TiledImage::TileSize - 1) / TiledImage::TileSize;
    int       completed            = 0;
    auto      tiles =
        TiledImage::fromBands(m_size, workingFormat(), [&](const QRect &_region) -> QImage {
            if (!operationStep(completed++, bands))
                return {};
            auto image = renderer->compositeRegion(_region);
            if (image.isNull() || image.size() != _region.size())
                return {};
            if (_plan.white) {
                auto opaque = ImageUtils::blank(image.size(), workingFormat());
                if (opaque.isNull())
                    return {};
                opaque.fill(Qt::white);
                QPainter painter(&opaque);
                painter.drawImage(0, 0, image);
                painter.end();
                image = std::move(opaque);
            }
            return image;
        });
    if (!operationStep(bands, bands))
        return false;
    if (tiles.isNull())
        return fail(tr("Mémoire insuffisante pour fusionner les calques."));
    auto                pixels = PixelPlane::fromTiles(std::move(tiles));
    PreparedPlaneBudget budget;
    if (!prepareNativePlane(pixels, workingFormat(), budget))
        return false;
    Layer merged;
    merged.name    = _plan.name;
    merged.groupId = _plan.groupId;
    merged.image   = std::move(pixels);
    QVector<Layer> result;
    result.reserve(m_layers.size() - _plan.removed.size() + 1);
    int active = -1;
    for (int i = 0; i <= m_layers.size(); ++i) {
        if (i == _plan.insertion) {
            active = result.size();
            result.append(std::move(merged));
        }
        if (i < m_layers.size() && !_plan.removed.contains(i))
            result.append(m_layers[i]);
    }
    if (active < 0)
        return fail(tr("Impossible de placer le calque fusionné."));
    m_layers      = std::move(result);
    m_active      = active;
    m_editingMask = false;
    selectOnly(m_active);
    showCompositeView();
    return true;
}
