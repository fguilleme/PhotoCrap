#include "Document.h"
#include "ImageUtils.h"

#include <QtConcurrent>
#include <QtMath>
#include <algorithm>
#include <array>
#include <utility>

namespace
{
struct EraseResult {
    QImage image;
    QRect  dirty;
};

std::array<int, 4> channels(QRgb _pixel)
{
    _pixel = qUnpremultiply(_pixel);
    return {qRed(_pixel) * 257, qGreen(_pixel) * 257, qBlue(_pixel) * 257, qAlpha(_pixel) * 257};
}
std::array<int, 4> channels(QRgba64 _pixel)
{
    _pixel = _pixel.unpremultiplied();
    return {_pixel.red(), _pixel.green(), _pixel.blue(), _pixel.alpha()};
}
QRgb faded(QRgb _pixel, qreal _keep)
{
    return qRgba(qRound(qRed(_pixel) * _keep), qRound(qGreen(_pixel) * _keep),
                 qRound(qBlue(_pixel) * _keep), qRound(qAlpha(_pixel) * _keep));
}
QRgba64 faded(QRgba64 _pixel, qreal _keep)
{
    return QRgba64::fromRgba64(
        quint16(qRound(_pixel.red() * _keep)), quint16(qRound(_pixel.green() * _keep)),
        quint16(qRound(_pixel.blue() * _keep)), quint16(qRound(_pixel.alpha() * _keep)));
}

template <typename Pixel>
EraseResult eraseColors(const QImage &_target, const QImage &_sample, const QSize &_size,
                        const QPoint &_seed, const QRegion &_selection, int _tolerance,
                        bool _contiguous, bool _antialias, qreal _opacity,
                        const std::shared_ptr<std::atomic_bool> &_cancel)
{
    const bool  selected = !_selection.isEmpty();
    const QRect document(QPoint{}, _size);
    const QRect limit = selected ? _selection.boundingRect().intersected(document) : document;
    if (!limit.contains(_seed) || (selected && !_selection.contains(_seed)) || _cancel->load())
        return {};
    // Match straight RGB at native precision, but erase premultiplied channels
    // together. Alpha is not a colour tolerance; transparent samples are skipped.
    const QImage sample =
        ImageUtils::expandedPlane(_sample, _size)
            .convertToFormat(sizeof(Pixel) == 8 ? QImage::Format_RGBA64_Premultiplied
                                                : QImage::Format_ARGB32_Premultiplied);
    if (sample.isNull() || _cancel->load())
        return {};
    const auto reference =
        channels(reinterpret_cast<const Pixel *>(sample.constScanLine(_seed.y()))[_seed.x()]);
    if (!reference[3])
        return {};
    QImage mask(limit.size(), QImage::Format_Grayscale8);
    if (mask.isNull())
        return {};
    mask.fill(0);
    const auto marked = [&](int _x, int _y) {
        return mask.constScanLine(_y - limit.top())[_x - limit.left()] != 0;
    };
    const auto matches = [&](int _x, int _y) {
        if (selected && !_selection.contains(QPoint(_x, _y)))
            return false;
        const auto pixel = channels(reinterpret_cast<const Pixel *>(sample.constScanLine(_y))[_x]);
        return pixel[3] && qAbs(pixel[0] - reference[0]) <= _tolerance * 257 &&
               qAbs(pixel[1] - reference[1]) <= _tolerance * 257 &&
               qAbs(pixel[2] - reference[2]) <= _tolerance * 257;
    };
    QRect      bounds;
    const auto markRun = [&](int _left, int _right, int _y) {
        auto *row = mask.scanLine(_y - limit.top());
        std::fill(row + _left - limit.left(), row + _right - limit.left() + 1, uchar(255));
        const QRect run(_left, _y, _right - _left + 1, 1);
        bounds |= run;
    };
    if (!_contiguous) {
        for (int y = limit.top(); y <= limit.bottom(); ++y) {
            if (_cancel->load())
                return {};
            int x = limit.left();
            while (x <= limit.right()) {
                while (x <= limit.right() && !matches(x, y))
                    ++x;
                const int left = x;
                while (x <= limit.right() && matches(x, y))
                    ++x;
                if (left < x)
                    markRun(left, x - 1, y);
            }
        }
    } else {
        QVector<QPoint> pending{_seed};
        while (!pending.isEmpty()) {
            if (_cancel->load())
                return {};
            const QPoint seed = pending.takeLast();
            if (marked(seed.x(), seed.y()) || !matches(seed.x(), seed.y()))
                continue;
            int left = seed.x(), right = seed.x();
            while (left > limit.left() && !marked(left - 1, seed.y()) &&
                   matches(left - 1, seed.y()))
                --left;
            while (right < limit.right() && !marked(right + 1, seed.y()) &&
                   matches(right + 1, seed.y()))
                ++right;
            markRun(left, right, seed.y());
            for (const int y : {seed.y() - 1, seed.y() + 1}) {
                if (y < limit.top() || y > limit.bottom())
                    continue;
                bool inRun = false;
                for (int x = left; x <= right; ++x) {
                    const bool match = !marked(x, y) && matches(x, y);
                    if (match && !inRun)
                        pending.append(QPoint(x, y));
                    inRun = match;
                }
            }
        }
    }
    if (bounds.isEmpty() || _cancel->load())
        return {};
    QImage result = ImageUtils::expandedPlane(_target, _size);
    result.detach(); // Full-plane copying stays on the worker, never on the UI thread.
    if (result.isNull())
        return {};
    QRect dirty;
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        if (_cancel->load())
            return {};
        auto *row = reinterpret_cast<Pixel *>(result.scanLine(y));
        for (int x = bounds.left(); x <= bounds.right(); ++x) {
            if (!marked(x, y))
                continue;
            qreal coverage = 1;
            if (_antialias) {
                // A small coverage filter softens the colour boundary. It never
                // touches nonmatching colours or feathers the selection itself.
                int sum = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const QPoint next(x + dx, y + dy);
                        const bool   outside =
                            !limit.contains(next) || (selected && !_selection.contains(next));
                        const int weight = (dx == 0 ? 2 : 1) * (dy == 0 ? 2 : 1);
                        if (outside || marked(next.x(), next.y()))
                            sum += weight;
                    }
                coverage = sum / 16.;
            }
            const Pixel erased = faded(row[x], 1. - _opacity * coverage);
            if (erased != row[x]) {
                row[x] = erased;
                dirty |= QRect(x, y, 1, 1);
            }
        }
    }
    return dirty.isEmpty() ? EraseResult{} : EraseResult{result, dirty};
}
} // namespace

