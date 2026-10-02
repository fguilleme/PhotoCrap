#include "Document.h"
#include "ImageUtils.h"

#include <QColorSpace>
#include <QPainter>
#include <QStringList>

namespace
{
bool requiresFullComposite(const Layer &_layer)
{
    static const QStringList filters{QStringLiteral("gaussianBlur"), QStringLiteral("motionBlur"),
                                     QStringLiteral("boxBlur"),      QStringLiteral("unsharpMask"),
                                     QStringLiteral("highPass"),     QStringLiteral("mosaic"),
                                     QStringLiteral("noise")};
    return _layer.visible && _layer.kind == QStringLiteral("adjustment") &&
           filters.contains(_layer.adjustmentType);
}

QImage alphaCoverage(const QImage &_source, qreal _opacity)
{
    if (_source.isNull())
        return {};
    const QImage source = _source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage       coverage(source.size(), QImage::Format_Grayscale8);
    const qreal  opacity = qBound(0., _opacity, 1.);
    for (int y = 0; y < source.height(); ++y) {
        const auto *pixels = reinterpret_cast<const QRgb *>(source.constScanLine(y));
        auto       *mask   = coverage.scanLine(y);
        for (int x = 0; x < source.width(); ++x)
            mask[x] = uchar(qRound(qAlpha(pixels[x]) * opacity));
    }
    return coverage;
}
} // namespace

const QImage &Document::composite() const
{
    if (!m_cacheDirty)
        return m_composite;
    const QRect documentRect(QPoint{}, m_size);
    const bool  fullEffect = std::any_of(m_layers.cbegin(), m_layers.cend(), requiresFullComposite);
    const QRect region     = (m_fullCompositeDirty || fullEffect || m_composite.size() != m_size
                                  ? documentRect
                                  : m_dirtyRegion.intersected(documentRect));
    if (region.isEmpty()) {
        m_cacheDirty = false;
        return m_composite;
    }
    QImage regionComposite = ImageUtils::blank(region.size());
    QImage clippingBase;
    qreal  clippingBaseOpacity = 1.;
    for (int layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex) {
        const auto &layer = m_layers[layerIndex];
        if (!layer.visible) {
            if (!layer.clippingMask)
                clippingBase = {};
            continue;
        }
        // An untouched low/high pair reconstructs its source independently of
        // the radius. Collapsing it avoids two full-image filters per slider tick.
        if (layer.kind == QStringLiteral("frequencyLow") && layerIndex + 1 < m_layers.size()) {
            const auto   &high       = m_layers[layerIndex + 1];
            const QString pairId     = layer.parameters.value("pairId").toString();
            const bool    plainMasks = (!layer.maskEnabled || layer.mask.isNull()) &&
                                       (!high.maskEnabled || high.mask.isNull());
            const bool    followedByClipping =
                layerIndex + 2 < m_layers.size() && m_layers[layerIndex + 2].clippingMask;
            if (!layer.clippingMask && !high.clippingMask && !followedByClipping && high.visible &&
                high.kind == QStringLiteral("frequencyHigh") && !pairId.isEmpty() &&
                high.parameters.value("pairId").toString() == pairId &&
                layer.source.cacheKey() == high.source.cacheKey() &&
                qFuzzyCompare(layer.opacity, 1.) && qFuzzyCompare(high.opacity, 1.) &&
                layer.blend == 0 && high.blend == 16 && plainMasks) {
                regionComposite = blendImages(regionComposite, layer.source.copy(region), 0, 1.);
                ++layerIndex;
                continue;
            }
        }
        QImage rendered;
        if (layer.kind == QStringLiteral("adjustment")) {
            static const QStringList scalableFilters{
                QStringLiteral("gaussianBlur"), QStringLiteral("motionBlur"),
                QStringLiteral("boxBlur"),      QStringLiteral("unsharpMask"),
                QStringLiteral("highPass"),     QStringLiteral("mosaic")};
            const bool draft = m_previewingLayer && layerIndex == m_active &&
                               scalableFilters.contains(layer.adjustmentType) &&
                               qMax(regionComposite.width(), regionComposite.height()) > 960;
            if (draft) {
                const qreal scale =
                    960. / qreal(qMax(regionComposite.width(), regionComposite.height()));
                const QSize draftSize(qMax(1, qRound(regionComposite.width() * scale)),
                                      qMax(1, qRound(regionComposite.height() * scale)));
                QVariantMap parameters = layer.parameters;
                for (const auto &key : {QStringLiteral("radius"), QStringLiteral("distance"),
                                        QStringLiteral("cellSize")})
                    if (parameters.contains(key))
                        parameters.insert(key, qMax(1., parameters.value(key).toDouble() * scale));
                rendered = adjustedImage(regionComposite.scaled(draftSize, Qt::IgnoreAspectRatio,
                                                                Qt::SmoothTransformation),
                                         layer.adjustmentType, parameters)
                               .scaled(regionComposite.size(), Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
            } else
                rendered = adjustedImage(regionComposite, layer.adjustmentType, layer.parameters);
        } else if (layer.kind.startsWith(QStringLiteral("frequency")))
            rendered = frequencyImage(layer).copy(region);
        else
            rendered = layer.image.copy(region);
        if (layer.maskEnabled && !layer.mask.isNull()) {
            const qreal  density = layer.parameters.value("_maskDensity", 100.).toDouble() / 100.;
            const qreal  feather = layer.parameters.value("_maskFeather", 0.).toDouble();
            const QImage maskRegion =
                (feather >= .5 ? blurImage(layer.mask, qRound(feather)) : layer.mask).copy(region);
            rendered = applyMask(rendered, maskRegion, density, 0);
        }
        if (layer.clippingMask) {
            if (clippingBase.isNull())
                continue;
            rendered = applyMask(rendered, alphaCoverage(clippingBase, clippingBaseOpacity));
        } else {
            clippingBase        = rendered;
            clippingBaseOpacity = layer.opacity;
        }
        regionComposite = blendImages(regionComposite, rendered, layer.blend, layer.opacity);
    }
    if (region == documentRect)
        m_composite = regionComposite;
    else {
        QPainter painter(&m_composite);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(region.topLeft(), regionComposite);
    }
    m_cacheDirty         = false;
    m_fullCompositeDirty = false;
    m_dirtyRegion        = {};
    return m_composite;
}
