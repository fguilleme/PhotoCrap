#include "Document.h"
#include "ImageUtils.h"
#include "ai/AiImageTypes.h"
#include "ai/SurfaceGeometry.h"
#include "text/TextLayer.h"
#include <QBuffer>
#include <QImageReader>
#include <QtConcurrentRun>
#include <algorithm>

namespace
{
// Stage the exact same stack for inference and for the final, undoable edit.
bool selectRelightPhoto(QVector<Layer> &layers, int &lightIndex, int photoIndex)
{
    if (photoIndex == -1)
        return true;
    const QString group = layers[lightIndex].groupId;
    const auto isPhoto = [&](int i) {
        return layers[i].kind == "pixel" && !layers[i].clippingMask && layers[i].groupId == group;
    };
    if (photoIndex < 0 || photoIndex >= layers.size() || !isPhoto(photoIndex))
        return false;
    layers[lightIndex].visible = true;
    if (!group.isEmpty()) {
        for (auto &layer : layers)
            if (layer.kind == "group" && layer.groupId == group)
                layer.visible = true;
    }
    int previous = lightIndex - 1;
    while (previous >= 0 && (!isPhoto(previous) || !layers[previous].visible))
        --previous;
    if (previous == photoIndex)
        return true;
    if (previous >= 0)
        layers[previous].visible = false;
    layers[photoIndex].visible = true;

    // Reuse the previous photo's place in the stack so the same adjustments still cover it.
    // If the light is clipped, put the new photo directly beneath it as its clipping base.
    const int slot = previous < 0 || layers[lightIndex].clippingMask ? lightIndex : previous;
    int end = photoIndex + 1;
    while (end < layers.size() && end != lightIndex && layers[end].clippingMask &&
           layers[end].groupId == group)
        ++end;
    if (photoIndex > slot) {
        std::rotate(layers.begin() + slot, layers.begin() + photoIndex, layers.begin() + end);
        if (lightIndex < photoIndex)
            lightIndex += end - photoIndex;
    } else if (end < slot && (previous >= 0 || layers[lightIndex].clippingMask)) {
        std::rotate(layers.begin() + photoIndex, layers.begin() + end, layers.begin() + slot);
    }
    return true;
}
} // namespace

bool Document::adoptAiDocument(const PhotoCrap::Ai::Result &result, const QVariantMap &metadata,
                               const PhotoCrap::Ai::Input &source, QString &error)
{
    const auto size = result.image.size();
    if (!result.error.isEmpty() || result.image.isNull() ||
        !validSize(size.width(), size.height()) || (result.bits != 8 && result.bits != 16) ||
        (!result.mask.isNull() && result.mask.size() != size)) {
        error = result.error.isEmpty() ? tr("Le résultat IA ne peut pas devenir un document.")
                                       : result.error;
        return false;
    }
    Layer layer;
    layer.name              = tr("Résultat IA — %1").arg(metadata.value("provider").toString());
    layer.image             = result.image;
    layer.mask              = result.mask;
    layer.aiSource          = result.original;
    layer.parameters["_ai"] = metadata;
    if (source.originalSource && source.selected) {
        layer.maskEnabled                = source.maskEnabled;
        layer.parameters["_maskDensity"] = source.maskDensity;
        const qreal scale                = qMin(qreal(size.width()) / source.bounds.width(),
                                 qreal(size.height()) / source.bounds.height());
        layer.parameters["_maskFeather"] = source.maskFeather * scale;
    }
    m_size                = size;
    m_channelBits         = result.bits;
    m_layers              = {layer};
    m_active              = 0;
    const auto sourceName = metadata.value("sourceName", metadata.value("provider")).toString();
    m_name       = metadata.contains("upscaleFactor") ? tr("%1 — Upscale").arg(sourceName)
                                                      : tr("%1 — IA").arg(sourceName);
    m_projectUrl = QUrl{};
    resetHistory(false);
    return true;
}

bool Document::aiInsertionBusy() const
{
    return pixelEditBusy() || selectionBusy() || rawLoading() || cropBusy() || sliceBusy() ||
           frequencyRetouchBusy() || m_drawing || m_textEditing || m_penGesture || m_penDrawing ||
           m_cropDragging || m_sliceDragging || gradientDragging() || m_previewingLayer ||
           m_polygonLassoActive;
}

