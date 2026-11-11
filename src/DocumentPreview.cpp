#include "Document.h"
#include "ImageUtils.h"
#include "masks/VectorMask.h"
#include "model3d/Model3D.h"
#include "tools/LayerTransform.h"

#include <QCache>
#include <QColorSpace>
#include <QJSValue>
#include <QMutexLocker>
#include <QScopedValueRollback>
#include <QSet>
#include <QtConcurrent>
#include <QtMath>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
QMutex                  screenProxyMutex;
QCache<QString, QImage> screenProxyCache(128 * 1024);
template <typename Image>
QImage cachedScreenProxy(const Image &_source, const QSize &_size, QImage::Format _format)
{
    if (_source.isNull() || !_size.isValid())
        return {};
    const QString key = QStringLiteral("%1:%2x%3:%4")
                            .arg(_source.cacheKey())
                            .arg(_size.width())
                            .arg(_size.height())
                            .arg(int(_format));
    {
        QMutexLocker locker(&screenProxyMutex);
        if (const QImage *cached = screenProxyCache.object(key))
            return *cached;
    }
    QImage    proxy = _source.scaled(_size, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                          .convertToFormat(_format);
    const int cost =
        qMax(1, int(qMin<qsizetype>(proxy.sizeInBytes() / 1024, std::numeric_limits<int>::max())));
    {
        QMutexLocker locker(&screenProxyMutex);
        screenProxyCache.insert(key, new QImage(proxy), cost);
    }
    return proxy;
}
} // namespace

void Document::beginLayerPreview()
{
    if (m_previewingLayer || m_active < 0 || m_active >= m_layers.size())
        return;
    cancelOperation();
    endStroke();
    // Starting a drag must never force a full-resolution composition on the
    // UI thread. Keep the cached image only when it already represents the
    // current layers; otherwise cancelLayerPreview() will schedule a fresh
    // asynchronous render of the restored snapshot.
    m_beforeLayerPreviewComposite = m_cacheDirty ? QImage{} : m_composite;
    m_beforeLayerPreview          = snapshot(tr("Aperçu"));
    m_previewingLayer             = true;
    m_layerPreviewChanged         = false;
    m_frequencyPreviewDraft       = false;
}
void Document::previewLayerParameter(const QString &_key, const QVariant &_value)
{
    if (m_active < 0 || m_active >= m_layers.size() || _key.isEmpty())
        return;
    if (!m_previewingLayer)
        beginLayerPreview();
    const QVariant value = _value.metaType() == QMetaType::fromType<QJSValue>()
                               ? _value.value<QJSValue>().toVariant(QJSValue::ConvertJSObjects)
                               : _value;
    if (!m_layers[m_active].directType.isEmpty()) {
        if (m_layers[m_active].directParameters.value(_key) == value)
            return;
        m_layers[m_active].directParameters[_key] = value;
        if (_key == "_limitSelection")
            m_layers[m_active].directSelection =
                value.toBool() && hasSelection() ? selectionMask() : QImage{};
        m_layerPreviewChanged = true;
        m_cacheDirty = m_fullCompositeDirty = true;
        requestPreviewRender(true);
        emit layersChanged();
        return;
    }
    if (m_layers[m_active].parameters.value(_key) == value)
        return;
    const QString pairId = m_layers[m_active].parameters.value("pairId").toString();
    if (_key == "radius" && !pairId.isEmpty()) {
        m_frequencyPreviewDraft = true;
        m_frequencyCache.clear();
        for (auto &layer : m_layers)
            if (layer.parameters.value("pairId").toString() == pairId)
                layer.parameters.insert(_key, value);
    } else
        m_layers[m_active].parameters.insert(_key, value);
    // The property panel already owns the live slider value. Avoid rebuilding
    // the layer model and its thumbnails for every pointer event.
    m_layerPreviewChanged = true;
    m_fullCompositeDirty  = true;
    m_cacheDirty          = true;
    requestPreviewRender(true);
}
void Document::finishLayerPreview(const QString &_label)
{
    if (!directAdjustmentInfo().isEmpty())
        return;
    if (!m_previewingLayer)
        return;
    m_previewingLayer       = false;
    m_frequencyPreviewDraft = false;
    if (m_layerPreviewChanged) {
        QImage screenPreview;
        {
            QMutexLocker locker(&m_previewImageMutex);
            screenPreview = m_previewComposite;
        }
        {
            QScopedValueRollback<bool> preserve(m_preserveGpuTonePreview, gpuTonePreviewActive());
            commit(m_beforeLayerPreview, _label);
        }
        m_gpuToneAuxTimer.stop();
        {
            QMutexLocker locker(&m_previewImageMutex);
            m_previewComposite = screenPreview;
        }
        requestPreviewRender(false);
    }
    m_beforeLayerPreview          = {};
    m_beforeLayerPreviewComposite = {};
    m_layerPreviewChanged         = false;
}

