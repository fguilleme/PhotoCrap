#include "Document.h"
#include "shapes/ShapeLayer.h"
#include <QPainter>
#include <QScopedValueRollback>
#include <cmath>

bool Document::renderMaskPenShape(const QVariantMap &_style)
{
    if (!m_editingMask || m_activePenPath < 0 || m_activePenPath >= m_penPaths.size() ||
        !editable())
        return false;
    if (m_maskPenBase.isNull() || m_maskPenLayerId != m_layers[m_active].historyId ||
        m_maskPenPathIndex != m_activePenPath) {
        m_maskPenBase      = toolPlane();
        m_maskPenLayerId   = m_layers[m_active].historyId;
        m_maskPenPathIndex = m_activePenPath;
        m_maskPenBounds    = {};
    }
    auto parameters = _style;
    PhotoCrap::Shape::setPath(parameters, m_penPaths[m_activePenPath]);
    const auto image =
        PhotoCrap::Shape::renderPlane(parameters, m_size, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return fail(tr("Mémoire insuffisante pour rendre la forme."));
    auto next   = m_maskPenBase;
    auto bounds = PhotoCrap::Shape::bounds(parameters, m_size);
    if (hasSelection())
        bounds = bounds.intersected(m_selectionRegion.boundingRect());
    if (!bounds.isEmpty()) {
        const auto before =
            next.region(bounds).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        auto       output = before.copy();
        const auto pixels = image.region(bounds);
        if (output.isNull() || pixels.isNull())
            return false;
        QPainter painter(&output);
        if (hasSelection())
            painter.setClipRegion(m_selectionRegion.translated(-bounds.topLeft()));
        painter.setOpacity(m_brushOpacity);
        painter.drawImage(0, 0, pixels);
        painter.end();
        SelectionCoverage::blend(output, before,
                                 m_selectionMask.isNull() ? QImage{} : m_selectionMask.copy(bounds),
                                 output.rect());
        if (!next.writeRegion(bounds.topLeft(), output.convertToFormat(next.format()), m_size))
            return false;
    }
    toolPlane() = std::move(next);
    m_dirtyRegion |= bounds.united(m_maskPenBounds);
    m_maskPenBounds = bounds;
    changed(false);
    return true;
}

int Document::penShapeLayerIndex() const
{
    if (m_editingMask)
        return -1;
    if (m_activePenPath < 0 || m_activePenPath >= m_penPaths.size())
        return -1;
    const auto id = m_penPaths[m_activePenPath].shapeLayerId;
    if (id.isEmpty())
        return -1;
    for (int i = 0; i < m_layers.size(); ++i)
        if (m_layers[i].kind == "shape" && m_layers[i].historyId == id)
            return i;
    return -1;
}
bool Document::penShapeEditable(int _layer)
{
    if (_layer < 0 || _layer >= m_layers.size())
        return false;
    if (m_layers[_layer].locked)
        return fail(tr("Ce calque est verrouillé."));
    const auto groupId = m_layers[_layer].groupId;
    for (const auto &group : m_layers)
        if (!groupId.isEmpty() && group.kind == "group" && group.groupId == groupId &&
            (group.locked || !group.visible))
            return fail(group.locked ? tr("Ce groupe est verrouillé.")
                                     : tr("Ce groupe est masqué."));
    return true;
}
void Document::setPenMode(const QString &_mode)
{
    if ((_mode != "path" && _mode != "shape") || m_penMode == _mode)
        return;
    finishPenPath();
    setActivePenPath(-1);
    m_penMode = _mode;
    emit penOptionsChanged();
}
QVariantMap Document::penShapeStyle() const
{
    auto style = m_penShapeDefaults.isEmpty() ? PhotoCrap::Shape::defaults() : m_penShapeDefaults;
    if (m_penShapeDefaults.isEmpty())
        style["fillColor"] = m_color.name(QColor::HexArgb);
    if (m_active >= 0 && m_active < m_layers.size() && m_layers[m_active].kind == "shape")
        for (auto it = style.begin(); it != style.end(); ++it)
            it.value() = m_layers[m_active].parameters.value(it.key(), it.value());
    return style;
}
void Document::setPenShapeStyle(const QString &_key, const QVariant &_value)
{
    const auto defaults = PhotoCrap::Shape::defaults();
    if (!defaults.contains(_key))
        return;
    endStroke();
    auto style = penShapeStyle();
    if (style.value(_key) == _value &&
        (_key != "strokeColor" || style.value("strokeEnabled").toBool()) &&
        (_key != "fillColor" || style.value("fillEnabled").toBool()))
        return;
    style[_key] = _value;
    // Picking a solid colour selects that paint, just like the colour swatch
    // in the options bar. "None" is an explicit, separate choice.
    if (_key == "strokeColor")
        style["strokeEnabled"] = true;
    else if (_key == "fillColor")
        style["fillEnabled"] = true;
    // Validate styles with a finite, harmless path, including at creation time.
    auto probe = style;
    PhotoCrap::Shape::setPath(probe, PenPath{"Style", {{{0, 0}, {0, 0}, {0, 0}, false}}, false});
    if (!PhotoCrap::Shape::normalize(probe))
        return;
    for (auto it = style.begin(); it != style.end(); ++it)
        it.value() = probe.value(it.key());
    if (m_penMode == "shape" && m_editingMask && m_activePenPath >= 0 &&
        !m_penPaths[m_activePenPath].points.isEmpty()) {
        const auto before = snapshot({});
        if (!renderMaskPenShape(style))
            return;
        commit(before, tr("Modifier le style de la forme"), false);
    } else if (m_penMode == "shape" && !m_editingMask && m_active >= 0 &&
               m_layers[m_active].kind == "shape") {
        if (!penShapeEditable(m_active)) {
            emit penOptionsChanged();
            return;
        }
        const auto before     = snapshot({});
        auto       parameters = m_layers[m_active].parameters;
        for (auto it = style.cbegin(); it != style.cend(); ++it)
            parameters[it.key()] = it.value();
        if (parameters == m_layers[m_active].parameters)
            return;
        const auto image = PhotoCrap::Shape::renderPlane(parameters, m_size, workingFormat());
        if (image.isNull()) {
            fail(tr("Mémoire insuffisante pour rendre la forme."));
            return;
        }
        m_dirtyRegion =
            m_dirtyRegion.united(PhotoCrap::Shape::bounds(m_layers[m_active].parameters, m_size))
                .united(PhotoCrap::Shape::bounds(parameters, m_size));
        m_layers[m_active].parameters = parameters;
        m_layers[m_active].image      = image;
        commit(before, tr("Modifier le style de la forme"), false);
        ++m_thumbnailRevision;
        emit thumbnailsChanged();
    }
    m_penShapeDefaults = style;
    emit penOptionsChanged();
}
bool Document::penPathToShape()
{
    endStroke();
    finishPenPath();
    if (m_activePenPath < 0 || m_activePenPath >= m_penPaths.size() ||
        !m_penPaths[m_activePenPath].shapeLayerId.isEmpty() ||
        m_penPaths[m_activePenPath].points.size() < 2)
        return false;
    const auto source = m_penPaths[m_activePenPath];
    if (m_editingMask) {
        const auto before = snapshot({});
        m_maskPenBase     = {};
        if (!renderMaskPenShape(penShapeStyle()))
            return false;
        commit(before, tr("Créer une forme depuis le tracé"), false);
        return true;
    }
    if (m_penPaths.size() >= PenGeometry::MaxPaths)
        return fail(tr("Limite de tracés atteinte."));
    auto parameters = penShapeStyle();
    PhotoCrap::Shape::setPath(parameters, source);
    const auto image = PhotoCrap::Shape::renderPlane(parameters, m_size, workingFormat());
    if (image.isNull())
        return fail(tr("Mémoire insuffisante pour rendre la forme."));
    finishLayerPreview(tr("Modifier les propriétés du calque"));
    acceptLayerPropertiesSession();
    const auto before = snapshot({});
    if (!startToolLayer(tr("Forme %1").arg(m_layers.size())))
        return false;
    m_strokeCreatedLayer = false;
    m_beforeStroke       = {};
    auto &layer          = m_layers[m_active];
    layer.kind           = "shape";
    layer.name           = source.name;
    layer.parameters     = parameters;
    layer.image          = image;
    // Keep the saved source intact; the new layer owns an independent,
    // editable vector mask. Both are restored atomically by Undo.
    syncPenShapePath(true);
    commit(before, tr("Créer une forme depuis le tracé"));
    return true;
}
QRectF Document::penShapeBounds() const
{
    const auto index = penShapeLayerIndex();
    return index >= 0
               ? PhotoCrap::Shape::path(m_layers[index].parameters).painterPath().boundingRect()
               : QRectF{};
}
void Document::resizePenShape(qreal _width, qreal _height)
{
    endPenGesture();
    const auto index  = penShapeLayerIndex();
    const auto bounds = penShapeBounds();
    if (index < 0 || !penShapeEditable(index) || bounds.isEmpty() || !std::isfinite(_width) ||
        !std::isfinite(_height) || _width <= 0 || _height <= 0)
        return;
    auto       parameters = m_layers[index].parameters;
    QTransform transform;
    transform.translate(bounds.x(), bounds.y());
    transform.scale(_width / bounds.width(), _height / bounds.height());
    transform.translate(-bounds.x(), -bounds.y());
    if (!PhotoCrap::Shape::transform(parameters, transform) ||
        parameters == m_layers[index].parameters)
        return;
    const auto before           = snapshot({});
    auto       path             = PhotoCrap::Shape::path(parameters);
    path.name                   = m_penPaths[m_activePenPath].name;
    path.shapeLayerId           = m_layers[index].historyId;
    m_penPaths[m_activePenPath] = path;
    commitPenEdit(before, tr("Redimensionner la forme"));
}
bool Document::startPenShapeLayer()
{
    if (m_editingMask) {
        m_penShapeGesture = true;
        return renderMaskPenShape(penShapeStyle());
    }
    const auto style = penShapeStyle();
    if (!startToolLayer(tr("Forme %1").arg(m_layers.size())))
        return false;
    m_strokeCreatedLayer = false;
    m_beforeStroke       = {};
    auto &layer          = m_layers[m_active];
    layer.kind           = "shape";
    layer.parameters     = style;
    auto &path           = m_penPaths[m_activePenPath];
    path.shapeLayerId    = layer.historyId;
    path.name            = layer.name;
    PhotoCrap::Shape::setPath(layer.parameters, path);
    m_penShapeGesture = true;
    emit layersChanged();
    emit penOptionsChanged();
    return updatePenShapeLayer();
}
bool Document::updatePenShapeLayer()
{
    if (m_editingMask && m_penMode == "shape")
        return renderMaskPenShape(penShapeStyle());
    const int index = penShapeLayerIndex();
    if (index < 0)
        return true;
    auto &layer      = m_layers[index];
    auto  parameters = layer.parameters;
    PhotoCrap::Shape::setPath(parameters, m_penPaths[m_activePenPath]);
    if (parameters == layer.parameters && layer.image.size() == m_size)
        return true;
    const auto image = PhotoCrap::Shape::renderPlane(parameters, m_size, workingFormat());
    if (image.isNull())
        return fail(tr("Mémoire insuffisante pour rendre la forme."));
    m_dirtyRegion    = m_dirtyRegion.united(PhotoCrap::Shape::bounds(layer.parameters, m_size))
                           .united(PhotoCrap::Shape::bounds(parameters, m_size));
    layer.parameters = parameters;
    layer.image      = image;
    changed(false);
    ++m_thumbnailRevision;
    emit thumbnailsChanged();
    return true;
}
void Document::syncPenShapePath(bool _activate)
{
    if (m_penGesture || m_syncingPenShape)
        return;
    QScopedValueRollback guard(m_syncingPenShape, true);
    const bool           shape   = m_active >= 0 && m_active < m_layers.size() &&
                                   m_layers[m_active].kind == "shape" && !m_editingMask;
    const auto           id      = shape ? m_layers[m_active].historyId : QString{};
    bool                 changed = false;
    for (int i = int(m_penPaths.size()) - 1; i >= 0; --i)
        if (!m_penPaths[i].shapeLayerId.isEmpty() && m_penPaths[i].shapeLayerId != id) {
            m_penPaths.removeAt(i);
            if (m_activePenPath == i) {
                m_activePenPath = -1;
                m_selectedPenPoints.clear();
                m_penDrawing = false;
            } else if (m_activePenPath > i)
                --m_activePenPath;
            changed = true;
        }
    if (shape) {
        auto path         = PhotoCrap::Shape::path(m_layers[m_active].parameters);
        path.name         = m_layers[m_active].name;
        path.shapeLayerId = id;
        int index         = -1;
        for (int i = 0; i < m_penPaths.size(); ++i)
            if (m_penPaths[i].shapeLayerId == id)
                index = i;
        if (index < 0) {
            m_penPaths.append(path);
            index     = int(m_penPaths.size()) - 1;
            _activate = _activate || m_activePenPath < 0;
            changed   = true;
        } else if (m_penPaths[index] != path) {
            m_penPaths[index] = path;
            changed           = true;
        }
        if (_activate) {
            m_activePenPath = index;
            m_selectedPenPoints.clear();
            m_penDrawing = false;
            changed      = true;
        }
        if (m_activePenPath == index)
            m_penMode = "shape";
    }
    if (!m_editingMask && m_activePenPath >= 0 && m_activePenPath < m_penPaths.size())
        m_penMode = m_penPaths[m_activePenPath].shapeLayerId.isEmpty() ? "path" : "shape";
    if (changed)
        emit penPathsChanged();
    emit penOptionsChanged();
}
void Document::editPenShape()
{
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].kind != "shape")
        return;
    finishPenPath();
    setTool("pen");
    syncPenShapePath(true);
}
void Document::rasterizeShapeLayer()
{
    endStroke();
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].kind != "shape" ||
        !penShapeEditable(m_active))
        return;
    edit(tr("Pixelliser la forme"), [&] {
        auto      &layer   = m_layers[m_active];
        const auto density = layer.parameters.value("_maskDensity", 100.);
        const auto feather = layer.parameters.value("_maskFeather", 0.);
        layer.kind         = "pixel";
        layer.parameters   = {{"_maskDensity", density}, {"_maskFeather", feather}};
    });
}
