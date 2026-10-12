#include "Document.h"
#include "ImageUtils.h"

#include <QFont>
#include <QPainter>
#include <QTimer>
#include <QUuid>
#include <QtMath>
#include <algorithm>
#include <utility>

void Document::addLayer()
{
    if (!validSize(width(), height(), int(m_layers.size()) + 1))
        return;
    edit(tr("Nouveau calque"), [&] {
        m_layers.insert(++m_active, {tr("Calque %1").arg(m_layers.size() + 1),
                                     ImageUtils::blank(m_size, workingFormat())});
        selectOnly(m_active);
    });
}
void Document::duplicateLayer()
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind == QStringLiteral("group"))
        return;
    if (!validSize(width(), height(), int(m_layers.size()) + 1))
        return;
    edit(tr("Dupliquer le calque"), [&] {
        auto layer = m_layers[m_active];
        layer.name += tr(" — copie");
        layer.historyId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_layers.insert(++m_active, layer);
        selectOnly(m_active);
    });
}
void Document::removeLayer()
{
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].locked ||
        m_layers.size() == 1)
        return;
    edit(tr("Supprimer le calque"), [&] {
        if (m_layers[m_active].kind == QStringLiteral("group")) {
            const QString groupId = m_layers[m_active].groupId;
            for (int i = m_layers.size() - 1; i >= 0; --i)
                if (m_layers[i].groupId == groupId)
                    m_layers.removeAt(i);
        } else
            m_layers.removeAt(m_active);
        if (m_layers.isEmpty())
            m_layers.append({tr("Calque 1"), ImageUtils::blank(m_size, workingFormat())});
        m_active      = qMin(m_active, int(m_layers.size()) - 1);
        m_editingMask = false;
        selectOnly(m_active);
    });
}
void Document::moveLayer(int _direction)
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind == QStringLiteral("group"))
        return;
    const int to = m_active + (_direction > 0 ? 1 : -1);
    if (to < 0 || to >= m_layers.size())
        return;
    if (!m_layers[m_active].groupId.isEmpty() && m_layers[to].groupId != m_layers[m_active].groupId)
        return;
    edit(tr("Réordonner le calque"), [&] {
        m_layers.swapItemsAt(m_active, to);
        m_active = to;
        selectOnly(m_active);
    });
}
void Document::mergeDown()
{
    if (m_active < 1 || m_active >= m_layers.size())
        return;
    const auto &lower = m_layers[m_active - 1];
    const auto &upper = m_layers[m_active];
    if (lower.locked || upper.locked || lower.kind != "pixel" || upper.kind != "pixel" ||
        (lower.maskEnabled && !lower.mask.isNull()) ||
        (upper.maskEnabled && !upper.mask.isNull()) || !lower.visible || !upper.visible ||
        lower.blend != 0 || upper.blend != 0 || lower.groupId != upper.groupId) {
        fail(tr("La fusion nécessite deux calques visibles en mode Normal, non verrouillés."));
        return;
    }
    edit(tr("Fusionner vers le bas"), [&] {
        auto       &bottom = m_layers[m_active - 1];
        const auto &top    = m_layers[m_active];
        QImage      image  = ImageUtils::blank(m_size, workingFormat());
        QPainter    painter(&image);
        painter.setOpacity(bottom.opacity);
        painter.drawImage(0, 0, bottom.image);
        painter.setOpacity(top.opacity);
        painter.drawImage(0, 0, top.image);
        painter.end();
        bottom.image   = image;
        bottom.opacity = 1;
        m_layers.removeAt(m_active--);
        selectOnly(m_active);
    });
}
void Document::groupSelectedLayers()
{
    if (!canGroupSelected() || !validSize(width(), height(), int(m_layers.size()) + 1))
        return;
    edit(tr("Grouper les calques"), [&] {
        QList<int> indices = m_selectedLayers.values();
        std::sort(indices.begin(), indices.end());
        QVector<Layer> selected;
        selected.reserve(indices.size());
        for (const int index : std::as_const(indices))
            selected.append(m_layers[index]);
        for (auto it = indices.crbegin(); it != indices.crend(); ++it)
            m_layers.removeAt(*it);

        const QString groupId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const int insertion = qBound(0, indices.last() - indices.size() + 1, int(m_layers.size()));
        for (int i = 0; i < selected.size(); ++i) {
            selected[i].groupId = groupId;
            m_layers.insert(insertion + i, selected[i]);
        }
        const int groupCount =
            int(std::count_if(m_layers.cbegin(), m_layers.cend(), [](const Layer &_layer) {
                return _layer.kind == QStringLiteral("group");
            }));
        Layer group;
        group.name    = tr("Groupe %1").arg(groupCount + 1);
        group.kind    = QStringLiteral("group");
        group.groupId = groupId;
        m_active      = insertion + selected.size();
        m_layers.insert(m_active, group);
        m_editingMask = false;
        selectOnly(m_active);
    });
}
void Document::ungroupActiveLayers()
{
    if (!canUngroupActive())
        return;
    const QString groupId = m_layers[m_active].groupId;
    edit(tr("Sortir du groupe"), [&] {
        int header = -1;
        for (int i = 0; i < m_layers.size(); ++i) {
            if (m_layers[i].groupId != groupId)
                continue;
            if (m_layers[i].kind == QStringLiteral("group"))
                header = i;
            else
                m_layers[i].groupId.clear();
        }
        if (header >= 0)
            m_layers.removeAt(header);
        m_active      = qBound(0, header - 1, int(m_layers.size()) - 1);
        m_editingMask = false;
        selectOnly(m_active);
    });
}
void Document::toggleGroupCollapsed(int _index)
{
    if (_index < 0 || _index >= m_layers.size() || m_layers[_index].kind != QStringLiteral("group"))
        return;
    m_layers[_index].groupCollapsed = !m_layers[_index].groupCollapsed;
    selectOnly(_index);
    m_active      = _index;
    m_editingMask = false;
    emit layersChanged();
}
void Document::renameLayer(int _index, const QString &_name)
{
    if (_index < 0 || _index >= m_layers.size() || _name.trimmed().isEmpty() ||
        m_layers[_index].name == _name.trimmed())
        return;
    edit(tr("Renommer le calque"), [&] { m_layers[_index].name = _name.trimmed().left(256); });
}
void Document::setLayerVisible(int _i, bool _visible)
{
    if (_i < 0 || _i >= m_layers.size() || m_layers[_i].visible == _visible)
        return;
    m_visibilityBeforeIsolation.clear();
    m_isolatedLayer = -1;
    edit(tr("Visibilité du calque"), [&] { m_layers[_i].visible = _visible; });
}
void Document::toggleLayerIsolation(int _i)
{
    if (_i < 0 || _i >= m_layers.size())
        return;
    if (m_isolatedLayer == _i && m_visibilityBeforeIsolation.size() == m_layers.size()) {
        const QVector<bool> previousVisibility = m_visibilityBeforeIsolation;
        edit(tr("Restaurer la visibilité des calques"), [&] {
            for (int index = 0; index < m_layers.size(); ++index)
                m_layers[index].visible = previousVisibility[index];
        });
        m_visibilityBeforeIsolation.clear();
        m_isolatedLayer = -1;
        return;
    }

    QVector<bool> previousVisibility;
    previousVisibility.reserve(m_layers.size());
    for (const Layer &layer : std::as_const(m_layers))
        previousVisibility.append(layer.visible);
    const QString targetGroup   = m_layers[_i].groupId;
    const bool    targetIsGroup = m_layers[_i].kind == QStringLiteral("group");
    const QString targetGroupId = targetIsGroup ? m_layers[_i].groupId : QString{};
    edit(tr("Isoler le calque"), [&] {
        for (int index = 0; index < m_layers.size(); ++index) {
            Layer &layer = m_layers[index];
            if (targetIsGroup)
                layer.visible = index == _i || (layer.groupId == targetGroupId &&
                                                layer.kind != QStringLiteral("group") &&
                                                previousVisibility[index]);
            else
                layer.visible =
                    index == _i || (layer.kind == QStringLiteral("group") &&
                                    !targetGroup.isEmpty() && layer.groupId == targetGroup);
        }
    });
    m_visibilityBeforeIsolation = previousVisibility;
    m_isolatedLayer             = _i;
}
void Document::setLayerLocked(int _i, bool _locked)
{
    if (_i < 0 || _i >= m_layers.size() || m_layers[_i].locked == _locked)
        return;
    edit(tr("Verrouillage du calque"), [&] { m_layers[_i].locked = _locked; });
}
void Document::setLayerOpacity(int _i, qreal _opacity)
{
    if (_i < 0 || _i >= m_layers.size() || m_layers[_i].kind == QStringLiteral("group") ||
        !std::isfinite(_opacity))
        return;
    _opacity = qBound(0., _opacity, 1.);
    if (qFuzzyCompare(m_layers[_i].opacity, _opacity))
        return;
    edit(tr("Opacité du calque"), [&] { m_layers[_i].opacity = _opacity; });
}
void Document::setLayerBlend(int _i, int _blend)
{
    if (_i < 0 || _i >= m_layers.size() || m_layers[_i].kind == QStringLiteral("group") ||
        _blend < 0 || _blend > 26 || m_layers[_i].blend == _blend)
        return;
    edit(tr("Mode de fusion"), [&] { m_layers[_i].blend = _blend; });
}
void Document::previewLayerBlend(int _i, int _blend)
{
    if (_i < 0 || _i >= m_layers.size() || m_layers[_i].kind == QStringLiteral("group") ||
        _blend < 0 || _blend > 26)
        return;
    if (!m_previewingLayer)
        beginLayerPreview();
    if (m_layers[_i].blend == _blend)
        return;
    m_layers[_i].blend    = _blend;
    m_layerPreviewChanged = true;
    m_fullCompositeDirty  = true;
    m_cacheDirty          = true;
    requestPreviewRender(true);
}
void Document::setClippingMask(bool _enabled)
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind == QStringLiteral("group") ||
        m_layers[m_active].clippingMask == _enabled || (_enabled && m_active == 0))
        return;
    edit(_enabled ? tr("Créer un masque d’écrêtage") : tr("Libérer le masque d’écrêtage"),
         [&] { m_layers[m_active].clippingMask = _enabled; });
}
void Document::addLayerMask(bool _revealAll)
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind == QStringLiteral("group"))
        return;
    if (!m_layers[m_active].mask.isNull()) {
        m_editingMask = true;
        emit layersChanged();
        return;
    }
    edit(tr("Ajouter un masque de fusion"), [&] {
        QImage mask(m_size, QImage::Format_Grayscale8);
        mask.fill(hasSelection() ? (_revealAll ? 0 : 255) : (_revealAll ? 255 : 0));
        if (hasSelection()) {
            QPainter painter(&mask);
            painter.setClipRegion(m_selectionRegion);
            painter.fillRect(QRect(QPoint{}, m_size), _revealAll ? Qt::white : Qt::black);
        }
        m_layers[m_active].mask        = mask;
        m_layers[m_active].maskEnabled = true;
        m_layers[m_active].parameters.insert("_maskDensity", 100.);
        m_layers[m_active].parameters.insert("_maskFeather", 0.);
        m_editingMask = true;
    });
}
void Document::copyLayerMask(int _sourceIndex, int _targetIndex)
{
    if (_sourceIndex < 0 || _sourceIndex >= m_layers.size() || _targetIndex < 0 ||
        _targetIndex >= m_layers.size() || _sourceIndex == _targetIndex ||
        m_layers[_sourceIndex].mask.isNull() ||
        m_layers[_targetIndex].kind == QStringLiteral("group") || m_layers[_targetIndex].locked)
        return;
    edit(tr("Copier le masque de fusion"), [&] {
        const auto &source                = m_layers[_sourceIndex];
        auto       &target                = m_layers[_targetIndex];
        target.mask                       = source.mask;
        target.maskEnabled                = source.maskEnabled;
        target.parameters["_maskDensity"] = source.parameters.value("_maskDensity", 100.);
        target.parameters["_maskFeather"] = source.parameters.value("_maskFeather", 0.);
        m_active                          = _targetIndex;
        m_editingMask                     = true;
        m_maskOnlyView                    = false;
        selectOnly(m_active);
    });
}
void Document::removeLayerMask()
{
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].mask.isNull())
        return;
    edit(tr("Supprimer le masque de fusion"), [&] {
        m_layers[m_active].mask = {};
        m_editingMask           = false;
    });
}
void Document::invertLayerMask()
{
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].mask.isNull())
        return;
    edit(tr("Inverser le masque de fusion"), [&] { m_layers[m_active].mask.invertPixels(); });
}
void Document::setMaskEnabled(bool _enabled)
{
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].mask.isNull() ||
        m_layers[m_active].maskEnabled == _enabled)
        return;
    edit(tr("Activer le masque de fusion"), [&] { m_layers[m_active].maskEnabled = _enabled; });
}
void Document::setEditingMask(bool _enabled)
{
    if (m_active < 0 || m_active >= m_layers.size())
        return;
    _enabled = _enabled && !m_layers[m_active].mask.isNull();
    if (m_editingMask == _enabled)
        return;
    endStroke();
    m_editingMask = _enabled;
    emit layersChanged();
}
void Document::createDemo()
{
    newDocument(1440, 1000);
    auto &background = m_layers[0];
    background.name  = tr("Fond · bleu nuit");
    QPainter        bg(&background.image);
    QLinearGradient gradient(0, 0, 1440, 1000);
    gradient.setColorAt(0, QColor("#0e2632"));
    gradient.setColorAt(1, QColor("#30424c"));
    bg.fillRect(background.image.rect(), gradient);
    bg.end();
    QImage   orb = ImageUtils::blank(m_size, workingFormat());
    QPainter p(&orb);
    p.setRenderHint(QPainter::Antialiasing);
    QRadialGradient sphere(QPointF(1010, 405), 350, QPointF(910, 270));
    sphere.setColorAt(0, QColor("#edffe0"));
    sphere.setColorAt(.36, QColor("#b3ecaf"));
    sphere.setColorAt(.7, QColor("#70bda5"));
    sphere.setColorAt(1, QColor("#28595c"));
    p.setBrush(sphere);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(1010, 405), 330, 330);
    p.setPen(QPen(QColor(213, 255, 224, 75), 1.5));
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 8; ++i)
        p.drawEllipse(QPointF(1010, 405), 350 + i * 17, 350 + i * 17);
    p.end();
    m_layers.append({tr("Sphère · lumière"), orb});
    QImage   typography = ImageUtils::blank(m_size, workingFormat());
    QPainter t(&typography);
    t.setRenderHint(QPainter::TextAntialiasing);
    QFont font("sans-serif");
    font.setPixelSize(19);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 5);
    t.setFont(font);
    t.setPen(QColor("#bee8ce"));
    t.drawText(QPointF(88, 106), QStringLiteral("PHOTOGRAPHIE  /  DESIGN  /  EXPLORATION"));
    font.setPixelSize(146);
    font.setLetterSpacing(QFont::AbsoluteSpacing, -6);
    font.setWeight(QFont::Bold);
    t.setFont(font);
    t.setPen(QColor("#f2f2e7"));
    t.drawText(QPointF(78, 390), QStringLiteral("Make room"));
    t.drawText(QPointF(78, 552), QStringLiteral("for wonder."));
    font.setPixelSize(25);
    font.setWeight(QFont::Normal);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0);
    t.setFont(font);
    t.setPen(QColor("#afc4c8"));
    t.drawText(QPointF(88, 646), tr("Une idée. Quelques calques. Des possibilités infinies."));
    t.setPen(QPen(QColor("#91b8a7"), 2));
    t.drawLine(QPointF(88, 856), QPointF(1352, 856));
    font.setPixelSize(19);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    t.setFont(font);
    t.drawText(QPointF(88, 912), QStringLiteral("STUDIO EXPÉRIMENTAL"));
    t.drawText(QPointF(1162, 912), QStringLiteral("VOL. 001"));
    t.end();
    m_layers.append({tr("Composition · typographie"), typography});
    m_layers.append({tr("Vos retouches"), ImageUtils::blank(m_size, workingFormat())});
    m_active = 3;
    selectOnly(m_active);
    m_name = tr("Exploration créative");
    resetHistory(false);
}