void Document::requestPreviewRender(bool _draft, bool _refine, bool _allowGpu)
{
    // Navigator/histogram work must not displace a queued mask/base update.
    // Completing that GPU request schedules a fresh auxiliary update itself.
    if (_draft && !_allowGpu && gpuTonePreviewActive() && m_previewRenderPending &&
        m_pendingPreviewGpuBase)
        return;
    if (_draft && _allowGpu && updateGpuTonePreview())
        return;
    m_pendingPreviewGpuBase =
        _draft && _allowGpu && m_previewingLayer && !m_gpuToneRejected && gpuToneEligible();
    m_pendingPreviewGpuAux     = _draft && !_allowGpu && gpuTonePreviewActive();
    m_pendingTransform =
        _draft && m_transform && m_transform->job.path < 0 && !m_transform->job.selectionOnly
            ? std::make_shared<const PhotoCrap::Transform::Job>(m_transform->job)
            : nullptr;
    m_pendingModel3DPreview    = _draft ? m_model3DPreview : nullptr;
    m_pendingPreviewLayers     = m_layers;
    m_pendingPreviewSize       = m_size;
    m_pendingPreviewTargetSize = m_previewTargetSize;
    m_pendingPreviewDraft      = _draft;
    m_pendingPreviewRefine     = _refine;
    m_previewRenderPending     = true;
    m_previewRenderActive.store(true);
    ++m_previewRequestGeneration;
    emit gpuStackChanged();
    if (_draft && (m_previewingLayer || m_gradientGesture != NoGradientGesture) &&
        m_gpuStackDisplayActive) {
        if (!m_gpuStackAuxTimer.isActive())
            m_gpuStackAuxTimer.start();
    } else {
        m_gpuStackAuxTimer.stop();
    }
    if (!m_previewRenderWatcher.isRunning() && !m_gpuStackAuxTimer.isActive())
        startPendingPreviewRender();
}

