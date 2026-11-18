#include "Document.h"
#include "ImageUtils.h"
#include "text/TextLayer.h"
#include <QPainter>
#include <QTextCursor>
#include <cmath>

namespace
{
QVariantList pointsForQml(const QPolygonF &polygon)
{
    QVariantList points;
    points.reserve(polygon.size());
    for (const auto &point : polygon)
        points.append(QVariant::fromValue(point));
    return points;
}
} // namespace

QVariantMap &Document::textParameters()
{
    return m_textMaskEdit ? m_maskTextParameters : m_layers[m_active].parameters;
}
const QVariantMap &Document::textParameters() const
{
    return m_textMaskEdit ? m_maskTextParameters : m_layers[m_active].parameters;
}
const PhotoCrap::Text::SurfaceLayout &Document::surfaceTextLayout(int index) const
{
    if (!m_surfaceTextLayout || index != m_surfaceLayoutLayer ||
        m_revision != m_surfaceLayoutRevision) {
        m_surfaceTextLayout =
            std::make_shared<PhotoCrap::Text::SurfaceLayout>(m_layers[index], m_size);
        m_surfaceLayoutLayer    = index;
        m_surfaceLayoutRevision = m_revision;
    }
    return *m_surfaceTextLayout;
}

QVariantMap Document::textEditState() const
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        (!m_textMaskEdit && m_layers[m_active].kind != "text"))
        return {};
    auto       state    = textParameters();
    const auto &layer = m_layers[m_active];
    const auto *surface = !m_textMaskEdit && !layer.surfaceNormals.isNull()
                            ? &surfaceTextLayout(m_active) : nullptr;
    const auto bounds = surface ? surface->extent : PhotoCrap::Text::size(state);
    state["viewWidth"]  = bounds.width();
    state["viewHeight"] = bounds.height();
    if (surface) {
        const auto values = [](const QTransform &matrix) {
            return QVariantList{matrix.m11(), matrix.m12(), matrix.m13(),
                                matrix.m21(), matrix.m22(), matrix.m23(),
                                matrix.m31(), matrix.m32(), matrix.m33()};
        };
        const auto &faces = surface->faces;
        if (faces) {
            const auto   &edge      = faces->edge;
            const auto    direction = (edge.p2() - edge.p1()) / edge.length();
            const QPointF n(-direction.y(), direction.x());
            state["surfaceEdge"]   = QVariantList{n.x(), n.y(), -QPointF::dotProduct(n, edge.p1())};
            state["surfaceAcross"] = values(faces->across);
            state["surfaceSecondInverse"] = values(faces->face(true).inverted());
        }
        state["transform"]       = values(surface->matrix);
        state["surfaceText"]     = true;
        state["surfaceRelief"]   = state.value("surfaceRelief", true);
        state["surfaceAngle"]    = state.value("surfaceAngle", 0.);
        state["surfaceStrength"] = state.value("surfaceStrength", 1.);
        QPointF    corner;
        const auto outline = surface->outline(QRectF(QPointF{}, bounds.expandedTo({32., 32.})), &corner);
        if (!outline.isEmpty()) {
            state["surfaceOutline"] = pointsForQml(outline);
            state["surfaceCorner"]  = corner;
        }
    }
    return state;
}
QPointF Document::surfaceTextLocalPoint(const QPointF &point) const
{
    if (m_textMaskEdit || m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind != "text" || m_layers[m_active].surfaceNormals.isNull())
        return {-1., -1.};
    return surfaceTextLayout(m_active).unmap(point);
}

QVariantList Document::surfaceTextRect(const QRectF &rect) const
{
    if (m_active < 0 || m_active >= m_layers.size() || m_textMaskEdit ||
        m_layers[m_active].kind != "text" || m_layers[m_active].surfaceNormals.isNull() || rect.isEmpty())
        return {};
    return pointsForQml(surfaceTextLayout(m_active).outline(rect));
}

