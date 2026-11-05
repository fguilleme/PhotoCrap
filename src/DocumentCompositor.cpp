#include "Document.h"
#include "ImageAlpha.h"
#include "ImageUtils.h"
#include "render/LayerVisibility.h"

#include <QColorSpace>
#include <QPainter>
#include <QStringList>
#include <QtMath>
#include <numeric>

namespace
{
using PhotoCrap::Render::LayerVisibility::contributes;
using PhotoCrap::Render::LayerVisibility::needsMask;
using PhotoCrap::Render::LayerVisibility::opaqueCoverIndex;

bool requiresFullComposite(const Layer &_layer)
{
    // These filters currently depend on absolute image coordinates. The blur
    // family is local and can be recomputed from a padded dirty rectangle.
    static const QStringList filters{QStringLiteral("mosaic"), QStringLiteral("noise")};
    return contributes(_layer) && _layer.kind == QStringLiteral("adjustment") &&
           filters.contains(_layer.adjustmentType);
}

int effectMargin(const Layer &_layer)
{
    if (!contributes(_layer))
        return 0;
    const int maskMargin =
        needsMask(_layer)
            ? qBound(0, qRound(_layer.parameters.value("_maskFeather").toDouble()), 100)
            : 0;
    if (_layer.kind != QStringLiteral("adjustment"))
        return maskMargin;
    const auto radius = [&] {
        return qBound(0, qCeil(_layer.parameters.value("radius").toDouble()), 100);
    };
    if (_layer.adjustmentType == QStringLiteral("gaussianBlur") ||
        _layer.adjustmentType == QStringLiteral("unsharpMask") ||
        _layer.adjustmentType == QStringLiteral("highPass"))
        return maskMargin + radius() * 3;
    if (_layer.adjustmentType == QStringLiteral("boxBlur"))
        return maskMargin + radius();
    if (_layer.adjustmentType == QStringLiteral("motionBlur"))
        return maskMargin +
               qBound(0, qCeil(_layer.parameters.value("distance").toDouble() / 2.) + 2, 102);
    return maskMargin;
}

QRect expanded(const QRect &_region, int _margin, const QRect &_bounds)
{
    return _region.adjusted(-_margin, -_margin, _margin, _margin).intersected(_bounds);
}

} // namespace

const QImage &Document::composite() const
{
    if (!m_cacheDirty)
        return m_composite;
    const QRect documentRect(QPoint{}, m_size);
    const auto  first       = m_layers.cbegin() + opaqueCoverIndex(m_layers, m_size);
    const bool  fullEffect  = std::any_of(first, m_layers.cend(), requiresFullComposite);
    const bool  fullRender  = m_fullCompositeDirty || fullEffect || m_composite.size() != m_size;
    const QRect dirtyRegion = fullRender ? documentRect : m_dirtyRegion.intersected(documentRect);
    if (dirtyRegion.isEmpty()) {
        m_cacheDirty = false;
        return m_composite;
    }
    const int totalMargin =
        std::accumulate(first, m_layers.cend(), 0,
                        [](int _sum, const Layer &_layer) { return _sum + effectMargin(_layer); });
    // The first expansion is the area whose pixels can change. The second is
    // a halo that supplies neighbouring pixels to the filters. Only the first
    // rectangle is copied back into the cached full-size composite.
    const QRect outputRegion =
        fullRender ? documentRect : expanded(dirtyRegion, totalMargin, documentRect);
    const QRect workRegion =
        fullRender ? documentRect : expanded(outputRegion, totalMargin, documentRect);
    const QImage regionComposite = renderCompositeRect(workRegion);
    const qint64 before          = m_composite.cacheKey();
    if (outputRegion == documentRect)
        m_composite = regionComposite;
    else {
        QPainter painter(&m_composite);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        const QRect sourceRect(outputRegion.topLeft() - workRegion.topLeft(), outputRegion.size());
        painter.drawImage(outputRegion.topLeft(), regionComposite, sourceRect);
    }
    m_compositeUpdateBefore = before;
    m_compositeUpdateAfter  = m_composite.cacheKey();
    m_compositeUpdateRegion = outputRegion;
    m_cacheDirty            = false;
    m_fullCompositeDirty    = false;
    m_dirtyRegion           = {};
    return m_composite;
}

QImage Document::compositeRegion(const QRect &_region) const
{
    const QRect bounds(QPoint{}, m_size);
    const QRect region = _region.intersected(bounds);
    if (region.isEmpty())
        return {};
    if (!m_cacheDirty && m_composite.size() == m_size)
        return m_composite.copy(region);
    // Preserve coordinate-dependent effects, draft rendering, and native image
    // scaling until those paths support an explicit document-space origin.
    const auto first = m_layers.cbegin() + opaqueCoverIndex(m_layers, m_size);
    const bool full =
        m_previewingLayer || std::any_of(first, m_layers.cend(), [](const Layer &_layer) {
            if (!contributes(_layer))
                return false;
            const auto scaled = [](const PixelPlane &_plane) {
                return !_plane.isNull() && _plane.tileImage(0, 0).devicePixelRatio() != 1.;
            };
            return requiresFullComposite(_layer) || _layer.kind.startsWith("frequency") ||
                   scaled(_layer.image) || scaled(_layer.source) ||
                   (needsMask(_layer) && scaled(_layer.mask));
        });
    if (full)
        return composite().copy(region);
    const int margin =
        std::accumulate(first, m_layers.cend(), 0,
                        [](int _sum, const Layer &_layer) { return _sum + effectMargin(_layer); });
    const QRect workRegion = expanded(region, margin, bounds);
    return renderCompositeRect(workRegion)
        .copy(QRect(region.topLeft() - workRegion.topLeft(), region.size()));
}