void Document::startPendingPreviewRender()
{
    if (!m_previewRenderPending || m_previewRenderWatcher.isRunning() ||
        m_gpuStackAuxTimer.isActive())
        return;
    QVector<Layer> layers      = std::move(m_pendingPreviewLayers);
    const QSize    size        = m_pendingPreviewSize;
    const QSize    targetSize  = m_pendingPreviewTargetSize;
    const bool     draft       = m_pendingPreviewDraft;
    const bool     refine      = m_pendingPreviewRefine;
    m_previewRenderPending     = false;
    m_runningPreviewGeneration = m_previewRequestGeneration;
    m_runningPreviewDraft      = draft;
    m_runningPreviewRefine     = refine;
    m_runningPreviewTargetSize = targetSize;
    const auto scratch         = m_scratchCache;
    const auto transform       = std::exchange(m_pendingTransform, {});
    const auto model           = std::exchange(m_pendingModel3DPreview, {});
    m_runningModel3DPreview    = bool(model);
    m_runningPreviewGpuBase    = m_pendingPreviewGpuBase;
    m_runningPreviewGpuAux     = m_pendingPreviewGpuAux;
    m_runningGpuToneEpoch      = m_gpuToneEpoch;
    const bool gpuBase         = m_runningPreviewGpuBase;
    PixelPlane mask;
    int        feather = 0;
    if (gpuBase) {
        const auto &layer = layers.constLast();
        if (layer.maskEnabled && !layer.mask.isNull() && !ImageUtils::compact(layer.mask)) {
            mask = layer.mask;
            feather =
                qRound(qBound(0., layer.parameters.value("_maskFeather", 0.).toDouble(), 100.));
        }
        layers.removeLast();
    }
    // Mask feathering changes no pixels in the stack below the adjustment.
    const QImage cachedBase = gpuBase ? m_gpuToneBase : QImage{};
    m_previewRenderWatcher.setFuture(QtConcurrent::run(
        [layers = std::move(layers), size, targetSize, draft, scratch, gpuBase, mask, feather,
         cachedBase, transform, model]() mutable -> PreviewRenderResult {
            QImage result = cachedBase.isNull()
                                ? renderPreviewSnapshot(std::move(layers), size, targetSize, draft,
                                                        scratch, transform, model)
                                : cachedBase;
            if (gpuBase) {
                if (ImageUtils::is16Bit(result))
                    return {};
                // Transparent bases retain the CPU compositor's existing semantics.
                if (cachedBase.isNull()) {
                    const QImage image = result.convertToFormat(QImage::Format_ARGB32);
                    for (int y = 0; y < image.height(); ++y) {
                        const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
                        for (int x = 0; x < image.width(); ++x)
                            if (qAlpha(row[x]) != 255)
                                return {};
                    }
                }
                if (!mask.isNull()) {
                    const qint64 key = mask.cacheKey();
                    QImage       preparedMask =
                        mask.size() != result.size()
                            ? cachedScreenProxy(mask, result.size(), QImage::Format_Grayscale8)
                            : QImage(mask);
                    if (feather > 0)
                        preparedMask = blurImage(preparedMask, feather)
                                           .convertToFormat(QImage::Format_Grayscale8);
                    if (preparedMask.isNull())
                        return {};
                    return {result, preparedMask, key, feather};
                }
            }
            return {result, {}};
        }));
}

