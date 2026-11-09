#include "Document.h"
#include "ImageUtils.h"
#include "ai/AiImageTypes.h"
#include <QBuffer>
#include <QImageReader>
#include <QtConcurrentRun>

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
                                                       bool _original, bool _nativePrecision)
{
    endStroke();
    finishTextEdit();
    finishLayerPreview();
    acceptLayerPropertiesSession();
    PhotoCrap::Ai::Input input;
    input.session  = m_documentSession;
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
    const auto layers   = m_layers;
    const auto coverage = m_selectionMask;
    const auto region   = m_selectionRegion;
    const auto scratch  = m_scratchCache;
    const auto bits     = m_channelBits;
    return QtConcurrent::run([input, layers, coverage, region, scratch, bits, _maxEdge, _cancel,
                              original, originalMask, _nativePrecision]() mutable {
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