QVariantMap Document::originalAiSource() const
{
    if (m_active < 0 || m_active >= m_layers.size())
        return {};
    const auto &layer = m_layers[m_active];
    if (!layer.aiSource || !layer.aiSource->matches(layer.image, m_size, m_channelBits))
        return {};
    QBuffer bytes;
    bytes.setData(layer.aiSource->encoded);
    bytes.open(QIODevice::ReadOnly);
    QImageReader reader(&bytes);
    reader.setAutoTransform(true);
    auto size = reader.size();
    if (reader.transformation() & QImageIOHandler::TransformationRotate90)
        size.transpose();
    if (!PhotoCrap::Ai::validSize(size))
        return {};
    return {{"width", size.width()}, {"height", size.height()}};
}

QFuture<PhotoCrap::Ai::Input> Document::prepareAiInput(bool _selection, int _maxEdge,
                                                       std::shared_ptr<std::atomic_bool> _cancel,
                                                       bool _original, bool _nativePrecision,
                                                       bool localImage, std::optional<int> relightPhoto)
{
    endStroke();
    finishTextEdit();
    finishLayerPreview();
    acceptLayerPropertiesSession();
    PhotoCrap::Ai::Input input;
    input.session  = m_documentSession;
    input.contentId      = m_contentId;
    input.canvas   = m_size;
    input.selected = _selection;
    input.originalSource = _original;
    input.bounds   = _selection
                         ? m_selectionRegion.boundingRect().intersected(QRect(QPoint{}, m_size))
                         : QRect(QPoint{}, m_size);
    if (m_active >= 0 && m_active < m_layers.size())
        input.layerId = m_layers[m_active].historyId;
    QByteArray original;
    PixelPlane originalMask;
    if (_original) {
        if (_selection || originalAiSource().isEmpty())
            input.error = tr("L’image IA d’origine n’est plus disponible sur ce calque.");
        else {
            const auto &layer = m_layers[m_active];
            original          = layer.aiSource->encoded;
            originalMask      = layer.mask;
            input.bounds      = layer.aiSource->bounds;
            input.selected    = !originalMask.isNull();
            input.maskEnabled = layer.maskEnabled;
            input.maskDensity = layer.parameters.value("_maskDensity", 100.).toDouble();
            input.maskFeather = layer.parameters.value("_maskFeather", 0.).toDouble();
        }
    }
    if (pixelEditBusy() || selectionBusy() || input.bounds.isEmpty() ||
        (_selection && !hasSelection()))
        input.error =
            tr("Terminez les opérations en cours et choisissez une image ou une sélection valide.");
    auto layers = m_layers;
    if (relightPhoto) {
        input.relightPhoto = relightPhoto;
        if (m_active < 0 || m_active >= layers.size() || layers[m_active].adjustmentType != "relight" ||
            layers[m_active].locked || m_editingMask) {
            input.error = tr("Sélectionnez un calque de rééclairage modifiable.");
        } else {
            int lightIndex = m_active;
            if (!selectRelightPhoto(layers, lightIndex, *relightPhoto))
                input.error = tr("Le calque photo choisi est indisponible.");
            layers.resize(lightIndex); // The light itself and everything above it are excluded.
        }
    }
    const auto coverage = m_selectionMask;
    const auto region   = m_selectionRegion;
    const auto scratch  = m_scratchCache;
    const auto bits     = m_channelBits;
    return QtConcurrent::run([input, layers, coverage, region, scratch, bits, _maxEdge, _cancel,
                              original, originalMask, _nativePrecision, localImage]() mutable {
        if (!input.error.isEmpty() || _cancel->load())
            return input;
        Document renderer;
        renderer.m_backgroundWorker   = true;
        renderer.m_historyBrushSource = {};
        renderer.m_layers             = layers;
        renderer.m_size               = input.canvas;
        renderer.m_channelBits        = bits;
        renderer.m_scratchCache       = scratch;
        renderer.m_backgroundCancel   = _cancel;
        QImage image;
        if (original.isEmpty())
            image = renderer.compositeRegion(input.bounds);
        else {
            QBuffer bytes;
            bytes.setData(original);
            bytes.open(QIODevice::ReadOnly);
            QImageReader reader(&bytes);
            reader.setAutoTransform(true);
            image = reader.read();
            if (image.colorSpace().isValid())
                image.convertToColorSpace(QColorSpace::SRgb);
        }
        if (image.isNull()) {
            input.error = tr("Impossible de préparer l’image à envoyer.");
            return input;
        }
        if (input.selected) {
            input.coverage = !original.isEmpty()
                                 ? originalMask.region(QRect(QPoint{}, input.canvas))
                             : coverage.isNull() ? SelectionCoverage::raster(region, input.canvas)
                                                 : coverage;
            if (input.coverage.isNull()) {
                input.error = tr("Impossible de préparer la sélection à envoyer.");
                return input;
            }
            // Clip the submitted image to the selected support, but apply the
            // fractional feather only once on the returned layer's mask.
            auto support =
                input.coverage.copy(input.bounds)
                    .scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            if (support.isNull()) {
                input.error = tr("Impossible de préparer la sélection à envoyer.");
                return input;
            }
            for (int y = 0; y < support.height(); ++y) {
                auto *row = support.scanLine(y);
                for (int x = 0; x < support.width(); ++x)
                    row[x] = row[x] ? 255 : 0;
            }
            if (input.maskEnabled)
                SelectionCoverage::attenuate(image, support, {});
        }
        if (_cancel->load())
            return input;
        if (qMax(image.width(), image.height()) > _maxEdge)
            image =
                image.scaled({_maxEdge, _maxEdge}, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        image          = image.convertToFormat(_nativePrecision ? ImageUtils::workingFormat(image)
                                                                : QImage::Format_RGBA8888);
        input.sentSize = image.size();
        if (image.isNull()) {
            input.error = tr("Impossible de préparer l’image à envoyer.");
            return input;
        }
        if (localImage) {
            input.localImage = std::move(image);
            return input;
        }
        QImage mask(input.sentSize, QImage::Format_RGBA8888);
        if (mask.isNull()) {
            input.error = tr("Impossible de préparer la sélection à envoyer.");
            return input;
        }
        mask.fill(Qt::transparent);
        if (input.selected && input.maskEnabled) {
            const auto selected =
                input.coverage.copy(input.bounds)
                    .scaled(input.sentSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            if (selected.isNull()) {
                input.error = tr("Impossible de préparer la sélection à envoyer.");
                return input;
            }
            for (int y = 0; y < mask.height(); ++y) {
                const auto *row = selected.constScanLine(y);
                auto *      out = mask.scanLine(y);
                for (int x = 0; x < mask.width(); ++x) {
                    out[4 * x] = out[4 * x + 1] = out[4 * x + 2] = 255;
                    out[4 * x + 3]                               = 255 - row[x];
                }
            }
        }
        QBuffer imageBuffer(&input.image), maskBuffer(&input.mask);
        imageBuffer.open(QIODevice::WriteOnly);
        maskBuffer.open(QIODevice::WriteOnly);
        if (image.isNull() || mask.isNull() || !image.save(&imageBuffer, "PNG") ||
            !mask.save(&maskBuffer, "PNG"))
            input.error = tr("Impossible d’encoder l’image IA.");
        if (input.image.size() > 50LL * 1024 * 1024 || input.mask.size() > 4LL * 1024 * 1024)
            input.error = tr("L’image ou le masque est trop volumineux. Réduisez la taille d’envoi "
                             "dans les réglages IA.");
        return input;
    });
}
bool Document::matchesAiSource(const PhotoCrap::Ai::Input &source) const
{
    return source.session == m_documentSession && source.contentId == m_contentId &&
           source.canvas == m_size;
}

bool Document::addRelightLayer(const PhotoCrap::Ai::Input &source,
                               const PhotoCrap::Ai::SurfaceGeometry &geometry)
{
    if (!matchesAiSource(source) || aiInsertionBusy() || transformActive() || !geometry.isValid())
        return false;
    const QSize size = geometry.normals.size();
    const bool refreshing = source.relightPhoto.has_value();
    if (refreshing && (m_active < 0 || m_active >= m_layers.size() || layerIdentity(m_active) != source.layerId ||
                       m_layers[m_active].adjustmentType != "relight" || m_layers[m_active].locked ||
                       m_editingMask))
        return false;
    if (qMax(size.width(), size.height()) > PhotoCrap::Ai::MaxSurfaceNormalEdge ||
        !validSize(width(), height(), int(m_layers.size()) + (refreshing ? 0 : 1)) ||
        !validLayerStorage(width(), height(), 0, !refreshing && hasSelection() ? 1 : 0, 0,
                           4LL * size.width() * size.height()))
        return false;
    // RGB stores camera-facing normals. Alpha stores relative inverse depth,
    // reserving zero for samples outside the model's validity mask.
    QImage packed = geometry.normals.convertToFormat(QImage::Format_RGBA8888);
    if (packed.isNull())
        return false;
    for (int y = 0; y < size.height(); ++y) {
        auto *out = packed.scanLine(y);
        const auto *depth = geometry.disparity.constScanLine(y);
        const auto *valid = geometry.validity.constScanLine(y);
        for (int x = 0; x < size.width(); ++x)
            out[4*x+3] = valid[x] ? 1 + (depth[x]*254+127)/255 : 0;
    }
    const QString label = refreshing ? tr("Adapter le rééclairage à la photo") : tr("Rééclairage");
    const auto before = snapshot(label);
    if (refreshing) {
        if (!selectRelightPhoto(m_layers, m_active, *source.relightPhoto))
            return false;
        if (*source.relightPhoto >= 0) {
            m_visibilityBeforeIsolation.clear();
            m_isolatedLayer = -1;
        }
        selectOnly(m_active);
        auto &layer = m_layers[m_active];
        // Keep handles at the same document positions after a crop or rotation.
        if (!layer.surfaceNormalTransform.isIdentity() || layer.surfaceNormalCanvas != m_size) {
            auto lights = layer.parameters.value("lights").toList();
            for (auto &entry : lights) {
                auto lamp = entry.toMap();
                const auto remap = [&](const QString &x, const QString &y) {
                    const QPointF p(lamp.value(x).toDouble() * layer.surfaceNormalCanvas.width() / 100.,
                                    lamp.value(y).toDouble() * layer.surfaceNormalCanvas.height() / 100.);
                    const auto mapped = layer.surfaceNormalTransform.map(p);
                    lamp[x] = mapped.x() * 100. / width();
                    lamp[y] = mapped.y() * 100. / height();
                };
                remap("x", "y");
                if (lamp.contains("targetX") && lamp.contains("targetY"))
                    remap("targetX", "targetY");
                entry = lamp;
            }
            layer.parameters["lights"] = lights;
        }
    } else {
        Layer layer;
        layer.kind = "adjustment";
        layer.adjustmentType = "relight";
        layer.name = label;
        layer.parameters = adjustmentDefaults(layer.adjustmentType);
        layer.mask = hasSelection() ? selectionMask() : ImageUtils::uniformMask(255);
        m_layers.append(std::move(layer));
        m_active = int(m_layers.size()) - 1;
        m_editingMask = false;
        selectOnly(m_active);
    }
    m_layers[m_active].surfaceNormals = std::move(packed);
    m_layers[m_active].surfaceNormalCanvas = m_size;
    m_layers[m_active].surfaceNormalTransform = {};
    commit(before, label, true, false);
    requestPreviewRender(true, true);
    emit imageChanged();
    return true;
}

bool Document::addSurfaceLayer(const PhotoCrap::Ai::Input &source, PixelPlane mask, QImage normals,
                               std::optional<QPoint> textPosition, bool followRelief,
                               qreal focalRatio, const QVariantList &crease)
{
    if (!matchesAiSource(source) || aiInsertionBusy() || transformActive() ||
        mask.size() != m_size || mask.format() != QImage::Format_Grayscale8 ||
        normals.format() != QImage::Format_RGB888 ||
        qMax(normals.width(), normals.height()) > PhotoCrap::Ai::MaxSurfaceNormalEdge ||
        (textPosition && !mask.rect().contains(*textPosition)) ||
        !validSize(width(), height(), int(m_layers.size()) + 1) ||
        !validLayerStorage(width(), height(), textPosition ? 1 : 0, !ImageUtils::compact(mask), 0,
                           normals.sizeInBytes()))
        return false;
    const bool hadSelection = hasSelection();
    const auto label = textPosition ? tr("Texte sur la surface") : tr("Peinture sur surface");
    Layer      layer(label, ImageUtils::blank({1, 1}, workingFormat()));
    if (layer.image.isNull())
        return false;
    layer.mask                = std::move(mask);
    layer.surfaceNormals      = std::move(normals);
    layer.surfaceNormalCanvas = m_size;
    const auto insert         = [&] {
        // The analysis covers the visible composition. Keep it above that
        // composition even when the previously active layer was in a group.
        m_layers.append(std::move(layer));
        m_active      = int(m_layers.size()) - 1;
        m_editingMask = false;
        selectOnly(m_active);
        if (hadSelection)
            clearActiveSelection(false);
    };
    if (textPosition) {
        layer.kind = "text";
        layer.parameters =
            PhotoCrap::Text::create(textPosition->x(), textPosition->y(), m_textSize, m_color);
        layer.parameters["surfaceRelief"] = followRelief;
        layer.parameters["surfaceFocal"]  = focalRatio;
        if (!crease.isEmpty())
            layer.parameters["surfaceCrease"] = crease;
        if (!PhotoCrap::Text::normalize(layer.parameters))
            return false;
        setTool(QStringLiteral("text"));
        m_beforeText = snapshot(label);
        insert();
        m_textEditing = m_textNewLayer = true;
        m_textGesture                  = false;
        m_text.clear();
        changed();
        emit textEditingChanged();
        emit textGeometryChanged();
        emit textEditRequested();
    } else {
        edit(label, insert);
        setTool(QStringLiteral("brush"));
    }
    if (hadSelection)
        emit selectionChanged();
    setDisplayChannel(0);
    return true;
}

bool Document::insertAiResult(const PhotoCrap::Ai::Input & _input,
                              const PhotoCrap::Ai::Result &_result, const QVariantMap &_metadata,
                              QString &_error)
{
    if (_input.session != m_documentSession || _input.canvas != m_size) {
        _error = tr("Le document a changé. Le résultat IA n’a pas été inséré.");
        return false;
    }
    // A retry of an already committed variant is a no-op, including at the
    // layer limit. Distinct variants and separate launches retain their identity.
    const auto batch   = _metadata.value("batch").toString();
    const int  variant = _metadata.value("variant").toInt();
    if (!batch.isEmpty() && variant > 0)
        for (const auto &layer : m_layers) {
            const auto ai = layer.parameters.value("_ai").toMap();
            if (ai.value("batch").toString() == batch && ai.value("variant").toInt() == variant)
                return true;
        }
    if (_result.bits != m_channelBits || aiInsertionBusy()) {
        _error = tr("Terminez l’édition en cours avant de relancer la génération IA.");
        return false;
    }
    if (_result.image.isNull() || !_result.error.isEmpty()) {
        _error = _result.error;
        return false;
    }
    if (!validSize(width(), height(), int(m_layers.size()) + 1) ||
        !validLayerStorage(width(), height(), 1, _input.selected ? 1 : 0)) {
        _error = tr("La limite de calques ou de mémoire empêche l’ajout du résultat IA.");
        return false;
    }
    finishLayerPreview();
    acceptLayerPropertiesSession();
    auto       label = tr("Calque IA — %1").arg(_metadata.value("provider").toString());
    const auto model = _metadata.value("model").toString();
    if (!model.isEmpty())
        label += " · " + model;
    label += tr(" · Variante %1/%2")
                 .arg(qMax(1, _metadata.value("variant", 1).toInt()))
                 .arg(qMax(1, _metadata.value("variantCount", 1).toInt()));
    const auto before = snapshot(label);
    int        source = -1;
    for (int i = 0; i < m_layers.size(); ++i)
        if (m_layers[i].historyId == _input.layerId)
            source = i;
    if (!batch.isEmpty()) {
        int latestVariant = 0;
        for (int i = 0; i < m_layers.size(); ++i) {
            const auto ai = m_layers[i].parameters.value("_ai").toMap();
            if (ai.value("batch").toString() == batch &&
                ai.value("variant").toInt() > latestVariant) {
                source        = i;
                latestVariant = ai.value("variant").toInt();
            }
        }
    }
    int     insertion = source < 0 ? int(m_layers.size()) : source + 1;
    QString groupId   = source < 0 ? QString{} : m_layers[source].groupId;
    for (int i = 0; i < m_layers.size(); ++i) {
        auto &group = m_layers[i];
        if (group.kind == "group" && group.groupId == groupId && !groupId.isEmpty()) {
            if (group.locked || !group.visible) {
                insertion = i + 1;
                groupId.clear();
            } else {
                group.groupCollapsed = false;
                if (source == i)
                    insertion = i;
            }
            break;
        }
    }
    // Alternatives from this request stay available; display only the latest.
    // This visibility change is part of the same undo step as its insertion.
    if (!batch.isEmpty())
        for (auto &previous : m_layers)
            if (previous.parameters.value("_ai").toMap().value("batch").toString() == batch)
                previous.visible = false;
    Layer layer;
    layer.name              = label;
    layer.image             = _result.image;
    layer.aiSource          = _result.original;
    layer.mask              = _result.mask;
    if (_input.originalSource && _input.selected) {
        layer.maskEnabled                = _input.maskEnabled;
        layer.parameters["_maskDensity"] = _input.maskDensity;
        layer.parameters["_maskFeather"] = _input.maskFeather;
    }
    layer.groupId           = groupId;
    layer.parameters["_ai"] = _metadata;
    m_layers.insert(insertion, std::move(layer));
    m_active      = insertion;
    m_editingMask = false;
    selectOnly(m_active);
    showCompositeView();
    commit(before, label);
    return true;
}