void Document::setMagicEraserTolerance(int _value)
{
    _value = qBound(0, _value, 255);
    if (m_magicEraserTolerance == _value)
        return;
    m_magicEraserTolerance = _value;
    emit settingsChanged();
}
void Document::setMagicEraserContiguous(bool _value)
{
    if (m_magicEraserContiguous == _value)
        return;
    m_magicEraserContiguous = _value;
    emit settingsChanged();
}
void Document::setMagicEraserAntialias(bool _value)
{
    if (m_magicEraserAntialias == _value)
        return;
    m_magicEraserAntialias = _value;
    emit settingsChanged();
}
void Document::setMagicEraserSampleAllLayers(bool _value)
{
    if (m_magicEraserSampleAllLayers == _value)
        return;
    m_magicEraserSampleAllLayers = _value;
    emit settingsChanged();
}
void Document::cancelMagicErase()
{
    if (m_magicEraserCancel)
        m_magicEraserCancel->store(true);
    m_magicEraserCancel.reset();
    if (m_magicEraserBusy) {
        m_magicEraserBusy = false;
        emit settingsChanged();
    }
}
void Document::applyMagicEraser(const QPoint &_seed)
{
    if (m_magicEraserBusy || !QRect(QPoint{}, m_size).contains(_seed) || !editable())
        return;
    if (m_editingMask) {
        fail(tr("La gomme magique efface les pixels du calque. Utilisez le pinceau ou la gomme "
                "pour modifier un masque."));
        return;
    }
    const QImage target = m_layers[m_active].image;
    if (target.isNull() || (ImageUtils::compact(target) && !target.pixelColor(0, 0).alpha()))
        return;
    const Snapshot       before     = snapshot(tr("Gomme magique"));
    const quint64        session    = m_documentSession;
    const QRegion        selection  = m_selectionRegion;
    const int            tolerance  = m_magicEraserTolerance;
    const bool           contiguous = m_magicEraserContiguous, antialias = m_magicEraserAntialias;
    const qreal          opacity = m_brushOpacity;
    const QSize          size    = m_size;
    const QVector<Layer> layers  = m_magicEraserSampleAllLayers ? m_layers : QVector<Layer>{};
    auto                 cancel  = std::make_shared<std::atomic_bool>(false);
    m_magicEraserCancel          = cancel;
    m_magicEraserBusy            = true;
    emit  settingsChanged();
    auto *watcher = new QFutureWatcher<EraseResult>(this);
    connect(watcher, &QFutureWatcher<EraseResult>::finished, this,
            [this, watcher, before, session, cancel] {
                const EraseResult result = watcher->result();
                watcher->deleteLater();
                if (m_magicEraserCancel != cancel)
                    return;
                cancelMagicErase();
                if (session != m_documentSession || before.id != m_stateId ||
                    m_active != before.active || m_editingMask || m_tool != "magicEraser" ||
                    result.image.isNull())
                    return;
                QImage image = result.image;
                SelectionCoverage::blend(image, before.layers[m_active].image, before.selectionMask,
                                         result.dirty);
                m_layers[m_active].image = image;
                m_dirtyRegion            = m_dirtyRegion.united(result.dirty);
                commit(before, tr("Gomme magique"), false, false);
                ++m_thumbnailRevision;
                emit thumbnailsChanged();
                requestPreviewRender(true, true);
            });
    watcher->setFuture(QtConcurrent::run([target, layers, size, selection, _seed, tolerance,
                                          contiguous, antialias, opacity, cancel] {
        if (cancel->load())
            return EraseResult{};
        const QImage sample =
            layers.isEmpty() ? target : renderPreviewSnapshot(layers, size, size, false);
        if (ImageUtils::is16Bit(target))
            return eraseColors<QRgba64>(target, sample, size, _seed, selection, tolerance,
                                        contiguous, antialias, opacity, cancel);
        return eraseColors<QRgb>(target, sample, size, _seed, selection, tolerance, contiguous,
                                 antialias, opacity, cancel);
    }));
}
