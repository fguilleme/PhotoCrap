#include "Document.h"
#include "ImageUtils.h"
#include "tools/FloodFill.h"

#include <QtMath>

namespace
{
QColor maskColor(const QColor &_color)
{
    const int value = qGray(_color.rgb());
    return QColor(value, value, value);
}
} // namespace

void Document::floodFill(QPoint _point)
{
    if (m_active < 0 || m_active >= m_layers.size())
        return;
    const PixelPlane &target = m_editingMask ? m_layers[m_active].mask : m_layers[m_active].image;
    QImage            solid(1, 1, ImageUtils::workingFormat(target));
    if (solid.isNull()) {
        fail(tr("Mémoire insuffisante pour le remplissage."));
        return;
    }
    solid.fill(m_editingMask ? maskColor(m_color) : m_color);
    PhotoCrap::Tools::FloodFillOptions options;
    options.size          = m_size;
    options.selection     = hasSelection() ? m_selectionRegion : QRegion{};
    options.selectionMask = m_selectionMask;
    options.replacement   = solid.pixelColor(0, 0);
    options.opacity       = m_brushOpacity;
    options.overlayFormat = workingFormat();
    options.tolerance     = qBound(0, m_fillTolerance, 255);
    options.contiguous    = m_fillContiguous;
    options.antialias     = m_fillAntialias;
    if (m_fillUsePattern) {
        options.pattern = stampPatternImage(m_fillPattern);
        if (options.pattern.isNull()) {
            fail(tr("Choisissez un motif disponible pour le remplissage."));
            return;
        }
        if (m_editingMask)
            options.pattern = options.pattern.convertToFormat(QImage::Format_Grayscale8);
    }
    if (m_fillBlend != 0) {
        options.compose = [mode = qBound(0, m_fillBlend, 26), opacity = options.opacity](
                              const QImage &base, const QImage &overlay, const QPoint &origin) {
            return blendImages(base, overlay, mode, opacity, origin);
        };
    }
    const QRect documentRect(QPoint{}, m_size);
    const QRect bounds =
        hasSelection() ? m_selectionRegion.boundingRect().intersected(documentRect) : documentRect;
    if (!bounds.contains(_point) || (hasSelection() && !m_selectionRegion.contains(_point)))
        return;
    // Preserve the immediate no-op for exact-color fills. With a tolerance or
    // a composite sample, other matching pixels may still need to be painted.
    if (!m_fillSampleAllLayers && options.pattern.isNull() && !options.compose &&
        options.tolerance == 0 && qFuzzyCompare(options.opacity, 1.)) {
        const auto tile =
            target.tileImage(_point.x() / TiledImage::TileSize, _point.y() / TiledImage::TileSize);
        if (tile.isNull()) {
            fail(tr("Mémoire insuffisante pour le remplissage."));
            return;
        }
        const auto seed = tile.size() == QSize(1, 1)
                              ? tile.pixelColor(0, 0)
                              : tile.pixelColor(_point.x() % TiledImage::TileSize,
                                                _point.y() % TiledImage::TileSize);
        if (seed == options.replacement)
            return;
    }
    auto fill = [point     = _point, options,
                 allLayers = m_fillSampleAllLayers && !m_editingMask](Document &worker) mutable {
        auto &target = worker.m_editingMask ? worker.m_layers[worker.m_active].mask
                                            : worker.m_layers[worker.m_active].image;
        if (allLayers) {
            options.sample = worker.composite();
            if (options.sample.isNull()) {
                worker.fail(tr("Mémoire insuffisante pour échantillonner les calques."));
                return;
            }
        }
        auto result = PhotoCrap::Tools::floodFill(target, point, options, worker.m_backgroundCancel,
                                                  worker.m_backgroundProgress);
        if (result.cancelled)
            return;
        if (!result.valid) {
            worker.fail(tr("Mémoire insuffisante pour le remplissage."));
            return;
        }
        if (!result.filledPixels)
            return;
        target = std::move(result.image);
        worker.m_dirtyRegion |= result.region;
        worker.m_strokeChanged = true;
    };
    if (deferEdit(tr("Remplissage"), qint64(bounds.width()) * bounds.height(), fill))
        return;
    fill(*this);
}
