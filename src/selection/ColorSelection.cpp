#include "Document.h"
#include "ImageUtils.h"
#include <QPainter>
#include <QtConcurrent>

QImage Document::selectionMask() const
{
    return m_selectionMask.isNull() ? SelectionCoverage::raster(m_selectionRegion, m_size)
                                    : m_selectionMask;
}
int Document::selectionCoverage(int _x, int _y) const
{
    if (!hasSelection())
        return 255;
    if (!m_selectionMask.isNull())
        return m_selectionMask.rect().contains(_x, _y) ? m_selectionMask.constScanLine(_y)[_x] : 0;
    return m_selectionRegion.contains(QPoint(_x, _y)) ? 255 : 0;
}
void Document::applySelectionCoverage(const SelectionCoverage::Result &_result)
{
    m_selectionMask       = _result.support.isEmpty() ? QImage{} : _result.mask;
    m_selectionRegion     = _result.support;
    m_selectionMaskRegion = _result.support;
    m_selectionOutline    = _result.outline;
    m_selection           = _result.support.boundingRect();
    m_selectionShape      = _result.support.isEmpty() ? 0 : 6;
    m_selectionPath.clear();
    m_polygonLassoActive = false;
}
void Document::blendSelectionEdit(QImage &_image, const QImage &_before)
{
    SelectionCoverage::blend(_image, _before, m_selectionMask, m_selection);
}
void Document::translateSelectionCoverage(const Snapshot &_before, const QPoint &_offset)
{
    if (_before.selectionMask.isNull())
        return;
    QImage mask(m_size, QImage::Format_Grayscale8);
    mask.fill(0);
    QPainter p(&mask);
    p.drawImage(_offset, _before.selectionMask);
    p.end();
    m_selectionMask       = mask;
    m_selectionMaskRegion = m_selectionRegion;
    m_selectionOutline =
        _before.selectionOutline.translated(_offset) & QRegion(QRect(QPoint{}, m_size));
}
void Document::combineSoftSelection(const QRegion &_candidate, int _mode, const Snapshot &_before)
{
    if (_mode == 0 || _before.selectionMask.isNull()) {
        m_selectionMask = {};
        return;
    }
    QImage   mask = _before.selectionMask;
    QPainter p(&mask);
    p.setClipRegion(_mode == 3 ? QRegion(QRect(QPoint{}, m_size)).subtracted(_candidate)
                               : _candidate);
    p.fillRect(mask.rect(), _mode == 1 ? Qt::white : Qt::black);
    p.end();
    m_selectionMask       = mask;
    m_selectionMaskRegion = m_selectionRegion;
    m_selectionOutline    = _mode == 1   ? _before.selectionOutline.united(_candidate)
                            : _mode == 2 ? _before.selectionOutline.subtracted(_candidate)
                                         : _before.selectionOutline.intersected(_candidate);
}
QImage Document::colorSelectionPreview(bool _mask, const QRect &_region,
                                       const QSize &_requested) const
{
    if (!_region.isValid())
        return _mask ? m_colorSelectionMaskPreview : m_colorSelectionSourcePreview;
    const auto &source = _mask ? m_colorSelectionResult.mask : m_colorSelectionSource;
    const QRect region = _region.intersected(source.rect());
    if (region.isEmpty())
        return {};
    const auto &proxy = _mask ? m_colorSelectionMaskPreview : m_colorSelectionSourcePreview;
    if (region == source.rect() && proxy.width() < source.width() &&
        proxy.height() < source.height() && !_requested.isEmpty() &&
        _requested.width() <= proxy.width() && _requested.height() <= proxy.height())
        return proxy;
    const QImage crop = source.copy(region);
    return _requested.isValid()
               ? crop.scaled(_requested, Qt::KeepAspectRatio, Qt::SmoothTransformation)
               : crop;
}
void Document::beginColorSelection()
{
    endStroke();
    cancelColorSelection();
    cancelSelectionOperation();
    m_beforeColorSelection  = snapshot({});
    m_colorSelectionActive  = true;
    m_colorSelectionOptions = {{"fuzziness", 40.}, {"type", "sampled"}, {"range", 100.}};
    m_colorSamples.clear();
    m_colorSelectionSource        = {};
    m_colorSelectionSourcePreview = {};
    m_colorSelectionMaskPreview   = {};
    m_colorSelectionResult        = {};
    const auto layers             = m_layers;
    const auto size               = m_size;
    const auto session            = m_documentSession;
    auto       cancel             = std::make_shared<std::atomic_bool>(false);
    m_selectionCancel             = cancel;
    m_selectionTaskStateId        = m_stateId;
    emit selectionOperationChanged();
    emit colorSelectionChanged();
    auto watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, cancel, session] {
        const auto image = watcher->result();
        watcher->deleteLater();
        if (m_selectionCancel != cancel || !m_colorSelectionActive || session != m_documentSession)
            return;
        cancelSelectionOperation();
        if (image.isNull()) {
            cancelColorSelection();
            fail(tr("Impossible de préparer la sélection par couleur."));
            return;
        }
        m_colorSelectionSource = image;
        m_colorSelectionSourcePreview =
            image.scaled(QSize(900, 650), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_colorSamples.append({image.pixelColor(image.width() / 2, image.height() / 2),
                               QPointF(image.width() / 2, image.height() / 2), false});
        ++m_colorSelectionRevision;
        emit colorSelectionChanged();
        previewColorSelection(m_colorSelectionOptions);
    });
    watcher->setFuture(QtConcurrent::run([layers, size, cancel] {
        return cancel->load() ? QImage{} : renderPreviewSnapshot(layers, size, size, false);
    }));
}
void Document::previewColorSelection(const QVariantMap &_options)
{
    // Convert the options on the GUI thread; workers never retain QML values.
    m_colorSelectionOptions = {{"type", _options.value("type", "sampled").toString()},
                               {"fuzziness", _options.value("fuzziness", 40.).toDouble()},
                               {"localized", _options.value("localized").toBool()},
                               {"contiguous", _options.value("contiguous").toBool()},
                               {"range", _options.value("range", 100.).toDouble()},
                               {"invert", _options.value("invert").toBool()}};
    if (!m_colorSelectionActive || m_colorSelectionSource.isNull())
        return;
    cancelSelectionOperation();
    m_colorSelectionResult.valid = false;
    auto cancel                  = std::make_shared<std::atomic_bool>(false);
    m_selectionCancel            = cancel;
    m_selectionTaskStateId       = m_stateId;
    const auto source            = m_colorSelectionSource;
    const auto samples           = m_colorSamples;
    const auto options           = m_colorSelectionOptions;
    emit       selectionOperationChanged();
    emit       colorSelectionChanged();
    auto       watcher = new QFutureWatcher<SelectionCoverage::Result>(this);
    connect(watcher, &QFutureWatcher<SelectionCoverage::Result>::finished, this,
            [this, watcher, cancel] {
                const auto result = watcher->result();
                watcher->deleteLater();
                if (m_selectionCancel != cancel || !m_colorSelectionActive)
                    return;
                cancelSelectionOperation();
                m_colorSelectionResult = result;
                if (result.valid)
                    m_colorSelectionMaskPreview = result.mask.scaled(
                        QSize(900, 650), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                else if (!result.error.isEmpty())
                    fail(tr("Impossible de calculer cette sélection par couleur."));
                ++m_colorSelectionRevision;
                emit colorSelectionChanged();
            });
    watcher->setFuture(QtConcurrent::run([source, samples, options, cancel] {
        return SelectionCoverage::colors(source, samples, options, cancel);
    }));
}
void Document::sampleSelectionColor(int _x, int _y, int _mode, const QVariantMap &_options)
{
    if (!m_colorSelectionActive || !m_colorSelectionSource.rect().contains(_x, _y))
        return;
    if (_mode == 0)
        m_colorSamples.clear();
    if (m_colorSamples.size() >= 64) {
        emit statusMessage(tr("Maximum : 64 échantillons de couleur."));
        return;
    }
    m_colorSamples.append({m_colorSelectionSource.pixelColor(_x, _y), QPointF(_x, _y), _mode == 2});
    emit colorSelectionChanged();
    previewColorSelection(_options.isEmpty() ? m_colorSelectionOptions : _options);
}
void Document::acceptColorSelection()
{
    if (!colorSelectionReady() || !m_colorSelectionActive)
        return;
    if (m_colorSelectionResult.support.isEmpty()) {
        cancelColorSelection();
        emit statusMessage(tr("Aucun pixel ne correspond aux couleurs choisies."));
        return;
    }
    const auto before = m_beforeColorSelection;
    const auto result = m_colorSelectionResult;
    if (before.id != m_stateId) {
        cancelColorSelection();
        return;
    }
    cancelColorSelection();
    applySelectionCoverage(result);
    commitSelectionEdit(before, tr("Sélection par plage de couleurs"));
    emit selectionChanged();
}
void Document::cancelColorSelection()
{
    if (!m_colorSelectionActive)
        return;
    cancelSelectionOperation();
    m_colorSelectionActive        = false;
    m_beforeColorSelection        = {};
    m_colorSelectionResult        = {};
    m_colorSelectionSource        = {};
    m_colorSelectionSourcePreview = {};
    m_colorSelectionMaskPreview   = {};
    m_colorSamples.clear();
    emit colorSelectionChanged();
}
