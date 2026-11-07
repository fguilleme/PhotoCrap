#include "Document.h"
#include "ImageUtils.h"
#include "editing/ImageClipboard.h"
#include <QColorSpace>
#include <QPainter>
#include <utility>

using PhotoCrap::Clipboard::Pixels;

bool Document::canPaste() const
{
    return PhotoCrap::Clipboard::hasImage();
}
QSize Document::clipboardImageSize() const
{
    return PhotoCrap::Clipboard::read().image.size();
}
QVariantMap Document::clipboardInfo() const
{
    const auto pixels = PhotoCrap::Clipboard::read();
    return {{"width", pixels.image.width()},
            {"height", pixels.image.height()},
            {"bits", ImageUtils::is16Bit(pixels.image) ? 16 : 8}};
}

void Document::beginClipboardTransfer(std::shared_ptr<Pixels> _pixels, bool _mutating)
{
    m_clipboardTransferPending   = true;
    m_clipboardTransferPublished = false;
    m_clipboardTransferMutating  = _mutating;
    m_clipboardTransferPixels    = std::move(_pixels);
    m_clipboardTransferSession   = m_documentSession;
    m_clipboardTransferState     = m_stateId;
    m_queuedClipboardPaste.clear();
    m_clipboardFinishConnection =
        connect(this, &Document::operationChanged, this, &Document::finishClipboardTransfer);
    emit clipboardChanged();
}
void Document::finishClipboardTransfer()
{
    if (!m_clipboardTransferPending || operationBusy())
        return;
    disconnect(m_clipboardFinishConnection);
    const bool ready = m_clipboardTransferSession == m_documentSession &&
                       (m_clipboardTransferMutating ? m_stateId != m_clipboardTransferState
                                                    : m_clipboardTransferPublished);
    const auto pixels          = std::move(m_clipboardTransferPixels);
    const auto mode            = std::exchange(m_queuedClipboardPaste, QString{});
    const auto session         = m_documentSession;
    m_clipboardTransferPending = false;
    emit clipboardChanged();
    if (ready && pixels && !pixels->image.isNull() && !mode.isEmpty())
        QTimer::singleShot(0, this, [this, pixels, mode, session] {
            if (session == m_documentSession && !pixelEditBusy())
                pastePreparedPixels(*pixels, mode);
        });
}

Pixels Document::clipboardPixels(bool _merged)
{
    Pixels result;
    if (m_size.isEmpty() || (!_merged && (m_active < 0 || m_active >= m_layers.size())))
        return result;
    const QRect canvas(QPoint{}, m_size);
    const QRect bounds =
        hasSelection() ? m_selectionRegion.boundingRect().intersected(canvas) : canvas;
    const auto & plane = _merged ? PixelPlane{} : toolPlane();
    const QImage source =
        _merged ? compositeRegion(bounds) : plane.region(bounds).convertToFormat(workingFormat());
    if (source.isNull() || bounds.isEmpty()) {
        fail(tr("Ce calque ne contient pas de pixels à copier."));
        return result;
    }
    auto image = ImageUtils::blank(bounds.size(), workingFormat());
    if (image.isNull()) {
        fail(tr("Mémoire insuffisante pour copier la sélection."));
        return result;
    }
    QPainter painter(&image);
    if (hasSelection())
        painter.setClipRegion(m_selectionRegion.translated(-bounds.topLeft()));
    painter.drawImage(0, 0, source);
    painter.end();
    SelectionCoverage::attenuate(image, m_selectionMask, bounds.topLeft());
    bool visible = false;
    for (int y = 0; y < image.height() && !visible; ++y) {
        if (m_backgroundCancel && m_backgroundCancel->load())
            return {};
        if (ImageUtils::is16Bit(image)) {
            const auto *row = reinterpret_cast<const QRgba64 *>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x)
                if (row[x].alpha()) {
                    visible = true;
                    break;
                }
        } else {
            const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x)
                if (qAlpha(row[x])) {
                    visible = true;
                    break;
                }
        }
    }
    if (!visible) {
        fail(tr("La zone sélectionnée est vide."));
        return {};
    }
    return {image, bounds.topLeft(), m_size, true};
}
void Document::copyPixels(bool _merged)
{
    endStroke();
    if (pixelEditBusy() || selectionBusy()) {
        fail(tr("Attendez la fin du calcul avant de copier ou couper."));
        return;
    }
    auto       result   = std::make_shared<Pixels>();
    const auto revision = PhotoCrap::Clipboard::revision();
    beginClipboardTransfer(result, false);
    const auto publish = [this, result, revision] {
        if (result->image.isNull() || revision != PhotoCrap::Clipboard::revision())
            return;
        PhotoCrap::Clipboard::write(*result);
        m_clipboardTransferPublished = true;
        emit clipboardChanged();
        emit statusMessage(tr("Copie dans le presse-papiers"));
    };
    if (deferOperation(
            _merged ? tr("Copier avec fusion") : tr("Copier"), editCost(false),
            [result, _merged](Document &worker) { *result = worker.clipboardPixels(_merged); },
            false, publish, true))
        return;
    *result = clipboardPixels(_merged);
    publish();
    finishClipboardTransfer();
}
void Document::copy()
{
    copyPixels(false);
}
void Document::copyMerged()
{
    copyPixels(true);
}

