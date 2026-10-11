#include "Document.h"
#include "ImageUtils.h"

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
constexpr qint64 MaxPixels        = 32LL * 1024 * 1024;
constexpr qint64 MaxDocumentBytes = 512LL * 1024 * 1024;
constexpr int    MaxLayers        = 64;
} // namespace

Document::Document(QObject *_parent) : QObject(_parent)
{
    connect(&m_previewRenderWatcher, &QFutureWatcher<QImage>::finished, this, [this] {
        const QImage result  = m_previewRenderWatcher.result();
        const bool   current = m_runningPreviewGeneration == m_previewRequestGeneration;
        if (current && !result.isNull()) {
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
            ++m_revision;
            emit imageChanged();
        }
        if (m_previewRenderPending)
            QTimer::singleShot(0, this, &Document::startPendingPreviewRender);
        else if (current && m_runningPreviewDraft && m_runningPreviewRefine)
            requestPreviewRender(false);
        else
            m_previewRenderActive.store(false);
    });
    newDocument(1280, 800);
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
    if (_count < 1 || _count > MaxLayers || qint64(_w) * _h * 4 * _count > MaxDocumentBytes)
        return fail(tr("Limite atteinte : 64 calques et 512 Mio de pixels par document."));
    return true;
}
bool Document::editable()
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
    if (m_layers[m_active].kind != QStringLiteral("pixel") && !m_editingMask)
        return fail(tr("Sélectionnez le masque de ce calque pour peindre dessus."));
    return true;
}
Document::Snapshot Document::snapshot(const QString &_label) const
{
    return {m_layers,
            m_size,
            m_active,
            m_selection,
            m_selectionRegion,
            m_selectionShape,
            m_selectionPath,
            m_selectedLayers,
            m_layerSelectionAnchor,
            m_editingMask,
            m_stateId,
            _label};
}
void Document::restore(const Snapshot &_state)
{
    const bool sizeChanged = m_size != _state.size;
    m_layers               = _state.layers;
    m_size                 = _state.size;
    m_active               = _state.active;
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
    // QImage snapshots share their storage until an edit detaches a layer.
    // Count unique storage, not theoretical copies, to bound retained history.
    while (m_undo.size() > 1) {
        QSet<qint64> seen;
        qint64       bytes = 0;
        for (const auto &state : m_undo)
            for (const auto &layer : state.layers) {
                for (const QImage *image : {&layer.image, &layer.mask, &layer.source})
                    if (!image->isNull() && !seen.contains(image->cacheKey())) {
                        seen.insert(image->cacheKey());
                        bytes += image->sizeInBytes();
                    }
            }
        if (m_undo.size() <= 40 && bytes <= 256LL * 1024 * 1024)
            break;
        m_undo.removeFirst();
    }
}
void Document::commit(const Snapshot &_before, const QString &_label, bool _invalidateFull,
                      bool _notifyImage)
{
    Snapshot state = _before;
    state.label    = _label;
    m_undo.append(state);
    m_redo.clear();
    m_stateId = m_nextId++;
    trimHistory();
    changed(_invalidateFull, _notifyImage);
    emit historyChanged();
}
void Document::edit(const QString &_label, const std::function<void()> &_operation)
{
    endStroke();
    auto before = snapshot(_label);
    _operation();
    commit(before, _label);
    if (before.size != m_size)
        emit canvasSizeChanged();
}
void Document::changed(bool _metadata, bool _notifyImage)
{
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
    m_previewTargetSize = target;
}
void Document::resetHistory(bool _clean)
{
    m_undo.clear();
    m_redo.clear();
    m_stateId         = m_nextId++;
    m_savedId         = _clean ? m_stateId : 0;
    m_selection       = {};
    m_selectionRegion = {};
    m_selectionShape  = 0;
    m_selectionPath.clear();
    m_polygonLassoActive = false;
    selectOnly(m_active);
    m_drawing               = false;
    m_editingMask           = false;
    m_previewingLayer       = false;
    m_frequencyPreviewDraft = false;
    m_beforeStroke          = {};
    m_beforeLayerPreview    = {};
    m_strokeBase            = {};
    m_strokeCoverage        = {};
    m_strokeBounds          = {};
    m_stampSource           = {};
    m_cloneSourceValid      = false;
    m_cloneOffsetValid      = false;
    m_patternOriginValid    = false;
    m_frequencyCache.clear();
    m_fullCompositeDirty = true;
    m_dirtyRegion        = {};
    changed();
    emit canvasSizeChanged();
    emit documentChanged();
    emit selectionChanged();
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
    const QImage     image = composite().convertToFormat(QImage::Format_ARGB32);
    QVector<quint64> bins(256, 0);
    const qint64     pixels  = qint64(image.width()) * image.height();
    const int        stride  = qMax(1, int(std::sqrt(double(pixels) / 262144.)));
    quint64          maximum = 0;
    for (int y = 0; y < image.height(); y += stride) {
        const auto row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); x += stride) {
            const QRgb pixel     = row[x];
            const int  luminance = qBound(
                0, qRound(.2126 * qRed(pixel) + .7152 * qGreen(pixel) + .0722 * qBlue(pixel)), 255);
            bins[luminance] += quint64(qAlpha(pixel));
            maximum = qMax(maximum, bins[luminance]);
        }
    }
    QVariantList result;
    result.reserve(256);
    for (const quint64 count : std::as_const(bins))
        result.append(maximum == 0 ? 0. : double(count) / double(maximum));
    return result;
}
QVariantMap Document::colorHistogram() const
{
    const QImage image = displayComposite().convertToFormat(QImage::Format_ARGB32);
    std::array<QVector<quint64>, 4> bins;
    for (auto &channel : bins)
        channel.fill(0, 256);
    const qint64 pixels  = qint64(image.width()) * image.height();
    const int    stride  = qMax(1, int(std::sqrt(double(pixels) / 65536.)));
    quint64      maximum = 0;
    for (int y = 0; y < image.height(); y += stride) {
        const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); x += stride) {
            const QRgb pixel     = row[x];
            const auto alpha     = quint64(qAlpha(pixel));
            const int  luminance = qBound(
                0, qRound(.2126 * qRed(pixel) + .7152 * qGreen(pixel) + .0722 * qBlue(pixel)), 255);
            bins[0][qRed(pixel)] += alpha;
            bins[1][qGreen(pixel)] += alpha;
            bins[2][qBlue(pixel)] += alpha;
            bins[3][luminance] += alpha;
            maximum = qMax(maximum, qMax(bins[3][luminance],
                                         qMax(bins[0][qRed(pixel)], qMax(bins[1][qGreen(pixel)],
                                                                         bins[2][qBlue(pixel)]))));
        }
    }
    const auto normalized = [&](const QVector<quint64> &_bins) {
        QVariantList values;
        values.reserve(256);
        for (const quint64 count : _bins)
            values.append(maximum == 0 ? 0. : double(count) / double(maximum));
        return values;
    };
    return {{QStringLiteral("red"), normalized(bins[0])},
            {QStringLiteral("green"), normalized(bins[1])},
            {QStringLiteral("blue"), normalized(bins[2])},
            {QStringLiteral("luminance"), normalized(bins[3])}};
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
QImage Document::layerImage(int _index) const
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
    if (layer.kind.startsWith(QStringLiteral("frequency")))
        return frequencyImage(layer);
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
    return layer.image;
}
QImage Document::maskImage(int _index) const
{
    return _index >= 0 && _index < m_layers.size() ? m_layers[_index].mask : QImage{};
}
void Document::setActiveLayer(int _index)
{
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