QImage Document::renderPreviewSnapshot(QVector<Layer> _layers, const QSize &_size,
                                       const QSize &_targetSize, bool _draft,
                                       std::shared_ptr<ScratchImageCache>               _scratch,
                                       std::shared_ptr<const PhotoCrap::Transform::Job> _transform,
                                       std::shared_ptr<const Model3DPreview>            _model)
{
    int modelIndex = -1;
    if (_model) {
        modelIndex = _model->layer;
        if (modelIndex < 0 || modelIndex >= _layers.size() || !_layers.at(modelIndex).model3D)
            return {};
        if (_model->copy) {
            const auto duplicate = _layers.at(modelIndex);
            _layers.insert(++modelIndex, duplicate);
        }
        _layers[modelIndex].parameters = _model->parameters;
    }
    int                  channelBits = 8;
    QHash<QString, bool> groupVisibility;
    for (const Layer &layer : std::as_const(_layers)) {
        if (ImageUtils::is16Bit(layer.image) || ImageUtils::is16Bit(layer.source))
            channelBits = 16;
        if (layer.kind == QStringLiteral("group"))
            groupVisibility.insert(layer.groupId, layer.visible);
    }
    for (Layer &layer : _layers) {
        if (layer.kind == QStringLiteral("group"))
            continue;
        if (_transform && _transform->maskView && !_transform->layers.isEmpty() &&
            &layer == &_layers[_transform->layers.first()])
            continue;
        if (!layer.visible || layer.opacity <= 0 ||
            (!layer.groupId.isEmpty() && !groupVisibility.value(layer.groupId, true))) {
            // Keep the placeholder so an invisible clipping base still breaks
            // its clipping chain, but do not scale or retain unused image planes.
            layer.visible = false;
            layer.image   = {};
            layer.mask    = {};
            layer.source  = {};
        }
    }
    QSize renderSize = _size;
    if (_draft) {
        if (_targetSize.isValid())
            renderSize = _targetSize.boundedTo(_size);
        else if (qMax(_size.width(), _size.height()) > 960)
            renderSize.scale(960, 960, Qt::KeepAspectRatio);
    }
    if (_model && qMax(renderSize.width(), renderSize.height()) > 512)
        renderSize.scale(512, 512, Qt::KeepAspectRatio);
    const auto bits         = renderSize == _size ? channelBits : 8;
    const auto rendersModel = [&](const Layer &layer) {
        return _model && layer.model3D && layer.visible &&
               (&layer == &_layers.at(modelIndex) ||
                layer.parameters.value("light3d") != _model->parameters.value("light3d"));
    };
    if (renderSize != _size) {
        const qreal scale = qreal(renderSize.width()) / qMax(1, _size.width());
        for (Layer &layer : _layers) {
            if (!layer.directType.isEmpty())
                layer.directParameters["_canvasSize"] = renderSize;
            if (!layer.directSelection.isNull())
                layer.directSelection =
                    cachedScreenProxy(layer.directSelection, renderSize, QImage::Format_Grayscale8);
            for (const auto &key : {"radius", "distance", "cellSize"})
                if (layer.directParameters.contains(key))
                    layer.directParameters[key] =
                        layer.directParameters.value(key).toDouble() * scale;
            if (layer.parameters.contains("_maskFeather"))
                layer.parameters["_maskFeather"] =
                    layer.parameters.value("_maskFeather").toDouble() * scale;
            if (layer.parameters.contains("_vectorFeather"))
                layer.parameters["_vectorFeather"] =
                    layer.parameters.value("_vectorFeather").toDouble() * scale;
            if (!rendersModel(layer) && !layer.image.isNull() && !ImageUtils::compact(layer.image))
                layer.image =
                    cachedScreenProxy(layer.image, renderSize, QImage::Format_ARGB32_Premultiplied);
            if (!layer.mask.isNull() && !ImageUtils::compact(layer.mask))
                layer.mask = cachedScreenProxy(layer.mask, renderSize, QImage::Format_Grayscale8);
            if (!layer.source.isNull())
                layer.source = cachedScreenProxy(layer.source, renderSize,
                                                 QImage::Format_ARGB32_Premultiplied);
            for (const QString &key :
                 {QStringLiteral("radius"), QStringLiteral("distance"), QStringLiteral("cellSize")})
                if (layer.parameters.contains(key))
                    layer.parameters.insert(
                        key, qMax(1., layer.parameters.value(key).toDouble() * scale));
        }
    }
    if (_model) {
        const auto scale = QTransform::fromScale(qreal(renderSize.width()) / _size.width(),
                                                 qreal(renderSize.height()) / _size.height());
        for (auto &layer : _layers) {
            if (!rendersModel(layer))
                continue;
            auto parameters       = layer.parameters;
            parameters["light3d"] = _model->parameters.value("light3d");
            if (!PhotoCrap::Model3D::transformPlacement(parameters, scale))
                return {};
            layer.image =
                PhotoCrap::Model3D::render(*layer.model3D, parameters, renderSize,
                                           bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                                      : QImage::Format_ARGB32_Premultiplied);
            if (layer.image.isNull())
                return {};
        }
    }
    if (_transform) {
        for (const int index : _transform->layers) {
            auto &layer = _layers[index];
            for (auto *plane : {&layer.image, &layer.mask})
                if (ImageUtils::compact(*plane))
                    *plane = PixelPlane::uniform(renderSize, plane->uniformPixel());
        }
        if (!PhotoCrap::Transform::applyLayers(_layers, renderSize, _size, *_transform, true))
            return {};
        if (_transform->maskView && !_transform->layers.isEmpty()) {
            const auto &layer = _layers[_transform->layers.first()];
            QImage      mask  = _transform->vectorMask
                                    ? PhotoCrap::Masks::vectorCoverage(layer, renderSize,
                                                                 QRect(QPoint{}, renderSize))
                                    : ImageUtils::expandedPlane(layer.mask, renderSize);
            mask              = mask.convertToFormat(QImage::Format_RGB32);
            mask.setColorSpace(QColorSpace::SRgb);
            return mask;
        }
    }
    Document renderer;
    renderer.m_scratchCache       = std::move(_scratch);
    renderer.m_historyBrushSource = {}; // Render helpers need no retained initial state.
    renderer.m_layers             = std::move(_layers);
    renderer.m_size               = renderSize;
    // The full render retains document precision even when every pixel layer
    // is invisible; reduced screen proxies always use the display format.
    renderer.m_channelBits        = bits;
    renderer.m_cacheDirty         = true;
    renderer.m_fullCompositeDirty = true;
    renderer.m_dirtyRegion        = {};
    for (const auto &layer : renderer.m_layers)
        if (!layer.directType.isEmpty() && layer.directMask &&
            layer.directParameters.value("_maskOnlyView").toBool()) {
            const auto mask      = ImageUtils::expandedPlane(layer.mask, renderSize);
            auto       displayed = directAdjustedImage(mask, layer, QRect(QPoint{}, renderSize))
                                 .convertToFormat(QImage::Format_RGB32);
            displayed.setColorSpace(QColorSpace::SRgb);
            return displayed;
        }
    return renderer.composite();
}
void Document::cancelLayerPreview()
{
    invalidateGpuTonePreview();
    if (!m_previewingLayer)
        return;
    const bool changedPreview = m_layerPreviewChanged;
    m_previewingLayer         = false;
    m_layerPreviewChanged     = false;
    m_frequencyPreviewDraft   = false;
    if (changedPreview) {
        restore(m_beforeLayerPreview);
        if (!m_beforeLayerPreviewComposite.isNull()) {
            m_composite          = m_beforeLayerPreviewComposite;
            m_cacheDirty         = false;
            m_fullCompositeDirty = false;
            m_dirtyRegion        = {};
        } else
            requestPreviewRender(true, true);
    }
    m_beforeLayerPreview          = {};
    m_beforeLayerPreviewComposite = {};
}