void Document::discardClipboardSelection()
{
    if (hasSelection()) {
        m_lastSelectionRegion  = m_selectionRegion;
        m_lastSelectionMask    = m_selectionMask;
        m_lastSelectionOutline = m_selectionOutline;
    }
    m_selection           = {};
    m_selectionRegion     = {};
    m_selectionMask       = {};
    m_selectionOutline    = {};
    m_selectionMaskRegion = {};
    m_selectionShape      = 0;
    m_selectionPath.clear();
    m_polygonLassoActive = false;
    emit selectionChanged();
}
bool Document::eraseClipboardSelection()
{
    auto        next      = toolPlane();
    const QRect bounds    = m_selectionRegion.boundingRect().intersected(QRect(QPoint{}, m_size));
    const int   side      = TiledImage::TileSize;
    int         completed = 0;
    const int count = ((bounds.width() + side - 1) / side) * ((bounds.height() + side - 1) / side);
    for (int y = bounds.y(); y <= bounds.bottom(); y += side)
        for (int x = bounds.x(); x <= bounds.right(); x += side) {
            if (!operationStep(completed++, count))
                return false;
            const QRect part(x, y, qMin(side, bounds.right() - x + 1),
                             qMin(side, bounds.bottom() - y + 1));
            if (!m_selectionRegion.intersects(part))
                continue;
            const auto before = next.region(part);
            auto       output = before.copy();
            if (output.isNull())
                return fail(tr("Mémoire insuffisante pour couper la sélection."));
            QPainter painter(&output);
            painter.setClipRegion(m_selectionRegion.translated(-part.topLeft()));
            if (m_editingMask)
                painter.fillRect(output.rect(), Qt::black);
            else {
                painter.setCompositionMode(QPainter::CompositionMode_Clear);
                painter.fillRect(output.rect(), Qt::transparent);
            }
            painter.end();
            SelectionCoverage::blend(
                output, before, m_selectionMask.isNull() ? QImage{} : m_selectionMask.copy(part),
                output.rect());
            if (!next.writeRegion(part.topLeft(), output, m_size))
                return fail(tr("Mémoire insuffisante pour couper la sélection."));
        }
    toolPlane() = std::move(next);
    m_dirtyRegion |= bounds;
    return true;
}
void Document::cut()
{
    endStroke();
    if (!hasSelection()) {
        fail(tr("Sélectionnez une zone de pixels à couper."));
        return;
    }
    if (pixelEditBusy() || selectionBusy() || !editable())
        return;
    const auto pixels = clipboardPixels(false);
    if (pixels.image.isNull())
        return;
    PhotoCrap::Clipboard::write(pixels);
    emit clipboardChanged();
    beginClipboardTransfer(std::make_shared<Pixels>(pixels), true);
    const auto work = [](Document &worker) {
        if (worker.eraseClipboardSelection())
            worker.discardClipboardSelection();
    };
    if (deferEdit(tr("Couper"), editCost(false), work))
        return;
    const auto before = snapshot({});
    if (!eraseClipboardSelection()) {
        finishClipboardTransfer();
        return;
    }
    discardClipboardSelection();
    commit(before, tr("Couper"), false);
    ++m_thumbnailRevision;
    emit thumbnailsChanged();
    finishClipboardTransfer();
}

