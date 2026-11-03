#include "Document.h"
#include "DocumentStorage.h"
#include "ImageUtils.h"
#include "storage/ScratchImageCache.h"

#include <QFont>
#include <QPainter>
#include <QTimer>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <utility>

namespace
{
constexpr qint64      MaxPixels = 32LL * 1024 * 1024;
constexpr int         MaxLayers = 512;
QHash<qint64, qint64> layerBlocks(const Layer &layer)
{
    auto blocks = layer.image.storageBlocks();
    blocks.insert(layer.mask.storageBlocks());
    blocks.insert(layer.source.storageBlocks());
    return blocks;
}

void freezeLayers(const QVector<Layer> &layers)
{
    QHash<qint64, const PixelPlane *> planes;
    for (const auto &layer : layers)
        for (const auto *plane : {std::addressof(layer.image), std::addressof(layer.mask),
                                  std::addressof(layer.source)}) {
            const auto key = plane->cacheKey();
            if (auto previous = planes.value(key); previous && plane->shareStorage(*previous))
                continue;
            plane->freeze();
            planes.insert(plane->cacheKey(), plane);
        }
}

// Regions and paths share their backing storage across snapshots, like QImage.
// Account for each retained allocation once, without counting live selection data twice.
struct SelectionMemory {
    QSet<quintptr> seen;
    qint64         bytes = 0;
    void           add(const QRegion &_region)
    {
        if (_region.isEmpty())
            return;
        const auto key = reinterpret_cast<quintptr>(_region.begin());
        if (!seen.contains(key)) {
            seen.insert(key);
            bytes += qint64(_region.rectCount()) * sizeof(QRect);
        }
    }
    void add(const QPolygonF &_path)
    {
        if (_path.isEmpty())
            return;
        const auto key = reinterpret_cast<quintptr>(_path.constData());
        if (!seen.contains(key)) {
            seen.insert(key);
            bytes += qint64(_path.size()) * sizeof(QPointF);
        }
    }
};
} // namespace