PixelPlane Document::rasterTextEdit(const QVariantMap &parameters, QRect &bounds) const
{
    const auto &layer = m_layers[m_active];
    auto        image = PhotoCrap::Text::renderPlane(
        parameters, m_size, m_textMaskEdit ? QImage::Format_ARGB32_Premultiplied : workingFormat(),
        m_textMaskEdit ? nullptr : &layer);
    if (image.isNull())
        return {};
    if (m_textMaskEdit) {
        auto next = m_beforeText.layers[m_active].mask;
        bounds    = PhotoCrap::Text::bounds(parameters, m_size);
        if (hasSelection())
            bounds = bounds.intersected(m_selectionRegion.boundingRect());
        if (!bounds.isEmpty()) {
            const auto before =
                next.region(bounds).convertToFormat(QImage::Format_ARGB32_Premultiplied);
            auto       output = before.copy();
            const auto text   = image.region(bounds);
            if (output.isNull() || text.isNull())
                return {};
            QPainter painter(&output);
            if (hasSelection())
                painter.setClipRegion(m_selectionRegion.translated(-bounds.topLeft()));
            painter.setOpacity(m_brushOpacity);
            painter.drawImage(0, 0, text);
            painter.end();
            SelectionCoverage::blend(
                output, before, m_selectionMask.isNull() ? QImage{} : m_selectionMask.copy(bounds),
                output.rect());
            if (!next.writeRegion(bounds.topLeft(), output.convertToFormat(next.format()), m_size))
                return {};
        }
        return next;
    }
    return image;
}

// The live editor and the stored layer keep their original fonts until a choice is accepted.
void Document::previewTextFont(const QString &family, int start, int end)
{
    if (!m_textEditing || m_textGesture || family.isEmpty() || family.size() > 256)
        return;
    auto                      parameters = textParameters();
    PhotoCrap::Text::Document sample;
    PhotoCrap::Text::configure(sample, parameters);
    if (sample.isEmpty())
        return;
    QTextCursor range(&sample);
    if (start == end)
        range.select(QTextCursor::Document);
    else {
        const int last = sample.characterCount() - 1;
        range.setPosition(qBound(0, start, last));
        range.setPosition(qBound(0, end, last), QTextCursor::KeepAnchor);
    }
    QTextCharFormat face;
    face.setFontFamilies({family});
    range.mergeCharFormat(face);
    parameters["html"] = sample.toHtml();
    QRect bounds;
    auto  plane = rasterTextEdit(parameters, bounds);
    if (plane.isNull()) {
        clearTextFontPreview();
        return;
    }
    m_textFontPreview = std::move(plane);
    requestPreviewRender(true, false, false);
}

void Document::clearTextFontPreview()
{
    if (m_textFontPreview.isNull())
        return;
    m_textFontPreview = {};
    ++m_previewRequestGeneration;
    m_previewRenderPending = false;
    m_previewRenderActive.store(false);
    {
        QMutexLocker lock(&m_previewImageMutex);
        m_previewComposite = {};
    }
    ++m_revision;
    emit gpuStackChanged();
    emit imageChanged();
}

