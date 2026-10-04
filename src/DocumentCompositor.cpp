#include "Document.h"
#include "ImageUtils.h"

#include <QColorSpace>
#include <QPainter>
#include <QStringList>
#include <QtMath>
#include <numeric>

namespace
{
bool requiresFullComposite(const Layer &_layer)
{
    // These filters currently depend on absolute image coordinates. The blur
    // family is local and can be recomputed from a padded dirty rectangle.
    static const QStringList filters{QStringLiteral("mosaic"), QStringLiteral("noise")};
    return _layer.visible && _layer.kind == QStringLiteral("adjustment") &&
           filters.contains(_layer.adjustmentType);
}

int effectMargin(const Layer &_layer)
{
    if (!_layer.visible || _layer.kind != QStringLiteral("adjustment"))
        return 0;
    const auto radius = [&] {
        return qBound(0, qCeil(_layer.parameters.value("radius").toDouble()), 100);
    };
    if (_layer.adjustmentType == QStringLiteral("gaussianBlur") ||
        _layer.adjustmentType == QStringLiteral("unsharpMask") ||
        _layer.adjustmentType == QStringLiteral("highPass"))
        return radius() * 3;
    if (_layer.adjustmentType == QStringLiteral("boxBlur"))
        return radius();
    if (_layer.adjustmentType == QStringLiteral("motionBlur"))
        return qBound(0, qCeil(_layer.parameters.value("distance").toDouble() / 2.) + 2, 102);
    return 0;
}

QRect expanded(const QRect &_region, int _margin, const QRect &_bounds)
{
    return _region.adjusted(-_margin, -_margin, _margin, _margin).intersected(_bounds);
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
    const bool  fullRender = m_fullCompositeDirty || fullEffect || m_composite.size() != m_size;
    const QRect dirtyRegion = fullRender ? documentRect : m_dirtyRegion.intersected(documentRect);
    if (dirtyRegion.isEmpty()) {
        m_cacheDirty = false;
        return m_composite;
    }
    const int totalMargin =
        std::accumulate(m_layers.cbegin(), m_layers.cend(), 0,
                        [](int _sum, const Layer &_layer) { return _sum + effectMargin(_layer); });
    // The first expansion is the area whose pixels can change. The second is
    // a halo that supplies neighbouring pixels to the filters. Only the first
    // rectangle is copied back into the cached full-size composite.
    const QRect outputRegion =
        fullRender ? documentRect : expanded(dirtyRegion, totalMargin, documentRect);
    const QRect workRegion =
        fullRender ? documentRect : expanded(outputRegion, totalMargin, documentRect);
    QImage               regionComposite = ImageUtils::blank(workRegion.size());
    QImage               clippingBase;
    qreal                clippingBaseOpacity = 1.;
    QHash<QString, bool> groupVisibility;
    for (const auto &layer : m_layers)
        if (layer.kind == QStringLiteral("group"))
            groupVisibility.insert(layer.groupId, layer.visible);
    for (int layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex) {
        const auto &layer = m_layers[layerIndex];
        if (layer.kind == QStringLiteral("group")) {
            clippingBase = {};
            continue;
        }
        const bool groupVisible =
            layer.groupId.isEmpty() || groupVisibility.value(layer.groupId, true);
        if (!layer.visible || !groupVisible) {
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
                regionComposite =
                    blendImages(regionComposite, layer.source.copy(workRegion), 0, 1.);
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
            rendered = frequencyImage(layer).copy(workRegion);
        else
            rendered = layer.image.copy(workRegion);
        if (layer.maskEnabled && !layer.mask.isNull()) {
            const qreal  density = layer.parameters.value("_maskDensity", 100.).toDouble() / 100.;
            const qreal  feather = layer.parameters.value("_maskFeather", 0.).toDouble();
            const QImage maskRegion =
                (feather >= .5 ? blurImage(layer.mask, qRound(feather)) : layer.mask)
                    .copy(workRegion);
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
    if (outputRegion == documentRect)
        m_composite = regionComposite;
    else {
        QPainter painter(&m_composite);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        const QRect sourceRect(outputRegion.topLeft() - workRegion.topLeft(), outputRegion.size());
        painter.drawImage(outputRegion.topLeft(), regionComposite, sourceRect);
    }
    m_cacheDirty         = false;
    m_fullCompositeDirty = false;
    m_dirtyRegion        = {};
    return m_composite;
}

QImage Document::displayComposite() const
{
    if (m_maskOnlyView && m_active >= 0 && m_active < m_layers.size() &&
        !m_layers[m_active].mask.isNull()) {
        QImage mask = m_layers[m_active].mask.convertToFormat(QImage::Format_RGB32);
        mask.setColorSpace(QColorSpace::SRgb);
        return mask;
    }
    QImage displayed;
    {
        QMutexLocker locker(&m_previewImageMutex);
        if (!m_previewComposite.isNull())
            displayed = m_previewComposite;
        else if (m_previewRenderActive.load())
            displayed = m_composite;
    }
    if (displayed.isNull())
        displayed = composite();
    if (m_displayChannel == 0 || displayed.isNull())
        return displayed;
    if (m_channelDisplaySourceKey == displayed.cacheKey() &&
        m_channelDisplayCacheIndex == m_displayChannel && !m_channelDisplayCache.isNull())
        return m_channelDisplayCache;

    const QImage input = displayed.convertToFormat(QImage::Format_ARGB32);
    QImage       result(input.size(), QImage::Format_ARGB32_Premultiplied);
    const int    shift = m_displayChannel == 1 ? 16 : m_displayChannel == 2 ? 8 : 0;
    for (int y = 0; y < input.height(); ++y) {
        const auto *source = reinterpret_cast<const QRgb *>(input.constScanLine(y));
        auto       *output = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < input.width(); ++x) {
            const int value = (source[x] >> shift) & 0xff;
            output[x]       = qRgba(value, value, value, qAlpha(source[x]));
        }
    }
    result.setColorSpace(displayed.colorSpace());
    m_channelDisplaySourceKey  = displayed.cacheKey();
    m_channelDisplayCacheIndex = m_displayChannel;
    m_channelDisplayCache      = result;
    return result;
}
