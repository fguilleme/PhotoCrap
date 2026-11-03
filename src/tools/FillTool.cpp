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
    PixelPlane &target = m_editingMask ? m_layers[m_active].mask : m_layers[m_active].image;
    QImage      solid(1, 1, ImageUtils::workingFormat(target));
    if (solid.isNull()) {
        fail(tr("Mémoire insuffisante pour la couverture du trait."));
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
    const QRect documentRect(QPoint{}, m_size);
    const QRect bounds =
        hasSelection() ? m_selectionRegion.boundingRect().intersected(documentRect) : documentRect;
    if (!bounds.contains(_point) || (hasSelection() && !m_selectionRegion.contains(_point)))
        return;
    const QImage seedTile =
        target.tileImage(_point.x() / TiledImage::TileSize, _point.y() / TiledImage::TileSize);
    if (seedTile.isNull()) {
        fail(tr("Mémoire insuffisante pour la couverture du trait."));
        return;
    }
    const QColor seedColor = seedTile.size() == QSize(1, 1)
                                 ? seedTile.pixelColor(0, 0)
                                 : seedTile.pixelColor(_point.x() % TiledImage::TileSize,
                                                       _point.y() % TiledImage::TileSize);
    if (seedColor == options.replacement && qFuzzyCompare(m_brushOpacity, 1.))
        return;
    const qreal opacity = m_brushOpacity;
    if (deferEdit(tr("Remplissage"), qint64(bounds.width()) * bounds.height(),
                  [point = _point, opacity](Document &worker) {
                      worker.m_brushOpacity = opacity;
                      worker.floodFill(point);
                  }))
        return;
    auto result = PhotoCrap::Tools::floodFill(target, _point, options, m_backgroundCancel,
                                              m_backgroundProgress);
    if (result.cancelled)
        return;
    if (!result.valid) {
        fail(tr("Mémoire insuffisante pour la couverture du trait."));
        return;
    }
    if (!result.filledPixels)
        return;
    target          = std::move(result.image);
    m_dirtyRegion   = m_dirtyRegion.isEmpty() ? result.region : m_dirtyRegion.united(result.region);
    m_strokeChanged = true;
}