bool Document::applyClipboardPixels(const Pixels &_pixels, const QString &_mode)
{
    const bool specialMask = _mode == "inside" || _mode == "outside";
    const bool intoMask    = m_editingMask && !specialMask;
    auto       source      = _pixels.image;
    source.setDevicePixelRatio(1.);
    if (source.colorSpace().isValid())
        source.convertToColorSpace(QColorSpace::SRgb);
    source = source.convertToFormat(workingFormat());
    if (source.isNull())
        return fail(tr("Mémoire insuffisante pour coller l’image."));
    const QRect canvas(QPoint{}, m_size);
    const QRect center   = hasSelection() ? m_selectionRegion.boundingRect() : canvas;
    QPoint      position = _mode == "place" && _pixels.positioned
                               ? _pixels.origin
                               : center.topLeft() + QPoint((center.width() - source.width()) / 2,
                                                      (center.height() - source.height()) / 2);
    // Keep an imported selection at least partly visible on a smaller canvas.
    position.setX(qBound(1 - source.width(), position.x(), width() - 1));
    position.setY(qBound(1 - source.height(), position.y(), height() - 1));
    const QRect bounds = QRect(position, source.size()).intersected(canvas);
    PixelPlane  image =
        intoMask ? toolPlane() : PixelPlane(ImageUtils::blank(QSize(1, 1), workingFormat()));
    const auto before = image.region(bounds);
    auto       output = before.copy();
    if (output.isNull())
        return fail(tr("Mémoire insuffisante pour coller l’image."));
    QPainter painter(&output);
    if (intoMask && hasSelection())
        painter.setClipRegion(m_selectionRegion.translated(-bounds.topLeft()));
    painter.drawImage(position - bounds.topLeft(), source);
    painter.end();
    if (intoMask)
        SelectionCoverage::blend(output, before,
                                 m_selectionMask.isNull() ? QImage{} : m_selectionMask.copy(bounds),
                                 output.rect());
    if (!image.writeRegion(bounds.topLeft(), output, m_size))
        return fail(tr("Mémoire insuffisante pour coller l’image."));
    PixelPlane mask;
    if (specialMask) {
        auto pixels = m_selectionMask.isNull()
                          ? SelectionCoverage::raster(m_selectionRegion, m_size)
                          : m_selectionMask;
        if (pixels.isNull())
            return fail(tr("Mémoire insuffisante pour créer le masque du collage."));
        if (_mode == "outside")
            pixels.invertPixels();
        mask = pixels;
    }
    if (intoMask)
        toolPlane() = std::move(image);
    else {
        if (!startToolLayer(tr("Image collée")))
            return false;
        m_strokeCreatedLayer = false;
        m_beforeStroke       = {};
        auto &layer          = m_layers[m_active];
        layer.name           = tr("Image collée");
        layer.image          = std::move(image);
        layer.mask           = std::move(mask);
    }
    discardClipboardSelection();
    m_dirtyRegion |= bounds;
    return true;
}
void Document::paste()
{
    pasteSpecial("normal");
}
void Document::pasteSpecial(const QString &_mode)
{
    if (_mode != "normal" && _mode != "place" && _mode != "inside" && _mode != "outside")
        return;
    if (m_clipboardTransferPending) {
        m_queuedClipboardPaste = _mode;
        return;
    }
    pastePreparedPixels(PhotoCrap::Clipboard::read(), _mode);
}
void Document::pastePreparedPixels(const Pixels &_pixels, const QString &_mode)
{
    endStroke();
    if (pixelEditBusy() || selectionBusy() || m_size.isEmpty())
        return;
    const bool specialMask = _mode == "inside" || _mode == "outside";
    if (specialMask && !hasSelection())
        return;
    if (m_editingMask && !specialMask) {
        if (!editable())
            return;
    } else if (!validSize(width(), height(), int(m_layers.size()) + 1) ||
               !validLayerStorage(width(), height(), 1, specialMask ? 1 : 0))
        return;
    const auto pixels = _pixels;
    if (pixels.image.isNull())
        return;
    finishLayerPreview();
    acceptLayerPropertiesSession();
    if (deferEdit(
            tr("Coller"), qint64(pixels.image.width()) * pixels.image.height(),
            [pixels, _mode](Document &worker) { worker.applyClipboardPixels(pixels, _mode); }))
        return;
    const auto before = snapshot({});
    if (applyClipboardPixels(pixels, _mode))
        commit(before, tr("Coller"));
}
void Document::layerViaSelection(bool _cut)
{
    endStroke();
    if (!hasSelection() || m_editingMask || pixelEditBusy() || selectionBusy() ||
        (_cut && !editable()))
        return;
    if (!validSize(width(), height(), int(m_layers.size()) + 1) ||
        !validLayerStorage(width(), height(), 1))
        return;
    const auto pixels = clipboardPixels(false);
    if (pixels.image.isNull())
        return;
    finishLayerPreview();
    acceptLayerPropertiesSession();
    const auto label =
        _cut ? tr("Nouveau calque par couper") : tr("Nouveau calque par copie de sélection");
    const auto work = [pixels, _cut](Document &worker) {
        if (_cut && !worker.eraseClipboardSelection())
            return;
        worker.applyClipboardPixels(pixels, "place");
    };
    if (deferEdit(label, editCost(false), work))
        return;
    const auto before = snapshot({});
    if (_cut && !eraseClipboardSelection())
        return;
    if (!applyClipboardPixels(pixels, "place")) {
        restore(before);
        return;
    }
    commit(before, label);
}
