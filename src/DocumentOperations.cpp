#include "Document.h"
#include "ImageUtils.h"
#include "storage/ScratchImageCache.h"
#include <QPointer>
#include <QPromise>
#include <QSignalBlocker>
#include <QThreadPool>
#include <limits>
#include <new>

struct Document::BackgroundOperation {
    struct Task {
        std::atomic_bool cancelled{false};
        std::atomic_int  progress{-1};
        // 0: cancellable, 1: final file publication claimed, 2: cancelled.
        std::atomic_int publication{0};
    };
    struct Result {
        Snapshot after{};
        QString  error;
        bool     valid = false, cancelled = false;
    };
    Snapshot                         before;
    QString                          label;
    bool                             busy    = false;
    quint64                          session = 0, state = 0;
    std::shared_ptr<Task>            task;
    QPointer<QFutureWatcher<Result>> watcher;
    QTimer                           progress;
};
bool Document::operationBusy() const
{
    return m_backgroundOperation && m_backgroundOperation->busy;
}
bool Document::operationCancellable() const
{
    return operationBusy() && m_backgroundOperation->task &&
           m_backgroundOperation->task->publication.load() == 0;
}
bool Document::beginOperationPublication()
{
    if (!m_backgroundWorker)
        return true;
    if (m_backgroundCancel && m_backgroundCancel->load())
        return false;
    if (!m_backgroundPublication)
        return true;
    int expected = 0;
    return m_backgroundPublication->compare_exchange_strong(expected, 1);
}
QString Document::operationLabel() const
{
    return operationBusy() ? m_backgroundOperation->label : QString{};
}
qreal Document::operationProgress() const
{
    if (!operationBusy() || !m_backgroundOperation->task)
        return -1;
    const int value = m_backgroundOperation->task->progress.load();
    return value < 0 ? -1 : value / 1000.;
}
void Document::setBackgroundEditThreshold(qint64 pixels)
{
    m_backgroundEditThreshold = pixels < 0 ? std::numeric_limits<qint64>::max() : pixels;
}
qint64 Document::editCost(bool allLayers, const QSize &target) const
{
    const QSize  size   = target.isValid() ? target : m_size;
    const qint64 pixels = qint64(size.width()) * size.height();
    if (!allLayers)
        return pixels;
    int planes = 0;
    for (const auto &layer : m_layers) {
        planes +=
            layer.kind == "text" || (!layer.image.isNull() && !ImageUtils::compact(layer.image));
        planes += !layer.mask.isNull() && !ImageUtils::compact(layer.mask);
        planes += !layer.source.isNull();
    }
    return pixels * planes;
}
bool Document::operationStep(int completed, int total)
{
    if (!m_backgroundWorker)
        return true;
    if (m_backgroundProgress && total > 1)
        m_backgroundProgress->store(qBound(0, int(qint64(completed) * 1000 / total), 1000));
    return !m_backgroundCancel || !m_backgroundCancel->load();
}
void Document::cancelOperation()
{
    if (!operationBusy())
        return;
    const auto state = m_backgroundOperation;
    if (state->task) {
        int expected = 0;
        if (!state->task->publication.compare_exchange_strong(expected, 2) && expected == 1)
            return; // A file already at its atomic publication point cannot be cancelled.
        state->task->cancelled.store(true);
    }
    state->busy = false;
    state->progress.stop();
    if (state->watcher)
        state->watcher->deleteLater();
    state->watcher.clear();
    state->task.reset();
    state->before = {};
    emit operationChanged();
    emit settingsChanged();
    emit statusMessage(tr("Opération annulée."));
}
bool Document::prepareNativePlane(PixelPlane &_plane, QImage::Format _format,
                                  PreparedPlaneBudget &_budget)
{
    const auto keepGoing = [this] { return operationStep(0, 1); };
    if (!keepGoing())
        return false;
    if (_plane.isNull())
        return true;
    if (!_plane.convertTo(_format, keepGoing)) {
        if (!keepGoing())
            return false;
        return fail(tr("Allocation des pixels impossible."));
    }
    if (!m_backgroundWorker || !m_scratchCache || !_budget.cacheAvailable ||
        m_scratchCache->options().maxBytes <= 0)
        return keepGoing();
    if (!_plane.freeze())
        return fail(tr("Allocation des pixels impossible."));
    const auto before = _plane.tiles().heapBlocks();
    for (auto it = before.cbegin(); it != before.cend(); ++it)
        if (!_budget.blocks.contains(it.key())) {
            _budget.blocks.insert(it.key(), it.value());
            _budget.heap += it.value();
        }
    if (!before.isEmpty() && _budget.heap > qint64(m_planeRamMiB) * 1024 * 1024) {
        const bool cached = _plane.offload(*m_scratchCache, keepGoing);
        const auto after  = _plane.tiles().heapBlocks();
        for (auto it = before.cbegin(); it != before.cend(); ++it)
            if (!after.contains(it.key()) && _budget.blocks.contains(it.key()))
                _budget.heap -= _budget.blocks.take(it.key());
        // Cache failure is non-fatal. Retain native pixels and stop repeated
        // write attempts for subsequent planes; the user can inspect the error.
        if (!cached)
            _budget.cacheAvailable = false;
    }
    return keepGoing();
}
bool Document::deferEdit(const QString &label, qint64 cost, std::function<void(Document &)> work,
                         bool mayRemoveMask)
{
    return deferOperation(label, cost, std::move(work), mayRemoveMask, {});
}
bool Document::deferOperation(const QString &_label, qint64 _cost,
                              std::function<void(Document &)> _work, bool _mayRemoveMask,
                              std::function<void()> _completed, bool _captureCurrent)
{
    if (m_backgroundWorker || _cost < m_backgroundEditThreshold)
        return false;
    if (pixelEditBusy()) {
        fail(tr("Terminez ou annulez l’opération en cours."));
        return true;
    }
    finishTextEdit();
    endStroke();
    finishLayerPreview();
    if (!m_backgroundOperation) {
        m_backgroundOperation = std::make_shared<BackgroundOperation>();
        m_backgroundOperation->progress.setInterval(100);
        connect(&m_backgroundOperation->progress, &QTimer::timeout, this, [this] {
            if (operationBusy())
                emit operationChanged();
        });
    }
    const auto state          = m_backgroundOperation;
    const bool captureCurrent = _captureCurrent || !_completed;
    state->before             = captureCurrent ? snapshot(_label) : Snapshot{};
    state->label              = _label;
    state->session            = m_documentSession;
    state->state              = m_stateId;
    state->task               = std::make_shared<BackgroundOperation::Task>();
    state->busy               = true;
    const auto   task         = state->task;
    const auto   before       = state->before;
    const auto   tool         = m_tool;
    const QColor color = m_color, secondary = m_secondary;
    const auto   scratch     = m_scratchCache;
    const int    planeRamMiB = m_planeRamMiB;
    const bool   readOnly    = bool(_completed);
    using Result             = BackgroundOperation::Result;
    auto promise             = std::make_shared<QPromise<Result>>();
    promise->start();
    auto *watcher  = new QFutureWatcher<Result>(this);
    state->watcher = watcher;
    connect(watcher, &QFutureWatcher<Result>::finished, this,
            [this, watcher, task, _completed = std::move(_completed)] {
                const auto state = m_backgroundOperation;
                if (!state || state->watcher != watcher) {
                    watcher->deleteLater();
                    return;
                }
                Result result;
                try {
                    result = watcher->result();
                } catch (...) {
                    result.error = tr("Mémoire insuffisante pour terminer l’opération.");
                }
                watcher->deleteLater();
                state->watcher.clear();
                state->progress.stop();
                const bool current =
                    state->busy && state->task == task && !task->cancelled.load() &&
                    state->session == m_documentSession && state->state == m_stateId;
                const auto before = std::move(state->before);
                state->busy       = false;
                state->task.reset();
                if (current && result.valid && !result.cancelled) {
                    if (_completed)
                        _completed();
                    else {
                        // restore() centralizes selection/path/size invariants, but its
                        // intermediate notifications must not expose the worker's IDs.
                        {
                            QSignalBlocker blocker(this);
                            const auto     isolation = m_visibilityBeforeIsolation;
                            const auto     isolated  = m_isolatedLayer;
                            restore(result.after);
                            if (isolation.size() == m_layers.size()) {
                                m_visibilityBeforeIsolation = isolation;
                                m_isolatedLayer             = isolated;
                            }
                        }
                        commit(before, state->label);
                        emit documentChanged();
                        if (before.size != m_size)
                            emit canvasSizeChanged();
                        emit selectionChanged();
                        emit slicesChanged();
                        emit penPathsChanged();
                    }
                } else if (current && !result.error.isEmpty())
                    fail(tr("Impossible de terminer « %1 » : %2").arg(state->label, result.error));
                emit operationChanged();
                emit settingsChanged();
            });
    watcher->setFuture(promise->future());
    state->progress.start();
    emit operationChanged();
    emit settingsChanged();
    // Lower priority than interactive preview jobs. Nothing on this worker
    // captures the live Document; closing/changing it never waits for this task.
    QThreadPool::globalInstance()->start(
        [promise, task, before, tool, color, secondary, scratch, planeRamMiB,
         _work = std::move(_work), _mayRemoveMask, readOnly, captureCurrent]() mutable {
            Result result;
            if (!task->cancelled.load()) {
                try {
                    Document renderer;
                    renderer.m_backgroundWorker = true;
                    renderer.m_backgroundCancel =
                        std::shared_ptr<std::atomic_bool>(task, &task->cancelled);
                    renderer.m_backgroundProgress =
                        std::shared_ptr<std::atomic_int>(task, &task->progress);
                    renderer.m_backgroundPublication =
                        std::shared_ptr<std::atomic_int>(task, &task->publication);
                    renderer.m_tool      = tool;
                    renderer.m_color     = color;
                    renderer.m_secondary = secondary;
                    if (captureCurrent)
                        renderer.restore(before);
                    renderer.m_historyBrushSource = {};
                    renderer.m_undo.clear();
                    renderer.m_redo.clear();
                    renderer.m_scratchCache = scratch;
                    renderer.m_planeRamMiB  = planeRamMiB;
                    QObject::connect(&renderer, &Document::errorOccurred, &renderer,
                                     [&result](const QString &message) { result.error = message; });
                    _work(renderer);
                    if (!task->cancelled.load() && result.error.isEmpty()) {
                        if (readOnly)
                            result.valid = true;
                        else {
                            result.after = renderer.snapshot({});
                            QHash<QString, const Layer *> originals;
                            for (const auto &layer : before.layers)
                                originals.insert(layer.historyId, &layer);
                            for (const auto &layer : result.after.layers) {
                                const auto *original = originals.value(layer.historyId);
                                if (((layer.kind == "pixel" || layer.kind == "text" ||
                                      layer.kind == "shape") &&
                                     layer.image.isNull()) ||
                                    (layer.kind.startsWith("frequency") && layer.source.isNull()) ||
                                    (!_mayRemoveMask && original && !original->mask.isNull() &&
                                     layer.mask.isNull())) {
                                    result.error = tr("Allocation des pixels impossible.");
                                    break;
                                }
                            }
                            result.valid = result.error.isEmpty();
                        }
                    }
                } catch (const std::bad_alloc &) {
                    result.error = tr("Mémoire insuffisante pour terminer l’opération.");
                } catch (...) {
                    result.error = tr("Le calcul de l’opération a échoué.");
                }
            }
            result.cancelled = task->cancelled.load();
            promise->addResult(std::move(result));
            promise->finish();
        },
        -1);
    return true;
}