Document::Document(QObject *_parent) : QObject(_parent)
{
    m_planeOffloadTimer.setSingleShot(true);
    m_planeOffloadTimer.setInterval(500);
    connect(&m_planeOffloadTimer, &QTimer::timeout, this, [this] {
        if (m_drawing || m_previewingLayer || pixelEditBusy() ||
            m_planeOffloadWatcher.isRunning()) {
            schedulePlaneOffload();
            return;
        }
        const auto cache = m_scratchCache;
        if (!cache || cache->options().maxBytes <= 0)
            return;
        QVector<PixelPlane> planes;
        const auto          collect = [&](const QVector<Layer> &layers) {
            for (const auto &layer : layers) {
                planes.append(layer.image);
                planes.append(layer.mask);
                planes.append(layer.source);
            }
        };
        collect(m_layers);
        for (const auto &state : m_undo)
            collect(state.layers);
        for (const auto &state : m_redo)
            collect(state.layers);
        collect(m_historyBrushSource.layers);
        const qint64 budget  = qint64(m_planeRamMiB) * 1024 * 1024;
        const auto   cancel  = std::make_shared<std::atomic_bool>(false);
        m_planeOffloadCancel = cancel;
        m_planeOffloadWatcher.setFuture(
            QtConcurrent::run([cache, budget, cancel, planes = std::move(planes)] {
                QHash<qint64, qint64> blocks;
                for (const auto &plane : planes) {
                    if (cancel->load())
                        return;
                    blocks.insert(plane.tiles().heapBlocks());
                }
                qint64 bytes = 0;
                for (auto value : blocks)
                    bytes += value;
                // Prefer older versions, collected after the live layers, to keep
                // live editing data in RAM when the budget permits it.
                for (auto it = planes.crbegin();
                     it != planes.crend() && bytes > budget && !cancel->load(); ++it) {
                    const auto before = it->tiles().heapBlocks();
                    if (before.isEmpty())
                        continue;
                    it->offload(*cache, [cancel] { return !cancel->load(); });
                    const auto after = it->tiles().heapBlocks();
                    for (auto b = before.cbegin(); b != before.cend(); ++b)
                        if (!after.contains(b.key()) && blocks.contains(b.key())) {
                            bytes -= blocks.take(b.key());
                        }
                }
            }));
    });
    connect(&m_rawWatcher, &QFutureWatcher<RawImageResult>::finished, this,
            &Document::finishRawLoad);
    m_gpuToneAuxTimer.setSingleShot(true);
    m_gpuStackAuxTimer.setSingleShot(true);
    m_gpuStackAuxTimer.setInterval(100);
    connect(&m_gpuStackAuxTimer, &QTimer::timeout, this, &Document::startPendingPreviewRender);
    m_gpuToneAuxTimer.setInterval(100);
    connect(&m_gpuToneAuxTimer, &QTimer::timeout, this, [this] {
        if (m_previewingLayer && gpuTonePreviewActive())
            requestPreviewRender(true, false, false);
    });
    connect(&m_previewRenderWatcher, &QFutureWatcher<PreviewRenderResult>::finished, this, [this] {
        const auto   output = m_previewRenderWatcher.result();
        const QImage result = output.image;
        if (m_runningPreviewGpuBase) {
            finishGpuToneBase(output, m_runningGpuToneEpoch);
            if (m_previewRenderPending)
                QTimer::singleShot(0, this, &Document::startPendingPreviewRender);
            else
                m_previewRenderActive.store(m_gpuToneActive);
            return;
        }
        const bool current   = m_runningPreviewGeneration == m_previewRequestGeneration;
        const bool auxiliary = m_runningPreviewGpuAux && m_previewingLayer &&
                               gpuTonePreviewActive() && m_runningGpuToneEpoch == m_gpuToneEpoch;
        if ((current || auxiliary) && !result.isNull()) {
            QImage screenResult = result;
            if (!m_runningPreviewDraft && m_runningPreviewTargetSize.isValid()) {
                const QSize target = m_runningPreviewTargetSize.boundedTo(result.size());
                if (target != result.size())
                    screenResult =
                        result.scaled(target, Qt::IgnoreAspectRatio, Qt::FastTransformation);
                screenResult = screenResult.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            }
            {
                QMutexLocker locker(&m_previewImageMutex);
                if (m_runningPreviewDraft)
                    m_previewComposite = result;
                else {
                    m_composite        = result;
                    m_previewComposite = screenResult;
                }
            }
            if (!m_runningPreviewDraft) {
                m_cacheDirty         = false;
                m_fullCompositeDirty = false;
                m_dirtyRegion        = {};
            }
            if (m_patchAwaitingImage) {
                m_patchAwaitingImage = false;
                clearPatch();
            }
            if (m_contentMoveAwaitingImage) {
                m_contentMoveAwaitingImage = false;
                clearPatch();
            }
            if (!m_runningPreviewDraft && m_gpuToneActive) {
                m_gpuToneActive = false;
                emit gpuTonePreviewChanged();
            }
            ++m_revision;
            emit imageChanged();
        }
        if (m_previewRenderPending)
            QTimer::singleShot(0, this, &Document::startPendingPreviewRender);
        else if (current && m_runningPreviewDraft && m_runningPreviewRefine)
            requestPreviewRender(false);
        else
            m_previewRenderActive.store(m_gpuToneActive);
    });
    connect(this, &Document::selectionChanged, this, [this] {
        cancelOperation();
        cancelSelectionOperation();
        if (!m_selectionMask.isNull() && m_selectionMaskRegion != m_selectionRegion) {
            m_selectionMask       = {};
            m_selectionOutline    = {};
            m_selectionMaskRegion = {};
        }
        if (!m_selectionEdgesVisible) {
            m_selectionEdgesVisible = true;
            emit selectionDisplayChanged();
        }
        invalidatePatch();
        invalidateContentMove();
        cancelRemoval();
        cancelMagicErase();
    });
    connect(this, &Document::layersChanged, this, [this] {
        cancelOperation();
        cancelSelectionOperation();
        invalidatePatch();
        invalidateContentMove();
        cancelRemoval();
        cancelMagicErase();
    });
    connect(this, &Document::layersChanged, this, &Document::historyBrushSourceChanged);
    connect(this, &Document::penPathsChanged, this, &Document::cancelOperation);
    connect(this, &Document::slicesChanged, this, &Document::cancelOperation);
    connect(this, &Document::textEditingChanged, this, &Document::cancelOperation);
    connect(this, &Document::textGeometryChanged, this, &Document::cancelOperation);
    connect(this, &Document::imageChanged, this, [this] {
        if (m_selectionCancel &&
            (m_drawing || m_previewingLayer || m_selectionTaskStateId != m_stateId))
            cancelSelectionOperation();
    });
    newDocument(1280, 800);
}
Document::~Document()
{
    cancelPlaneOffload();
    cancelOperation();
    cancelSelectionOperation();
    cancelRawLoad();
    cancelFrequencyRetouch();
    cancelMagicErase();
    if (m_removeCancel)
        m_removeCancel->store(true);
    if (m_contentMoveCancel)
        m_contentMoveCancel->store(true);
    if (m_patchCancel)
        m_patchCancel->store(true);
}
bool Document::fail(const QString &_message)
{
    emit errorOccurred(_message);
    return false;
}
bool Document::validSize(int _w, int _h, int _count)
{
    if (_w < 1 || _h < 1 || _w > 16384 || _h > 16384 || qint64(_w) * _h > MaxPixels)
        return fail(tr("Dimensions invalides : 1 à 16 384 px, 32 mégapixels maximum."));
    if (_count < 1 || _count > MaxLayers)
        return fail(tr("Limite atteinte : %1 calques par document (demandés : %2).")
                        .arg(MaxLayers)
                        .arg(_count));
    return true;
}
bool Document::validStorageBytes(qint64 _bytes)
{
    if (_bytes > DocumentStorage::MaxLayerBytes)
        return fail(tr("Limite mémoire atteinte : %1 Mio nécessaires, %2 Mio maximum pour les "
                       "pixels, sources et masques des calques.")
                        .arg((_bytes + 1024 * 1024 - 1) / (1024 * 1024))
                        .arg(DocumentStorage::MaxLayerBytes / (1024 * 1024)));
    return true;
}
bool Document::validLayerStorage(int _w, int _h, int _extraColors, int _extraMasks, int _bits)
{
    int colors = _extraColors, masks = _extraMasks;
    for (const auto &layer : std::as_const(m_layers)) {
        colors +=
            (!layer.image.isNull() && !ImageUtils::compact(layer.image)) + !layer.source.isNull();
        masks += !layer.mask.isNull() && !ImageUtils::compact(layer.mask);
    }
    return validStorageBytes(
        DocumentStorage::estimateBytes(_w, _h, colors, masks, _bits ? _bits : m_channelBits));
}
bool Document::editable(bool _materialize, bool _allowText)
{
    if (m_active < 0 || m_active >= m_layers.size())
        return false;
    if (m_layers[m_active].locked)
        return fail(tr("Ce calque est verrouillé."));
    if (!m_layers[m_active].groupId.isEmpty())
        for (const auto &layer : std::as_const(m_layers))
            if (layer.kind == QStringLiteral("group") &&
                layer.groupId == m_layers[m_active].groupId && (layer.locked || !layer.visible))
                return fail(layer.locked ? tr("Ce groupe est verrouillé.")
                                         : tr("Ce groupe est masqué."));
    if (!m_editingMask && m_layers[m_active].kind.startsWith(QStringLiteral("frequency")))
        return fail(tr("Choisissez « Préparer la retouche » dans les propriétés de la séparation "
                       "de fréquences pour modifier ses pixels."));
    if (m_layers[m_active].kind != QStringLiteral("pixel") && !m_editingMask &&
        !(_allowText && m_layers[m_active].kind == "text"))
        return fail(tr("Sélectionnez le masque de ce calque pour peindre dessus."));
    const auto &target = m_editingMask ? m_layers[m_active].mask : m_layers[m_active].image;
    if (_materialize && ImageUtils::compact(target) &&
        !validLayerStorage(width(), height(), m_editingMask ? 0 : 1, m_editingMask ? 1 : 0))
        return false;
    return true;
}
Document::Snapshot Document::snapshot(const QString &_label) const
{
    freezeLayers(m_layers);
    Snapshot state;
    state.layers               = m_layers;
    state.size                 = m_size;
    state.active               = m_active;
    state.selection            = m_selection;
    state.selectionRegion      = m_selectionRegion;
    state.selectionShape       = m_selectionShape;
    state.selectionPath        = m_selectionPath;
    state.selectedLayers       = m_selectedLayers;
    state.layerSelectionAnchor = m_layerSelectionAnchor;
    state.editingMask          = m_editingMask;
    state.id                   = m_stateId;
    state.label                = _label;
    state.slices               = m_slices;
    state.activeSlice          = m_activeSlice;
    state.channelBits          = m_channelBits;
    state.penPaths             = m_penPaths;
    state.activePenPath        = m_activePenPath;
    state.selectedPenPoints    = m_selectedPenPoints;
    state.penDrawing           = m_penDrawing;
    state.selectionMask        = m_selectionMask;
    state.lastSelectionMask    = m_lastSelectionMask;
    state.selectionOutline     = m_selectionOutline;
    state.lastSelectionOutline = m_lastSelectionOutline;
    state.contentId            = m_contentId;
    state.lastSelectionRegion  = m_lastSelectionRegion;
    return state;
}
void Document::restore(const Snapshot &_state)
{
    cancelSelectionOperation();
    m_selectionMask            = _state.selectionMask;
    m_lastSelectionMask        = _state.lastSelectionMask;
    m_selectionOutline         = _state.selectionOutline;
    m_lastSelectionOutline     = _state.lastSelectionOutline;
    m_selectionMaskRegion      = _state.selectionRegion;
    m_contentId                = _state.contentId;
    m_lastSelectionRegion      = _state.lastSelectionRegion;
    m_selectionGestureRecorded = false;
    m_beforeSelectionGesture   = {};
    if (_state.selectionOnly) {
        m_stateId            = _state.id;
        m_selection          = _state.selection;
        m_selectionRegion    = _state.selectionRegion;
        m_selectionShape     = _state.selectionShape;
        m_selectionPath      = _state.selectionPath;
        m_polygonLassoActive = false;
        m_activePenPath      = _state.activePenPath;
        m_selectedPenPoints  = _state.selectedPenPoints;
        m_penDrawing         = _state.penDrawing && m_tool == "pen";
        emit penPathsChanged();
        emit selectionChanged();
        emit historyChanged();
        return;
    }
    m_penPaths          = _state.penPaths;
    m_activePenPath     = _state.activePenPath;
    m_penGesture        = 0;
    m_penDrawing        = _state.penDrawing && m_tool == "pen";
    m_selectedPenPoints = _state.selectedPenPoints;
    emit penPathsChanged();
    if (_state.penOnly) {
        m_stateId = _state.id;
        emit historyChanged();
        return;
    }
    cancelCrop();
    const bool sizeChanged = m_size != _state.size;
    m_layers               = _state.layers;
    m_size                 = _state.size;
    m_channelBits          = _state.channelBits;
    m_active               = _state.active;
    m_slices               = _state.slices;
    m_activeSlice          = _state.activeSlice;
    m_sliceDragging        = false;
    m_sliceDraft           = {};
    emit slicesChanged();
    m_selection            = _state.selection;
    m_selectionRegion      = _state.selectionRegion;
    m_selectionShape       = _state.selectionShape;
    m_selectionPath        = _state.selectionPath;
    m_polygonLassoActive   = false;
    m_selectedLayers       = _state.selectedLayers;
    m_layerSelectionAnchor = _state.layerSelectionAnchor;
    m_editingMask          = _state.editingMask;
    m_visibilityBeforeIsolation.clear();
    m_isolatedLayer = -1;
    m_stateId       = _state.id;
    changed();
    if (sizeChanged)
        emit canvasSizeChanged();
    emit documentChanged();
    emit selectionChanged();
    emit historyChanged();
}
void Document::trimHistory()
{
    if (m_undo.size() > 40)
        m_undo.remove(0, m_undo.size() - 40);
    QSet<qint64>    seen;
    SelectionMemory selections;
    selections.add(m_selectionOutline);
    selections.add(m_lastSelectionOutline);
    selections.add(m_selectionRegion);
    selections.add(m_lastSelectionRegion);
    selections.add(m_selectionPath);
    const qint64 currentSelectionBytes = selections.bytes;
    for (const auto &layer : std::as_const(m_layers)) {
        const auto blocks = layerBlocks(layer);
        for (auto it = blocks.cbegin(); it != blocks.cend(); ++it)
            seen.insert(it.key());
    }
    for (const QImage *mask : {&m_selectionMask, &m_lastSelectionMask})
        if (!mask->isNull())
            seen.insert(mask->cacheKey());
    qint64     bytes = 0;
    const auto count = [&](const Snapshot &_state) {
        selections.add(_state.selectionOutline);
        selections.add(_state.lastSelectionOutline);
        selections.add(_state.selectionRegion);
        selections.add(_state.lastSelectionRegion);
        selections.add(_state.selectionPath);
        for (const QImage *mask : {&_state.selectionMask, &_state.lastSelectionMask})
            if (!mask->isNull() && !seen.contains(mask->cacheKey())) {
                seen.insert(mask->cacheKey());
                bytes += mask->sizeInBytes();
            }
        for (const auto &layer : _state.layers) {
            const auto blocks = layerBlocks(layer);
            for (auto it = blocks.cbegin(); it != blocks.cend(); ++it)
                if (!seen.contains(it.key())) {
                    seen.insert(it.key());
                    bytes += it.value();
                }
        }
    };
    count(m_historyBrushSource);
    // Current document pixels are not additional history storage. Walk from
    // newest to oldest once, retaining at least the most recent undo operation.
    for (int i = m_undo.size() - 1; i >= 0; --i) {
        count(m_undo[i]);
        if (bytes + selections.bytes - currentSelectionBytes > 256LL * 1024 * 1024 &&
            i < m_undo.size() - 1) {
            m_undo.remove(0, i + 1);
            break;
        }
    }
}
QVariantMap Document::memoryUsage() const
{
    QSet<qint64>    seen;
    SelectionMemory selections;
    selections.add(m_selectionOutline);
    selections.add(m_lastSelectionOutline);
    selections.add(m_selectionRegion);
    selections.add(m_lastSelectionRegion);
    selections.add(m_selectionPath);
    const qint64 selectionRegionBytes = selections.bytes;
    qint64       selectionBytes       = selections.bytes;
    qint64       retainedMaskBytes    = 0;
    for (const QImage *mask : {&m_selectionMask, &m_lastSelectionMask})
        if (!mask->isNull() && !seen.contains(mask->cacheKey())) {
            seen.insert(mask->cacheKey());
            selectionBytes += mask->sizeInBytes();
        }
    qint64                layerBytes = 0, historyBytes = 0;
    QHash<qint64, qint64> heapBlocks;
    int                   colors = 0, masks = 0, compactPlanes = 0;
    const auto            count = [&](const QVector<Layer> &_layers, qint64 &_bytes) {
        for (const auto &layer : _layers) {
            heapBlocks.insert(layer.image.heapBlocks());
            heapBlocks.insert(layer.mask.heapBlocks());
            heapBlocks.insert(layer.source.heapBlocks());
            const auto blocks = layerBlocks(layer);
            for (auto it = blocks.cbegin(); it != blocks.cend(); ++it)
                if (!seen.contains(it.key())) {
                    seen.insert(it.key());
                    _bytes += it.value();
                }
        }
    };
    count(m_layers, layerBytes);
    const auto countSelection = [&](const Snapshot &_state) {
        selections.add(_state.selectionOutline);
        selections.add(_state.lastSelectionOutline);
        selections.add(_state.selectionRegion);
        selections.add(_state.lastSelectionRegion);
        selections.add(_state.selectionPath);
        for (const QImage *mask : {&_state.selectionMask, &_state.lastSelectionMask})
            if (!mask->isNull() && !seen.contains(mask->cacheKey())) {
                seen.insert(mask->cacheKey());
                retainedMaskBytes += mask->sizeInBytes();
            }
    };
    for (const auto &state : m_undo) {
        count(state.layers, historyBytes);
        countSelection(state);
    }
    for (const auto &state : m_redo) {
        count(state.layers, historyBytes);
        countSelection(state);
    }
    count(m_historyBrushSource.layers, historyBytes);
    countSelection(m_historyBrushSource);
    const qint64 historySelectionBytes =
        selections.bytes - selectionRegionBytes + retainedMaskBytes;
    for (const auto &layer : m_layers) {
        colors +=
            (!layer.image.isNull() && !ImageUtils::compact(layer.image)) + !layer.source.isNull();
        masks += !layer.mask.isNull() && !ImageUtils::compact(layer.mask);
        compactPlanes += ImageUtils::compact(layer.image) + ImageUtils::compact(layer.mask);
    }
    qint64 tileHeapBytes = 0;
    for (auto bytes : heapBlocks)
        tileHeapBytes += bytes;
    qint64     retainedDecoderBytes = 0;
    const auto rawFuture            = m_rawWatcher.future();
    if (rawFuture.isValid() && rawFuture.isFinished() && rawFuture.resultCount() > 0)
        retainedDecoderBytes = rawFuture.result().image.sizeInBytes();
    return {{"tileHeapBytes", tileHeapBytes},
            {"retainedDecoderBytes", retainedDecoderBytes},
            {"layerCount", m_layers.size()},
            {"compactPlanes", compactPlanes},
            {"layerBytes", layerBytes},
            {"historyBytes", historyBytes + historySelectionBytes},
            {"selectionBytes", selectionBytes},
            {"historySelectionBytes", historySelectionBytes},
            {"reservedLayerBytes",
             DocumentStorage::estimateBytes(width(), height(), colors, masks, m_channelBits)},
            {"frequencyCacheBytes", qint64(m_frequencyCache.totalCost()) * 1024},
            {"strokeCoverageBytes", m_tiledStrokeCoverage.allocatedBytes()},
            {"strokeCoverageTiles", m_tiledStrokeCoverage.tileCount()}};
}
void Document::configureScratchCache(const QString &_directory, qint64 _maxBytes)
{
    cancelPlaneOffload();
    ScratchImageCache::Options options;
    options.directory = _directory;
    options.maxBytes  = qBound(qint64(0), _maxBytes, 64LL * 1024 * 1024 * 1024);
    m_frequencyCache.clear();
    m_scratchCache = std::make_shared<ScratchImageCache>(std::move(options));
    schedulePlaneOffload();
}
void Document::cancelPlaneOffload()
{
    if (m_planeOffloadCancel)
        m_planeOffloadCancel->store(true);
}
void Document::schedulePlaneOffload()
{
    if (m_scratchCache && !m_backgroundWorker)
        m_planeOffloadTimer.start();
}
QVariantMap Document::scratchCacheUsage() const
{
    return m_scratchCache ? m_scratchCache->statistics()
                          : QVariantMap{{"diskBytes", qint64(0)},
                                        {"mappedBytes", qint64(0)},
                                        {"entries", 0},
                                        {"hits", 0},
                                        {"misses", 0},
                                        {"budgetBytes", qint64(0)},
                                        {"directory", QString{}},
                                        {"error", QString{}}};
}
void Document::clearScratchCache()
{
    auto retired = std::make_shared<QVector<QImage>>();
    retired->reserve(m_frequencyCache.size());
    for (const QString &key : m_frequencyCache.keys()) {
        std::unique_ptr<QImage> image(m_frequencyCache.take(key));
        if (image)
            retired->append(std::move(*image));
    }
    const auto cache = m_scratchCache;
    (void)QtConcurrent::run([cache, retired] {
        if (cache)
            cache->clear();
        // Retired RAM entries may own file mappings; release those on this
        // worker too, keeping unlink/unmap work out of the settings button.
    });
}
void Document::commit(const Snapshot &_before, const QString &_label, bool _invalidateFull,
                      bool _notifyImage)
{
    Snapshot state = _before;
    state.label    = _label;
    m_undo.append(state);
    m_redo.clear();
    freezeLayers(m_layers);
    m_stateId = m_contentId = m_nextId++;
    trimHistory();
    changed(_invalidateFull, _notifyImage);
    schedulePlaneOffload();
    emit historyChanged();
}
void Document::edit(const QString &_label, const std::function<void()> &_operation)
{
    endStroke();
    auto before = snapshot(_label);
    _operation();
    if (m_backgroundWorker && m_backgroundCancel && m_backgroundCancel->load())
        return;
    commit(before, _label);
    if (before.size != m_size) {
        cancelCrop();
        m_slices.clear();
        m_activeSlice = -1;
        emit slicesChanged();
        emit canvasSizeChanged();
    }
}
void Document::changed(bool _metadata, bool _notifyImage)
{
    cancelOperation();
    m_gpuStackAuxTimer.stop();
    if (!m_preserveGpuTonePreview)
        invalidateGpuTonePreview();
    cancelMagicErase();
    invalidatePatch();
    invalidateContentMove();
    cancelRemoval();
    if (m_maskOnlyView &&
        (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].mask.isNull()))
        m_maskOnlyView = false;
    ++m_previewRequestGeneration;
    m_previewRenderPending = false;
    m_previewRenderActive.store(false);
    {
        QMutexLocker locker(&m_previewImageMutex);
        m_previewComposite = {};
    }
    m_cacheDirty = true;
    if (_metadata)
        m_fullCompositeDirty = true;
    ++m_revision;
    if (_metadata) {
        ++m_thumbnailRevision;
        emit thumbnailsChanged();
        emit layersChanged();
    }
    if (_notifyImage)
        emit imageChanged();
}