void Document::beginLayerPropertiesSession()
{
    if (m_layerPropertiesSession)
        return;
    cancelLayerPreview();
    m_beforeLayerProperties     = snapshot(tr("Modifier les propriétés du calque"));
    m_undoBeforeLayerProperties = m_undo;
    m_redoBeforeLayerProperties = m_redo;
    m_layerPropertiesSession    = true;
}

void Document::acceptLayerPropertiesSession()
{
    if (!m_layerPropertiesSession)
        return;
    m_layerPropertiesSession = false;
    m_beforeLayerProperties  = {};
    m_undoBeforeLayerProperties.clear();
    m_redoBeforeLayerProperties.clear();
}

void Document::cancelLayerPropertiesSession()
{
    if (!m_layerPropertiesSession)
        return;
    cancelFrequencyRetouch();
    cancelLayerPreview();
    const Snapshot before    = m_beforeLayerProperties;
    m_undo                   = m_undoBeforeLayerProperties;
    m_redo                   = m_redoBeforeLayerProperties;
    m_layerPropertiesSession = false;
    m_beforeLayerProperties  = {};
    m_undoBeforeLayerProperties.clear();
    m_redoBeforeLayerProperties.clear();
    restore(before);
}

QColor Document::sampleCurvePoint(const QString &_mode, int _x, int _y)
{
    static const QSet<QString> modes{QStringLiteral("black"), QStringLiteral("gray"),
                                     QStringLiteral("white")};
    if (!modes.contains(_mode) || m_active < 0 || m_active >= m_layers.size() ||
        (m_layers[m_active].adjustmentType != QStringLiteral("curves") &&
         m_layers[m_active].directType != QStringLiteral("curves")) ||
        m_size.isEmpty())
        return {};
    const QColor sampled = sampleCompositeColor(_x, _y);
    if (!sampled.isValid())
        return {};
    beginLayerPreview();
    previewLayerParameter(_mode + QStringLiteral("Sample"),
                          QVariantList{sampled.red(), sampled.green(), sampled.blue()});
    finishLayerPreview(tr("Échantillonner le point de la courbe"));
    return sampled;
}