bool Document::renderTextEdit()
{
    auto &layer = m_layers[m_active];
    QRect bounds;
    auto  image = rasterTextEdit(textParameters(), bounds);
    if (image.isNull())
        return fail(tr("Mémoire insuffisante pour rendre le texte."));
    if (m_textMaskEdit) {
        layer.mask = std::move(image);
        m_dirtyRegion |= m_maskTextBounds.united(bounds);
        m_maskTextBounds = bounds;
        m_text           = PhotoCrap::Text::plainText(textParameters());
        changed(false);
        emit textGeometryChanged();
        emit settingsChanged();
        return true;
    }
    layer.image = std::move(image);
    m_text      = PhotoCrap::Text::plainText(layer.parameters);
    if (!m_text.trimmed().isEmpty() && layer.parameters.value("autoName", true).toBool())
        layer.name = m_text.simplified().left(64);
    changed(false);
    emit textGeometryChanged();
    emit settingsChanged();
    return true;
}
void Document::beginTextGesture(qreal _x, qreal _y, bool _forceNew)
{
    if (_x < 0 || _y < 0 || _x >= width() || _y >= height())
        return;
    if (m_editingMask) {
        if (!editable())
            return;
        finishLayerPreview();
        acceptLayerPropertiesSession();
        cancelOperation();
        m_beforeText         = snapshot(tr("Texte"));
        m_textMaskEdit       = true;
        m_maskTextBounds     = {};
        m_maskTextParameters = PhotoCrap::Text::create(_x, _y, m_textSize, m_color);
        PhotoCrap::Text::normalize(m_maskTextParameters);
        m_textEditing = m_textGesture = true;
        m_textNewLayer                = false;
        m_textAnchor                  = {_x, _y};
        m_text.clear();
        emit textEditingChanged();
        emit textGeometryChanged();
        return;
    }
    if (!_forceNew)
        for (int i = int(m_layers.size()) - 1; i >= 0; --i) {
            const auto &layer = m_layers[i];
            if (layer.kind != "text" || !layer.visible || layer.locked || layer.opacity <= 0)
                continue;
            bool protectedGroup = false;
            for (const auto &group : m_layers)
                if (group.kind == "group" && group.groupId == layer.groupId &&
                    (group.locked || !group.visible))
                    protectedGroup = true;
            if (protectedGroup)
                continue;
            const auto *surface = layer.surfaceNormals.isNull() ? nullptr : &surfaceTextLayout(i);
            const auto extent = surface ? surface->extent : PhotoCrap::Text::size(layer.parameters);
            const QPointF canvasPoint(_x, _y);
            const auto point = surface ? surface->unmap(canvasPoint)
                                       : PhotoCrap::Text::transform(layer.parameters).inverted().map(canvasPoint);
            if (QRectF(QPointF{}, extent).contains(point)) {
                m_textAnchor = point;
                beginTextEdit(i, true);
                return;
            }
        }
    if (!startToolLayer(tr("Texte")))
        return;
    m_beforeText         = std::move(m_beforeStroke);
    m_strokeCreatedLayer = false;
    auto &layer          = m_layers[m_active];
    layer.kind           = "text";
    layer.name           = tr("Texte");
    layer.parameters     = PhotoCrap::Text::create(_x, _y, m_textSize, m_color);
    PhotoCrap::Text::normalize(layer.parameters);
    m_textEditing  = true;
    m_textGesture  = true;
    m_textNewLayer = true;
    m_textAnchor   = {_x, _y};
    m_text.clear();
    emit textEditingChanged();
    emit textGeometryChanged();
    emit layersChanged();
}
void Document::updateTextGesture(qreal _x, qreal _y)
{
    if (!m_textGesture || (!m_textNewLayer && !m_textMaskEdit) || !std::isfinite(_x) ||
        !std::isfinite(_y))
        return;
    const QRectF rect =
        QRectF(m_textAnchor, QPointF(qBound(-32768., _x, 32768.), qBound(-32768., _y, 32768.)))
            .normalized();
    if (rect.width() < 4 || rect.height() < 4)
        return;
    auto &p        = textParameters();
    p["paragraph"] = true;
    p["width"]     = qMin(32768., rect.width());
    p["height"]    = qMin(32768., rect.height());
    p["transform"] =
        PhotoCrap::Text::create(rect.x(), rect.y(), m_textSize, m_color).value("transform");
    emit textGeometryChanged();
}
void Document::editTextLayer(int _index)
{
    beginTextEdit(_index, false);
}
void Document::beginTextEdit(int _index, bool _fromCanvas)
{
    _index = finishTextForLayer(_index);
    endStroke();
    if (_index < 0 || _index >= m_layers.size() || m_layers[_index].kind != "text")
        return;
    const auto &layer = m_layers[_index];
    if (layer.locked) {
        fail(tr("Ce calque est verrouillé."));
        return;
    }
    for (const auto &group : m_layers)
        if (group.kind == "group" && group.groupId == layer.groupId &&
            (group.locked || !group.visible)) {
            fail(tr("Ce groupe est verrouillé ou masqué."));
            return;
        }
    finishLayerPreview();
    acceptLayerPropertiesSession();
    setActiveLayer(_index);
    setTool("text");
    m_beforeText   = snapshot(tr("Modifier le texte"));
    m_textEditing  = true;
    m_textGesture  = false;
    m_textNewLayer = false;
    m_text         = PhotoCrap::Text::plainText(m_layers[m_active].parameters);
    if (!m_layers[m_active].surfaceNormals.isNull() &&
        !m_layers[m_active].parameters.contains("surfaceFocal"))
        setSurfaceTextParameter(QStringLiteral("surfaceFocal"), .85);
    m_textGesture = _fromCanvas;
    showCompositeView();
    emit textEditingChanged();
    emit textGeometryChanged();
    if (!_fromCanvas)
        emit textEditRequested();
}
void Document::updateTextHtml(const QString &_html)
{
    updateTextContent(_html, {});
}
void Document::updateTextContent(const QString &_html, const QVariantMap &_formatting)
{
    if (!m_textEditing || m_textGesture)
        return;
    auto parameters      = textParameters();
    parameters["html"]   = _html;
    parameters["runs"]   = _formatting.value("runs");
    parameters["blocks"] = _formatting.value("blocks");
    if (!PhotoCrap::Text::normalize(parameters)) {
        fail(tr("Texte invalide ou trop volumineux."));
        return;
    }
    if (parameters == textParameters())
        return;
    const auto before = textParameters();
    const auto *surface = m_textMaskEdit ? nullptr : &m_layers[m_active];
    m_dirtyRegion |= PhotoCrap::Text::bounds(before, m_size, surface) |
                     PhotoCrap::Text::bounds(parameters, m_size, surface);
    textParameters() = parameters;
    if (!renderTextEdit())
        textParameters() = before;
}
void Document::setTextBox(bool _paragraph, qreal _width, qreal _height)
{
    if (!m_textEditing)
        return;
    auto p         = textParameters();
    p["paragraph"] = _paragraph;
    p["width"]     = _width;
    p["height"]    = _height;
    if (!PhotoCrap::Text::normalize(p)) {
        fail(tr("Dimensions du bloc de texte invalides."));
        return;
    }
    const auto *surface = m_textMaskEdit ? nullptr : &m_layers[m_active];
    m_dirtyRegion |= PhotoCrap::Text::bounds(textParameters(), m_size, surface) |
                     PhotoCrap::Text::bounds(p, m_size, surface);
    textParameters() = p;
    renderTextEdit();
}
void Document::moveTextBy(qreal _dx, qreal _dy)
{
    if (!m_textEditing || m_textGesture || !std::isfinite(_dx) || !std::isfinite(_dy) ||
        qAbs(_dx) > 32768 || qAbs(_dy) > 32768 || (_dx == 0 && _dy == 0))
        return;
    const auto before     = textParameters();
    auto       parameters = before;
    PhotoCrap::Text::transform(parameters, QTransform::fromTranslate(_dx, _dy));
    if (!PhotoCrap::Text::normalize(parameters))
        return;
    const auto *surface = m_textMaskEdit ? nullptr : &m_layers[m_active];
    m_dirtyRegion |= PhotoCrap::Text::bounds(before, m_size, surface) |
                     PhotoCrap::Text::bounds(parameters, m_size, surface);
    textParameters() = parameters;
    if (!renderTextEdit())
        textParameters() = before;
}
void Document::setSurfaceTextAngle(qreal angle)
{
    if (std::isfinite(angle))
        setSurfaceTextParameter(QStringLiteral("surfaceAngle"), qBound(-180., angle, 180.));
}
void Document::setSurfaceTextRelief(bool enabled)
{
    setSurfaceTextParameter(QStringLiteral("surfaceRelief"), enabled);
}
void Document::setSurfaceTextStrength(qreal strength)
{
    if (std::isfinite(strength))
        setSurfaceTextParameter(QStringLiteral("surfaceStrength"), qBound(0., strength, 3.));
}
void Document::setSurfaceTextParameter(const QString &key, const QVariant &value)
{
    if (!m_textEditing || m_textGesture || m_textMaskEdit ||
        m_layers[m_active].surfaceNormals.isNull())
        return;
    auto &layer  = m_layers[m_active];
    auto  before = layer.parameters;
    if (before.value(key) == value)
        return;
    layer.parameters[key] = value;
    m_dirtyRegion |= PhotoCrap::Text::bounds(before, m_size, &layer) |
                     PhotoCrap::Text::bounds(layer.parameters, m_size, &layer);
    if (!renderTextEdit())
        layer.parameters = std::move(before);
}
void Document::finishTextEdit(bool _cancel)
{
    if (!m_textEditing)
        return;
    clearTextFontPreview();
    m_textGesture      = false;
    const auto  before = m_beforeText;
    const auto &layer  = m_layers[m_active];
    const bool  empty  = PhotoCrap::Text::plainText(textParameters()).trimmed().isEmpty();
    const bool  changedText =
        m_textMaskEdit ? layer.mask.cacheKey() != before.layers[m_active].mask.cacheKey()
                        : m_textNewLayer || layer.parameters != before.layers[m_active].parameters;
    m_textEditing = false;
    m_beforeText  = {};
    const bool maskEdit = m_textMaskEdit;
    m_textMaskEdit      = false;
    m_maskTextParameters.clear();
    m_maskTextBounds = {};
    if (_cancel || ((m_textNewLayer || maskEdit) && empty))
        restore(before);
    else if (changedText)
        commit(before, tr("Texte"));
    emit textEditingChanged();
    emit textGeometryChanged();
}
void Document::rasterizeTextLayer()
{
    finishTextEdit();
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].kind != "text" ||
        m_layers[m_active].locked)
        return;
    edit(tr("Pixelliser le texte"), [&] {
        auto      &layer       = m_layers[m_active];
        const auto maskDensity = layer.parameters.value("_maskDensity", 100.);
        const auto maskFeather = layer.parameters.value("_maskFeather", 0.);
        layer.kind             = "pixel";
        auto parameters = QVariantMap{{"_maskDensity", maskDensity}, {"_maskFeather", maskFeather}};
        for (auto it = layer.parameters.cbegin(); it != layer.parameters.cend(); ++it)
            if (it.key().startsWith("_vector"))
                parameters[it.key()] = it.value();
        layer.parameters = parameters;
    });
}

void Document::updateTextFormatting(const QVariantMap &_formatting)
{
    if (!m_textEditing || m_textGesture)
        return;
    auto p      = textParameters();
    p["runs"]   = _formatting.value("runs");
    p["blocks"] = _formatting.value("blocks");
    if (!PhotoCrap::Text::normalize(p)) {
        fail(tr("Mise en forme du texte invalide."));
        return;
    }
    if (p == textParameters())
        return;
    const auto *surface = m_textMaskEdit ? nullptr : &m_layers[m_active];
    m_dirtyRegion |= PhotoCrap::Text::bounds(textParameters(), m_size, surface) |
                     PhotoCrap::Text::bounds(p, m_size, surface);
    textParameters() = p;
    renderTextEdit();
}

int Document::finishTextForLayer(int _index)
{
    if (_index < 0 || _index >= m_layers.size())
        return -1;
    if (!m_textEditing)
        return _index;
    const QString id = m_layers[_index].historyId;
    finishTextEdit();
    for (int i = 0; i < m_layers.size(); ++i)
        if (m_layers[i].historyId == id)
            return i;
    return -1;
}
