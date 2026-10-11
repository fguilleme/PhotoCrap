#include "Document.h"

#include <QCache>
#include <QJSValue>
#include <QMutexLocker>
#include <QSet>
#include <QtConcurrent>
#include <QtMath>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
QImage cachedScreenProxy(const QImage &_source, const QSize &_size, QImage::Format _format)
{
    if (_source.isNull() || !_size.isValid())
        return {};
    const QString                  key = QStringLiteral("%1:%2x%3:%4")
                                             .arg(_source.cacheKey())
                                             .arg(_size.width())
                                             .arg(_size.height())
                                             .arg(int(_format));
    static QMutex                  cacheMutex;
    static QCache<QString, QImage> cache(128 * 1024);
    {
        QMutexLocker locker(&cacheMutex);
        if (const QImage *cached = cache.object(key))
            return *cached;
    }
    QImage    proxy = _source.scaled(_size, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                          .convertToFormat(_format);
    const int cost =
        qMax(1, int(qMin<qsizetype>(proxy.sizeInBytes() / 1024, std::numeric_limits<int>::max())));
    {
        QMutexLocker locker(&cacheMutex);
        cache.insert(key, new QImage(proxy), cost);
    }
    return proxy;
}
} // namespace

void Document::beginLayerPreview()
{
    if (m_previewingLayer || m_active < 0 || m_active >= m_layers.size())
        return;
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
        commit(m_beforeLayerPreview, _label);
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

void Document::requestPreviewRender(bool _draft, bool _refine)
{
    m_pendingPreviewLayers     = m_layers;
    m_pendingPreviewSize       = m_size;
    m_pendingPreviewTargetSize = m_previewTargetSize;
    m_pendingPreviewDraft      = _draft;
    m_pendingPreviewRefine     = _refine;
    m_previewRenderPending     = true;
    m_previewRenderActive.store(true);
    ++m_previewRequestGeneration;
    if (!m_previewRenderWatcher.isRunning())
        startPendingPreviewRender();
}

void Document::startPendingPreviewRender()
{
    if (!m_previewRenderPending || m_previewRenderWatcher.isRunning())
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
    m_previewRenderWatcher.setFuture(
        QtConcurrent::run([layers = std::move(layers), size, targetSize, draft]() mutable {
            return renderPreviewSnapshot(std::move(layers), size, targetSize, draft);
        }));
}

QImage Document::renderPreviewSnapshot(QVector<Layer> _layers, const QSize &_size,
                                       const QSize &_targetSize, bool _draft)
{
    QSize renderSize = _size;
    if (_draft) {
        if (_targetSize.isValid())
            renderSize = _targetSize.boundedTo(_size);
        else if (qMax(_size.width(), _size.height()) > 960)
            renderSize.scale(960, 960, Qt::KeepAspectRatio);
    }
    if (renderSize != _size) {
        const qreal scale = qreal(renderSize.width()) / qMax(1, _size.width());
        for (Layer &layer : _layers) {
            if (!layer.image.isNull())
                layer.image =
                    cachedScreenProxy(layer.image, renderSize, QImage::Format_ARGB32_Premultiplied);
            if (!layer.mask.isNull())
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
    Document renderer;
    renderer.m_historyBrushSource = {}; // Render helpers need no retained initial state.
    renderer.m_layers             = std::move(_layers);
    renderer.m_size               = renderSize;
    renderer.m_cacheDirty         = true;
    renderer.m_fullCompositeDirty = true;
    renderer.m_dirtyRegion        = {};
    return renderer.composite().copy();
}
void Document::cancelLayerPreview()
{
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
        m_layers[m_active].adjustmentType != QStringLiteral("curves") || m_size.isEmpty())
        return {};
    const QColor sampled = composite().pixelColor(qBound(0, _x, m_size.width() - 1),
                                                  qBound(0, _y, m_size.height() - 1));
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
    return composite().pixelColor(qBound(0, _x, m_size.width() - 1),
                                  qBound(0, _y, m_size.height() - 1));
}

QVariantList Document::autoBlackWhiteValues() const
{
    const QVariantList defaults{40, 60, 40, 60, 20, 80};
    if (m_active < 0 || m_active >= m_layers.size())
        return defaults;
    QVector<Layer> layers    = m_layers;
    layers[m_active].visible = false;
    const QSize  target      = m_size.scaled(QSize(640, 640), Qt::KeepAspectRatio);
    const QImage source      = renderPreviewSnapshot(std::move(layers), m_size, target, true)
                                   .convertToFormat(QImage::Format_ARGB32);
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
