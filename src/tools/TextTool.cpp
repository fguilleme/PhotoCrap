#include "Document.h"
#include "ImageUtils.h"
#include "text/TextLayer.h"
#include <QTextCursor>
#include <cmath>

QVariantMap Document::textEditState() const
{
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].kind != "text")
        return {};
    auto       state    = m_layers[m_active].parameters;
    const auto bounds   = PhotoCrap::Text::size(state);
    state["viewWidth"]  = bounds.width();
    state["viewHeight"] = bounds.height();
    return state;
}
bool Document::renderTextEdit()
{
    auto &layer = m_layers[m_active];
    auto  image = PhotoCrap::Text::renderPlane(layer.parameters, m_size, workingFormat());
    if (image.isNull())
        return fail(tr("Mémoire insuffisante pour rendre le texte."));
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
            const auto point =
                PhotoCrap::Text::transform(layer.parameters).inverted().map(QPointF(_x, _y));
            if (QRectF(QPointF{}, PhotoCrap::Text::size(layer.parameters)).contains(point)) {
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
    if (!m_textGesture || !m_textNewLayer || !std::isfinite(_x) || !std::isfinite(_y))
        return;
    const QRectF rect =
        QRectF(m_textAnchor, QPointF(qBound(-32768., _x, 32768.), qBound(-32768., _y, 32768.)))
            .normalized();
    if (rect.width() < 4 || rect.height() < 4)
        return;
    auto &p        = m_layers[m_active].parameters;
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
    m_textGesture  = _fromCanvas;
    m_textNewLayer = false;
    m_text         = PhotoCrap::Text::plainText(m_layers[m_active].parameters);
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
    auto parameters      = m_layers[m_active].parameters;
    parameters["html"]   = _html;
    parameters["runs"]   = _formatting.value("runs");
    parameters["blocks"] = _formatting.value("blocks");
    if (!PhotoCrap::Text::normalize(parameters)) {
        fail(tr("Texte invalide ou trop volumineux."));
        return;
    }
    if (parameters == m_layers[m_active].parameters)
        return;
    const auto before = m_layers[m_active];
    m_dirtyRegion     = m_dirtyRegion.united(PhotoCrap::Text::bounds(before.parameters, m_size))
                            .united(PhotoCrap::Text::bounds(parameters, m_size));
    m_layers[m_active].parameters = parameters;
    if (!renderTextEdit())
        m_layers[m_active] = before;
}
void Document::setTextBox(bool _paragraph, qreal _width, qreal _height)
{
    if (!m_textEditing)
        return;
    auto p         = m_layers[m_active].parameters;
    p["paragraph"] = _paragraph;
    p["width"]     = _width;
    p["height"]    = _height;
    if (!PhotoCrap::Text::normalize(p)) {
        fail(tr("Dimensions du bloc de texte invalides."));
        return;
    }
    m_dirtyRegion =
        m_dirtyRegion.united(PhotoCrap::Text::bounds(m_layers[m_active].parameters, m_size))
            .united(PhotoCrap::Text::bounds(p, m_size));
    m_layers[m_active].parameters = p;
    renderTextEdit();
}
void Document::moveTextBy(qreal _dx, qreal _dy)
{
    if (!m_textEditing || m_textGesture || !std::isfinite(_dx) || !std::isfinite(_dy) ||
        qAbs(_dx) > 32768 || qAbs(_dy) > 32768 || (_dx == 0 && _dy == 0))
        return;
    const auto before     = m_layers[m_active];
    auto       parameters = before.parameters;
    PhotoCrap::Text::transform(parameters, QTransform::fromTranslate(_dx, _dy));
    if (!PhotoCrap::Text::normalize(parameters))
        return;
    m_dirtyRegion = m_dirtyRegion.united(PhotoCrap::Text::bounds(before.parameters, m_size))
                        .united(PhotoCrap::Text::bounds(parameters, m_size));
    m_layers[m_active].parameters = parameters;
    if (!renderTextEdit())
        m_layers[m_active] = before;
}
void Document::finishTextEdit(bool _cancel)
{
    if (!m_textEditing)
        return;
    m_textGesture      = false;
    const auto  before = m_beforeText;
    const auto &layer  = m_layers[m_active];
    const bool  empty  = PhotoCrap::Text::plainText(layer.parameters).trimmed().isEmpty();
    const bool  changedText =
        m_textNewLayer || layer.parameters != before.layers[m_active].parameters;
    m_textEditing = false;
    m_beforeText  = {};
    if (_cancel || (m_textNewLayer && empty))
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
        layer.parameters       = {{"_maskDensity", maskDensity}, {"_maskFeather", maskFeather}};
    });
}

void Document::updateTextFormatting(const QVariantMap &_formatting)
{
    if (!m_textEditing || m_textGesture)
        return;
    auto p      = m_layers[m_active].parameters;
    p["runs"]   = _formatting.value("runs");
    p["blocks"] = _formatting.value("blocks");
    if (!PhotoCrap::Text::normalize(p)) {
        fail(tr("Mise en forme du texte invalide."));
        return;
    }
    if (p == m_layers[m_active].parameters)
        return;
    m_dirtyRegion =
        m_dirtyRegion.united(PhotoCrap::Text::bounds(m_layers[m_active].parameters, m_size))
            .united(PhotoCrap::Text::bounds(p, m_size));
    m_layers[m_active].parameters = p;
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