QColor Document::sampleCompositeColor(int _x, int _y) const
{
    if (m_size.isEmpty())
        return {};
    const QPoint point(qBound(0, _x, m_size.width() - 1), qBound(0, _y, m_size.height() - 1));
    const QImage sample = compositeRegion(QRect(point, QSize(1, 1)));
    return sample.isNull() ? QColor{} : sample.pixelColor(0, 0);
}

QVariantList Document::autoBlackWhiteValues() const
{
    const QVariantList defaults{40, 60, 40, 60, 20, 80};
    if (m_active < 0 || m_active >= m_layers.size())
        return defaults;
    const QSize target = m_size.scaled(QSize(640, 640), Qt::KeepAspectRatio);
    QImage      source;
    const auto &layer = m_layers[m_active];
    if (!layer.directType.isEmpty())
        source = ImageUtils::expandedPlane(layer.directMask ? layer.mask : layer.image, target)
                     .convertToFormat(QImage::Format_ARGB32);
    else {
        QVector<Layer> layers    = m_layers;
        layers[m_active].visible = false;
        source                   = renderPreviewSnapshot(std::move(layers), m_size, target, true)
                     .convertToFormat(QImage::Format_ARGB32);
    }
    if (source.isNull())
        return defaults;

    std::array<double, 6> luminanceSum{};
    std::array<double, 6> weights{};
    double                globalSum    = 0.;
    double                globalWeight = 0.;
    for (int y = 0; y < source.height(); ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(source.constScanLine(y));
        for (int x = 0; x < source.width(); ++x) {
            const QRgb pixel = row[x];
            const int  red = qRed(pixel), green = qGreen(pixel), blue = qBlue(pixel);
            const int  maximum = qMax(red, qMax(green, blue));
            const int  minimum = qMin(red, qMin(green, blue));
            const int  chroma  = maximum - minimum;
            if (chroma < 6)
                continue;
            double hue = 0.;
            if (maximum == red)
                hue = std::fmod((green - blue) / double(chroma) + 6., 6.);
            else if (maximum == green)
                hue = (blue - red) / double(chroma) + 2.;
            else
                hue = (red - green) / double(chroma) + 4.;
            const int    sector    = qBound(0, int(std::floor(hue + .5)) % 6, 5);
            const double weight    = chroma / 255.;
            const double luminance = .2126 * red + .7152 * green + .0722 * blue;
            luminanceSum[size_t(sector)] += luminance * weight;
            weights[size_t(sector)] += weight;
            globalSum += luminance * weight;
            globalWeight += weight;
        }
    }
    if (globalWeight <= 0.)
        return defaults;
    const double globalMean = globalSum / globalWeight;
    QVariantList result;
    for (int index = 0; index < 6; ++index) {
        const double familyMean = weights[size_t(index)] > 0.
                                      ? luminanceSum[size_t(index)] / weights[size_t(index)]
                                      : globalMean;
        result.append(
            qBound(-200, defaults[index].toInt() + qRound((globalMean - familyMean) * .45), 300));
    }
    return result;
}