void Document::setPreviewSize(int _width, int _height)
{
    if (_width < 1 || _height < 1)
        return;
    QSize target(_width, _height);
    if (target.width() > 1280 || target.height() > 1280)
        target.scale(1280, 1280, Qt::KeepAspectRatio);
    if (target == m_previewTargetSize)
        return;
    m_previewTargetSize = target;
    if (m_previewingLayer &&
        (!m_gpuToneBase.isNull() || m_pendingPreviewGpuBase || m_runningPreviewGpuBase)) {
        invalidateGpuTonePreview();
        requestPreviewRender(true);
    }
}
void Document::resetHistory(bool _clean)
{
    cancelPlaneOffload();
    cancelSelectionOperation();
    m_selectionGestureRecorded = false;
    m_beforeSelectionGesture   = {};
    cancelColorSelection();
    m_selectionMask        = {};
    m_lastSelectionMask    = {};
    m_selectionOutline     = {};
    m_lastSelectionOutline = {};
    m_selectionMaskRegion  = {};
    m_penPaths.clear();
    m_activePenPath = -1;
    m_penDrawing    = false;
    m_penGesture    = 0;
    m_selectedPenPoints.clear();
    m_beforePenGesture = {};
    emit penPathsChanged();
    m_lastSelectionRegion = {};
    cancelRawLoad();
    cancelFrequencyRetouch();
    cancelMagicErase();
    cancelCrop();
    m_slices.clear();
    m_activeSlice   = -1;
    m_sliceDragging = false;
    m_sliceDraft    = {};
    emit slicesChanged();
    ++m_documentSession;
    if (m_scratchCache)
        m_scratchCache = std::make_shared<ScratchImageCache>(m_scratchCache->options());
    invalidatePatch();
    invalidateContentMove();
    cancelRemoval();
    m_undo.clear();
    m_redo.clear();
    m_stateId = m_contentId = m_nextId++;
    m_savedId               = _clean ? m_stateId : 0;
    m_selection             = {};
    m_selectionRegion       = {};
    m_selectionShape        = 0;
    m_selectionPath.clear();
    m_polygonLassoActive = false;
    selectOnly(m_active);
    m_drawing     = false;
    m_textEditing = m_textGesture = false;
    m_beforeText                  = {};
    emit textEditingChanged();
    m_editingMask             = false;
    m_previewingLayer         = false;
    m_frequencyPreviewDraft   = false;
    m_beforeStroke            = {};
    m_strokeCreatedLayer      = false;
    m_beforeLayerPreview      = {};
    m_strokeBase              = {};
    m_strokePlaneBase         = {};
    m_tiledStrokeCoverage     = {};
    m_strokeCoverageFailed    = false;
    m_strokeBounds            = {};
    m_stampSource             = {};
    m_smudgePickup            = {};
    m_smudgeMask              = {};
    m_smudgeSample            = {};
    m_mixerPickup             = {};
    m_mixerSample             = {};
    m_mixerTip                = {};
    m_mixerReservoirTip       = {};
    m_mixerPickupBeforeStroke = {};
    m_mixerLastEndValid       = false;
    m_healingSourceValid      = false;
    m_healingOffsetValid      = false;
    m_healingSample           = {};
    m_healingTip              = {};
    finishSpotHealingStroke();
    clearPatch();
    m_cloneSourceValid   = false;
    m_cloneOffsetValid   = false;
    m_patternOriginValid = false;
    m_frequencyCache.clear();
    m_fullCompositeDirty = true;
    m_dirtyRegion        = {};
    changed();
    emit canvasSizeChanged();
    emit documentChanged();
    emit selectionChanged();
    m_historyBrushSource      = snapshot(tr("État initial"));
    m_historyBrushSourceLabel = tr("État initial");
    m_historyStrokeSource     = {};
    emit historyBrushSourceChanged();
    emit historyChanged();
}
QVariantList Document::layers() const
{
    QVariantList  result;
    QSet<QString> collapsedGroups;
    for (const auto &layer : m_layers)
        if (layer.kind == QStringLiteral("group") && layer.groupCollapsed)
            collapsedGroups.insert(layer.groupId);
    // UI order is top to bottom, storage order is bottom to top.
    for (int i = int(m_layers.size()) - 1; i >= 0; --i) {
        const auto &l = m_layers[i];
        if (l.kind != QStringLiteral("group") && collapsedGroups.contains(l.groupId))
            continue;
        result.append(QVariantMap{{"layerIndex", i},
                                  {"name", l.name},
                                  {"visible", l.visible},
                                  {"locked", l.locked},
                                  {"opacity", l.opacity},
                                  {"blend", l.blend},
                                  {"kind", l.kind},
                                  {"adjustmentType", l.adjustmentType},
                                  {"parameters", l.parameters},
                                  {"hasMask", !l.mask.isNull()},
                                  {"maskEnabled", l.maskEnabled},
                                  {"clippingMask", l.clippingMask},
                                  {"groupId", l.groupId},
                                  {"groupCollapsed", l.groupCollapsed},
                                  {"selected", m_selectedLayers.contains(i)},
                                  {"editingMask", m_editingMask && i == m_active}});
    }
    return result;
}
QVariantList Document::histogram() const
{
    return m_histograms.luminance(displayComposite());
}
QVariantMap Document::colorHistogram() const
{
    return m_histograms.color(displayComposite());
}
QStringList Document::history() const
{
    QStringList result{tr("État initial")};
    for (const auto &state : m_undo)
        result.append(state.label);
    for (auto it = m_redo.crbegin(); it != m_redo.crend(); ++it)
        result.append(it->label);
    return result;
}
QImage Document::layerImage(int _index, const QSize &_requested) const
{
    if (_index < 0 || _index >= m_layers.size())
        return {};
    const auto &layer = m_layers[_index];
    if (layer.kind == QStringLiteral("group")) {
        QImage icon(96, 72, QImage::Format_ARGB32_Premultiplied);
        icon.fill(QColor("#232b32"));
        QPainter painter(&icon);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#c7a95a"));
        painter.drawRoundedRect(QRect(14, 24, 68, 38), 5, 5);
        painter.drawRoundedRect(QRect(18, 15, 30, 18), 4, 4);
        return icon;
    }
    if (layer.kind.startsWith(QStringLiteral("frequency"))) {
        if (!_requested.isValid())
            return frequencyImage(layer);
        // Thumbnails must never render native-resolution frequency bands.
        Layer       proxy = layer;
        const QSize size  = m_size.scaled(_requested, Qt::KeepAspectRatio);
        proxy.source = layer.source.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        proxy.parameters.insert("radius", qMax(1., layer.parameters.value("radius", 8).toDouble() *
                                                       size.width() / qMax(1, width())));
        Document renderer;
        renderer.m_historyBrushSource = {};
        return renderer.frequencyImage(proxy);
    }
    if (layer.kind == QStringLiteral("adjustment")) {
        QImage icon(96, 72, QImage::Format_ARGB32_Premultiplied);
        icon.fill(QColor("#232b32"));
        QPainter        p(&icon);
        QLinearGradient g(0, 72, 96, 0);
        g.setColorAt(0, QColor("#273845"));
        g.setColorAt(1, QColor("#c5f2cf"));
        p.fillRect(icon.rect().adjusted(8, 8, -8, -8), g);
        p.setPen(QColor("#ffffff"));
        p.drawText(icon.rect(), Qt::AlignCenter, QStringLiteral("A"));
        return icon;
    }
    const QSize size =
        _requested.isValid() ? m_size.scaled(_requested, Qt::KeepAspectRatio) : m_size;
    return ImageUtils::expandedPlane(layer.image, size);
}
QImage Document::maskImage(int _index, const QSize &_requested) const
{
    const QSize size =
        _requested.isValid() ? m_size.scaled(_requested, Qt::KeepAspectRatio) : m_size;
    return _index >= 0 && _index < m_layers.size()
               ? ImageUtils::expandedPlane(m_layers[_index].mask, size)
               : QImage{};
}
void Document::setActiveLayer(int _index)
{
    _index = finishTextForLayer(_index);
    if (_index < 0 || _index >= m_layers.size())
        return;
    endStroke();
    cancelLayerPreview();
    if (m_active != _index)
        m_maskOnlyView = false;
    m_active      = _index;
    m_editingMask = false;
    selectOnly(_index);
    emit layersChanged();
}
void Document::setDisplayChannel(int _channel)
{
    _channel = qBound(0, _channel, 3);
    if (m_displayChannel == _channel && !m_maskOnlyView)
        return;
    m_displayChannel          = _channel;
    m_maskOnlyView            = false;
    m_channelDisplayCache     = {};
    m_channelDisplaySourceKey = 0;
    emit displayChannelChanged();
    emit imageChanged();
}
void Document::toggleMaskOnlyView(int _index)
{
    if (_index < 0 || _index >= m_layers.size() || m_layers[_index].mask.isNull())
        return;
    const bool disable = m_maskOnlyView && m_active == _index;
    if (m_active != _index)
        setActiveLayer(_index);
    m_maskOnlyView = !disable;
    emit displayChannelChanged();
    emit imageChanged();
}
void Document::showCompositeView()
{
    if (!m_maskOnlyView)
        return;
    m_maskOnlyView = false;
    emit displayChannelChanged();
    emit imageChanged();
}
void Document::selectOnly(int _index)
{
    m_selectedLayers       = {_index};
    m_layerSelectionAnchor = _index;
}
int Document::selectedLayerCount() const
{
    return m_selectedLayers.size();
}
bool Document::canGroupSelected() const
{
    if (m_selectedLayers.size() < 2)
        return false;
    return std::all_of(m_selectedLayers.cbegin(), m_selectedLayers.cend(), [&](int _index) {
        return _index >= 0 && _index < m_layers.size() &&
               m_layers[_index].kind != QStringLiteral("group") &&
               m_layers[_index].groupId.isEmpty();
    });
}
bool Document::canUngroupActive() const
{
    return m_active >= 0 && m_active < m_layers.size() &&
           (m_layers[m_active].kind == QStringLiteral("group") ||
            !m_layers[m_active].groupId.isEmpty());
}
void Document::selectLayer(int _index, bool _toggle, bool _range)
{
    _index = finishTextForLayer(_index);
    if (_index < 0 || _index >= m_layers.size())
        return;
    endStroke();
    cancelLayerPreview();
    if (_range && m_layerSelectionAnchor >= 0 && m_layerSelectionAnchor < m_layers.size()) {
        m_selectedLayers.clear();
        const int first = qMin(_index, m_layerSelectionAnchor);
        const int last  = qMax(_index, m_layerSelectionAnchor);
        for (int i = first; i <= last; ++i)
            m_selectedLayers.insert(i);
    } else if (_toggle) {
        if (m_selectedLayers.contains(_index) && m_selectedLayers.size() > 1)
            m_selectedLayers.remove(_index);
        else
            m_selectedLayers.insert(_index);
        m_layerSelectionAnchor = _index;
    } else
        selectOnly(_index);
    m_active      = _index;
    m_editingMask = false;
    emit layersChanged();
}