QImage Document::renderCompositeRect(const QRect &_region) const
{
    QImage     regionComposite = ImageUtils::blank(_region.size(), workingFormat());
    const auto compositeLayer  = [&](const QImage &_source, int _mode, qreal _opacity) {
        if (_mode == 0) {
            // The accumulator belongs to this render. Painting into it avoids
            // allocating and copying the whole composite for every normal layer.
            QPainter painter(&regionComposite);
            painter.setOpacity(qBound(0., _opacity, 1.));
            painter.drawImage(0, 0, _source);
        } else
            regionComposite =
                blendImages(regionComposite, _source, _mode, _opacity, _region.topLeft());
    };
    const auto regionImage = [&](const QImage &_image) {
        // QImage keeps immutable source pixels shared. Only a partial render
        // needs a cropped allocation; a full render can read the original.
        return _region == _image.rect() ? _image : _image.copy(_region);
    };
    QImage     clippingBase, clippingCoverage;
    qreal      clippingBaseOpacity = 1.;
    const auto resetClipping       = [&] {
        clippingBase     = {};
        clippingCoverage = {};
    };
    QHash<QString, bool> groupVisibility;
    for (const auto &layer : m_layers)
        if (layer.kind == QStringLiteral("group"))
            groupVisibility.insert(layer.groupId, layer.visible);
    for (int layerIndex = opaqueCoverIndex(m_layers, m_size); layerIndex < m_layers.size();
         ++layerIndex) {
        const auto &layer = m_layers[layerIndex];
        if (layer.kind == QStringLiteral("group")) {
            resetClipping();
            continue;
        }
        const bool groupVisible =
            layer.groupId.isEmpty() || groupVisibility.value(layer.groupId, true);
        if (!groupVisible || !contributes(layer)) {
            if (!layer.clippingMask)
                resetClipping();
            continue;
        }
        // A clipped layer cannot contribute without coverage from a visible
        // base. Reject it before filters, masks or frequency bands are built.
        if (layer.clippingMask && clippingBase.isNull())
            continue;
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
                compositeLayer(regionImage(layer.source), 0, 1.);
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
            rendered = regionImage(frequencyImage(layer));
        else
            rendered = ImageUtils::compact(layer.image)
                           ? ImageUtils::expandedPlane(layer.image, _region.size())
                           : layer.image.region(_region);
        if (needsMask(layer)) {
            const qreal density = layer.parameters.value("_maskDensity", 100.).toDouble() / 100.;
            const qreal feather = layer.parameters.value("_maskFeather", 0.).toDouble();
            QImage      maskRegion;
            if (ImageUtils::compact(layer.mask))
                maskRegion = QImage(layer.mask);
            else if (feather >= .5) {
                const int    radius     = qBound(1, qRound(feather), 100);
                const QRect  maskBounds = expanded(_region, radius, QRect(QPoint{}, m_size));
                const QImage blurred    = blurImage(layer.mask.region(maskBounds), radius);
                maskRegion = maskBounds == _region
                                 ? blurred
                                 : blurred.copy(QRect(_region.topLeft() - maskBounds.topLeft(),
                                                      _region.size()));
            } else
                maskRegion = layer.mask.region(_region);
            rendered = applyMask(rendered, maskRegion, density, 0);
        }
        if (layer.clippingMask) {
            if (clippingBase.isNull())
                continue;
            if (clippingCoverage.isNull())
                clippingCoverage = PhotoCrap::alphaCoverage(clippingBase, clippingBaseOpacity);
            rendered = applyMask(rendered, clippingCoverage);
        } else {
            clippingBase        = rendered;
            clippingBaseOpacity = layer.opacity;
            clippingCoverage    = {};
        }
        compositeLayer(rendered, layer.blend, layer.opacity);
    }
    return regionComposite;
}

std::unique_ptr<Document> Document::nativeCompositeSource(QVector<Layer> _layers) const
{
    auto renderer                  = std::make_unique<Document>();
    renderer->m_backgroundWorker   = true;
    renderer->m_scratchCache       = m_scratchCache;
    renderer->m_historyBrushSource = {};
    renderer->m_layers             = std::move(_layers);
    renderer->m_size               = m_size;
    renderer->m_channelBits        = m_channelBits;
    renderer->m_cacheDirty         = true;
    renderer->m_fullCompositeDirty = true;
    renderer->m_dirtyRegion        = {};
    return renderer;
}

QRect Document::compositeUpdateRegion(qint64 _before, qint64 _after) const
{
    return _before == m_compositeUpdateBefore && _after == m_compositeUpdateAfter
               ? m_compositeUpdateRegion
               : QRect{};
}

QImage Document::displayComposite() const
{
    if (m_maskOnlyView && m_active >= 0 && m_active < m_layers.size() &&
        !m_layers[m_active].mask.isNull()) {
        QImage mask = maskImage(m_active).convertToFormat(QImage::Format_RGB32);
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