void Document::requestChannelBits(int _bits)
{
    if ((_bits != 8 && _bits != 16) || _bits == m_channelBits) {
        setChannelBits(_bits);
        return;
    }
    if (!deferEdit(tr("Profondeur des couleurs"), editCost(true),
                   [_bits](Document &d) { d.setChannelBits(_bits); }))
        setChannelBits(_bits);
}

bool Document::setChannelBits(int _bits)
{
    if (_bits != 8 && _bits != 16)
        return fail(tr("La profondeur doit être de 8 ou 16 bits par canal."));
    if (_bits == m_channelBits)
        return true;
    if (!validLayerStorage(width(), height(), 0, 0, _bits))
        return false;
    endStroke();
    auto       layers = m_layers;
    const auto format =
        _bits == 16 ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied;
    PreparedPlaneBudget budget;
    int                 completed = 0;
    for (auto &layer : layers) {
        if (!operationStep(completed++, int(layers.size())) ||
            !prepareNativePlane(layer.image, format, budget) ||
            !prepareNativePlane(layer.source, format, budget))
            return false;
    }
    if (!operationStep(0, 1))
        return false;
    edit(tr("Profondeur des couleurs"), [&] {
        m_channelBits = _bits;
        m_layers      = std::move(layers);
    });
    return true;
}

bool Document::toolSupportsDepth(const QString &_tool) const
{
    if (m_channelBits == 8 || m_editingMask)
        return true;
    // These legacy algorithms still work in QRgb patches. Do not silently
    // truncate a high-precision image or interpret RGBA64 storage as QRgb.
    static const QStringList eightBitTools{"smudge",           "mixerBrush",     "healing",
                                           "spotHealing",      "remove",         "patch",
                                           "contentAwareMove", "redEye",         "colorReplacement",
                                           "historyBrush",     "artHistoryBrush"};
    return !eightBitTools.contains(_tool);
}
