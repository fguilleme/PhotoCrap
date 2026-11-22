#include "Document.h"
#include "ImageAlpha.h"
#include "filters/RelightFilter.h"
#include "ImagePixels.h"
#include "ImageUtils.h"
#include "masks/VectorMask.h"
#include "render/LayerVisibility.h"
#include "tools/GradientRaster.h"
#include "tools/LayerTransform.h"

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

bool requiresFullComposite(const Layer &_layer, bool _opaqueBase, bool _hasClipping)
{
    if (!contributes(_layer))
        return false;
    // Preserve the full-image rounding paths for clipped dissolve stacks and
    // for noise over transparency.
    // Neural tiles are anchored to the full input, including its reflected edges.
    return _layer.adjustmentType == QStringLiteral("denoise") || (_layer.blend == 1 && _hasClipping) ||
           (_layer.kind == QStringLiteral("adjustment") &&
            _layer.adjustmentType == QStringLiteral("noise") &&
            (!_opaqueBase || _layer.clippingMask));
}

bool opaqueBase(const Layer &_layer)
{
    return _layer.kind == QStringLiteral("pixel") && _layer.visible && _layer.opacity == 1 &&
           _layer.blend == 0 && !_layer.clippingMask && _layer.groupId.isEmpty() &&
           !needsMask(_layer) && _layer.image.knownOpaque();
}

bool canRenderFrequencyRegion(const Layer &_layer, const QSize &_size)
{
    // Straight-alpha conversion of 8-bit pixels can round differently in
    // Qt's vector and tail code. High frequencies subtract those values.
    return _layer.source.size() == _size && _layer.source.devicePixelRatio() == 1. &&
           (_layer.kind != QStringLiteral("frequencyHigh") || ImageUtils::is16Bit(_layer.source) ||
            _layer.source.knownOpaque());
}

int effectMargin(const Layer &_layer)
{
    if (!contributes(_layer))
        return 0;
    const int maskMargin =
        needsMask(_layer)
            ? qBound(0, qRound(_layer.parameters.value("_maskFeather").toDouble()), 100)
            : 0;
    const int vectorMargin = PhotoCrap::Masks::vectorEnabled(_layer)
                                 ? qCeil(_layer.parameters.value("_vectorFeather", 0.).toDouble())
                                 : 0;
    const int directMargin =
        _layer.directType.isEmpty()
            ? 0
            : qCeil(_layer.directParameters.value("radius", 0.).toDouble() * 3 +
                    _layer.directParameters.value("distance", 0.).toDouble());
    if (_layer.kind != QStringLiteral("adjustment"))
        return maskMargin + vectorMargin + directMargin;
    const auto radius = [&] {
        return qBound(0, qCeil(_layer.parameters.value("radius").toDouble()), 100);
    };
    if (_layer.adjustmentType == QStringLiteral("gaussianBlur") ||
        _layer.adjustmentType == QStringLiteral("unsharpMask") ||
        _layer.adjustmentType == QStringLiteral("highPass"))
        return maskMargin + radius() * 3;
    if (_layer.adjustmentType == QStringLiteral("boxBlur"))
        return maskMargin + radius();
    if (_layer.adjustmentType == QStringLiteral("clarityDehaze"))
        return maskMargin + vectorMargin + qBound(1, _layer.parameters.value("radius", 18).toInt(), 100);
    if (_layer.adjustmentType == QStringLiteral("texture")) {
        const int r = qBound(1, qRound(_layer.parameters.value("radius", 4.).toDouble()), 100);
        return maskMargin + vectorMargin + r + r / 4;
    }
    if (_layer.adjustmentType == QStringLiteral("motionBlur"))
        return maskMargin +
               qBound(0, qCeil(_layer.parameters.value("distance").toDouble() / 2.) + 2, 102);
    if (_layer.adjustmentType == QStringLiteral("mosaic")) {
        const int cell = _layer.parameters.value("cellSize", 12).toInt();
        return maskMargin + (cell > 1 ? qMin(200, cell) + 2 : 0);
    }
    return maskMargin + vectorMargin + directMargin;
}

QRect expanded(const QRect &_region, int _margin, const QRect &_bounds)
{
    return _region.adjusted(-_margin, -_margin, _margin, _margin).intersected(_bounds);
}

// A color adjustment inherits its input alpha. Remove that contribution before
// blending atop the base, retaining any coverage introduced by the layer mask.
template <typename Pixel>
QImage adjustmentColors(const QImage &_image, const QImage &backdrop = {})
{
    using P      = ImageUtils::PixelAccess<Pixel>;
    QImage image = _image.convertToFormat(P::format);
    if (image.isNull())
        return image;
    auto        *data   = image.bits();
    const auto   stride = image.bytesPerLine();
    QVector<int> rows(image.height());
    std::iota(rows.begin(), rows.end(), 0);
    ImageUtils::blockingMap(rows, [&](int y) {
        auto       *row  = reinterpret_cast<Pixel *>(data + y * stride);
        const auto *base = backdrop.isNull()
                               ? nullptr
                               : reinterpret_cast<const Pixel *>(backdrop.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (!P::alpha(row[x]))
                continue;
            const double alpha = base && P::alpha(base[x]) > 0.
                                     ? qMin(255., 255. * P::alpha(row[x]) / P::alpha(base[x]))
                                     : 255.;
            if constexpr (P::wide)
                row[x].setAlpha(qRound(alpha * 257.));
            else
                row[x] = (row[x] & 0x00ffffffu) | (QRgb(qRound(alpha)) << 24);
        }
    });
    return image;
}

} // namespace

const QImage &Document::composite() const
{
    if (!m_cacheDirty)
        return m_composite;
    const QRect documentRect(QPoint{}, m_size);
    const auto  first       = m_layers.cbegin() + opaqueCoverIndex(m_layers, m_size);
    const bool  opaque      = first != m_layers.cend() && opaqueBase(*first);
    const bool  clipping =
        std::any_of(first, m_layers.cend(), [](const Layer &layer) { return layer.clippingMask; });
    const bool  fullEffect  = std::any_of(first, m_layers.cend(), [&](const Layer &_layer) {
        return requiresFullComposite(_layer, opaque, clipping);
    });
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
    // Preserve draft rendering and native image scaling when sampling directly.
    const auto first = m_layers.cbegin() + opaqueCoverIndex(m_layers, m_size);
    const bool opaque = first != m_layers.cend() && opaqueBase(*first);
    const bool clipping =
        std::any_of(first, m_layers.cend(), [](const Layer &layer) { return layer.clippingMask; });
    const bool full =
        m_previewingLayer || m_frequencyPreviewDraft ||
        std::any_of(first, m_layers.cend(), [&](const Layer &_layer) {
            if (!contributes(_layer))
                return false;
            const auto scaled = [](const PixelPlane &_plane) {
                return _plane.devicePixelRatio() != 1.;
            };
            return requiresFullComposite(_layer, opaque, clipping) ||
                   (_layer.kind.startsWith("frequency") &&
                    !canRenderFrequencyRegion(_layer, m_size)) ||
                   scaled(_layer.image) || scaled(_layer.source) ||
                   (needsMask(_layer) && scaled(_layer.mask));
        });
    if (full)
        return composite().copy(region);
    const int margin =
        std::accumulate(first, m_layers.cend(), 0,
                        [](int _sum, const Layer &_layer) { return _sum + effectMargin(_layer); });
    const QRect workRegion = expanded(region, margin, bounds);
    const QImage rendered   = renderCompositeRect(workRegion);
    return workRegion == region ? rendered
                                : rendered.copy(region.translated(-workRegion.topLeft()));
}

QImage Document::renderCompositeRect(const QRect &_region) const
{
    QImage     regionComposite = ImageUtils::blank(_region.size(), workingFormat());
    const auto compositeLayer  = [&](const QImage &_source, const Layer &layer) {
        const bool preserveAlpha =
            layer.kind == "adjustment" &&
            (layer.adjustmentType == "texture" || layer.adjustmentType == "splitToning" ||
             layer.adjustmentType == "relight" || layer.adjustmentType == "clarityDehaze" ||
             layer.adjustmentType == "grain" || layer.adjustmentType == "denoise");
        if (preserveAlpha) {
            if (layer.blend == 0 && layer.opacity == 1. && !needsMask(layer) &&
                !layer.clippingMask && layer.directType.isEmpty()) {
                regionComposite = _source;
                return;
            }
            const auto colors = m_channelBits == 16
                                     ? adjustmentColors<QRgba64>(_source, regionComposite)
                                     : adjustmentColors<QRgb>(_source, regionComposite);
            regionComposite   = blendImages(regionComposite, colors, layer.blend, layer.opacity,
                                             _region.topLeft(), true);
        } else if (layer.blend == 0) {
            // The accumulator belongs to this render. Painting into it avoids
            // allocating and copying the whole composite for every normal layer.
            QPainter painter(&regionComposite);
            painter.setOpacity(qBound(0., layer.opacity, 1.));
            painter.drawImage(0, 0, _source);
        } else
            regionComposite = blendImages(regionComposite, _source, layer.blend, layer.opacity,
                                           _region.topLeft());
    };
    const auto regionImage = [&](const QImage &_image) {
        // QImage keeps immutable source pixels shared. Only a partial render
        // needs a cropped allocation; a full render can read the original.
        return _region == _image.rect() ? _image : _image.copy(_region);
    };
    QImage     clippingBase, clippingCoverage;
    qreal      clippingBaseOpacity = 1.;
    int        isolatedBase        = -1;
    const auto finishClipping      = [&] {
        if (isolatedBase >= 0) {
            const auto &base = m_layers[isolatedBase];
            compositeLayer(clippingBase, base);
            isolatedBase = -1;
        }
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
        if (!layer.clippingMask || layer.kind == QStringLiteral("group") ||
            (layerIndex > 0 && m_layers[layerIndex - 1].groupId != layer.groupId))
            finishClipping();
        if (layer.kind == QStringLiteral("group"))
            continue;
        const bool groupVisible =
            layer.groupId.isEmpty() || groupVisibility.value(layer.groupId, true);
        if (!groupVisible || !contributes(layer))
            continue;
        // A clipped layer cannot contribute without coverage from a visible
        // base. Reject it before filters, masks or frequency bands are built.
        if (layer.clippingMask && clippingBase.isNull())
            continue;
        // An untouched low/high pair reconstructs its source independently of
        // the radius. Collapsing it avoids two full-image filters per slider tick.
        if (PhotoCrap::Render::LayerVisibility::reconstructsFrequencySource(m_layers, layerIndex)) {
            compositeLayer(_region.size() != m_size && layer.source.size() == m_size &&
                                   layer.source.devicePixelRatio() == 1.
                               ? layer.source.region(_region)
                               : regionImage(layer.source),
                           layer);
            ++layerIndex;
            continue;
        }
        QImage rendered;
        if (layer.adjustmentType == "gradientFill") {
            PhotoCrap::Tools::GradientRaster gradient;
            if (PhotoCrap::Tools::GradientRaster::decode(
                    layer.parameters.value("gradient").toMap(), m_size, gradient))
                rendered = gradient.image(_region, m_channelBits == 16);
        } else if (layer.kind == QStringLiteral("adjustment")) {
            const bool isolatedAdjustment =
                isolatedBase >= 0 && PhotoCrap::Render::LayerVisibility::samplesComposite(layer);
            const QImage            &input = isolatedAdjustment ? clippingBase : regionComposite;
            static const QStringList scalableFilters{
                QStringLiteral("gaussianBlur"), QStringLiteral("motionBlur"),
                QStringLiteral("boxBlur"),      QStringLiteral("unsharpMask"),
                QStringLiteral("highPass"),     QStringLiteral("mosaic")};
            const bool draft = m_previewingLayer && layerIndex == m_active &&
                               scalableFilters.contains(layer.adjustmentType) &&
                               qMax(input.width(), input.height()) > 960;
            if (layer.adjustmentType == "relight")
                rendered = PhotoCrap::Filters::relight(input, layer, _region.topLeft());
            else if (draft) {
                const qreal scale = 960. / qreal(qMax(input.width(), input.height()));
                const QSize draftSize(qMax(1, qRound(input.width() * scale)),
                                      qMax(1, qRound(input.height() * scale)));
                QVariantMap parameters = layer.parameters;
                for (const auto &key : {QStringLiteral("radius"), QStringLiteral("distance"),
                                        QStringLiteral("cellSize")})
                    if (parameters.contains(key))
                        parameters.insert(key, qMax(1., parameters.value(key).toDouble() * scale));
                rendered =
                    adjustedImage(
                        input.scaled(draftSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                        layer.adjustmentType, parameters)
                        .scaled(input.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            } else
                rendered = adjustedImage(input, layer.adjustmentType, layer.parameters,
                                         _region.topLeft(), m_size);
            if (isolatedAdjustment)
                rendered = m_channelBits == 16 ? adjustmentColors<QRgba64>(rendered)
                                               : adjustmentColors<QRgb>(rendered);
        } else if (layer.kind.startsWith(QStringLiteral("frequency"))) {
            const bool regional = !m_frequencyPreviewDraft && _region.size() != m_size &&
                                  canRenderFrequencyRegion(layer, m_size);
            rendered =
                regional ? frequencyImage(layer, _region) : regionImage(frequencyImage(layer));
        } else
            rendered = ImageUtils::compact(layer.image)
                           ? ImageUtils::expandedPlane(layer.image, _region.size())
                           : layer.image.region(_region);
        if (!layer.directType.isEmpty() && !layer.directMask)
            rendered = directAdjustedImage(rendered, layer, _region);
        if (layer.maskEnabled && !layer.mask.isNull() && (needsMask(layer) || layer.directMask)) {
            const qreal density = layer.parameters.value("_maskDensity", 100.).toDouble() / 100.;
            const qreal feather = layer.parameters.value("_maskFeather", 0.).toDouble();
            QImage      maskRegion;
            if (!layer.directType.isEmpty() && layer.directMask) {
                const int    radius = qBound(0, qRound(feather), 100);
                const QRect  bounds = expanded(_region, radius, QRect(QPoint{}, m_size));
                const QImage raw    = ImageUtils::compact(layer.mask)
                                          ? ImageUtils::expandedPlane(layer.mask, bounds.size())
                                          : layer.mask.region(bounds);
                maskRegion          = directAdjustedImage(raw, layer, bounds);
                if (radius > 0)
                    maskRegion = blurImage(maskRegion, radius);
                if (bounds != _region)
                    maskRegion = maskRegion.copy(
                        QRect(_region.topLeft() - bounds.topLeft(), _region.size()));
            } else if (ImageUtils::compact(layer.mask))
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
        if (PhotoCrap::Masks::vectorEnabled(layer)) {
            const int radius =
                qBound(0, qRound(layer.parameters.value("_vectorFeather", 0.).toDouble()), 100);
            const QRect bounds   = expanded(_region, radius, QRect(QPoint{}, m_size));
            auto        coverage = PhotoCrap::Masks::vectorCoverage(layer, m_size, bounds);
            if (radius > 0)
                coverage = blurImage(coverage, radius);
            if (bounds != _region)
                coverage =
                    coverage.copy(QRect(_region.topLeft() - bounds.topLeft(), _region.size()));
            rendered = applyMask(rendered, coverage,
                                 layer.parameters.value("_vectorDensity", 100.).toDouble() / 100.);
        }
        if (layer.clippingMask) {
            if (isolatedBase >= 0) {
                clippingBase = blendImages(clippingBase, rendered, layer.blend, layer.opacity,
                                           _region.topLeft(), true);
                continue;
            }
            if (clippingCoverage.isNull())
                clippingCoverage = PhotoCrap::alphaCoverage(clippingBase, clippingBaseOpacity);
            rendered = applyMask(rendered, clippingCoverage);
        } else {
            clippingBase        = rendered;
            clippingBaseOpacity = layer.opacity;
            if (!rendered.isNull() && PhotoCrap::Render::LayerVisibility::needsIsolatedClipping(
                                          m_layers, layerIndex, m_size)) {
                isolatedBase = layerIndex;
                continue;
            }
        }
        compositeLayer(rendered, layer);
    }
    finishClipping();
    return regionComposite;
}

QImage Document::frozenCompositeRegion(const QRect &_region) const
{
    const QRect bounds(QPoint{}, m_size);
    const QRect region = _region.intersected(bounds);
    if (region.isEmpty() || !m_cacheDirty)
        return compositeRegion(region);
    if (m_frozenSampleRegion.contains(region))
        return region == m_frozenSampleRegion
                   ? m_frozenSample
                   : m_frozenSample.copy(region.translated(-m_frozenSampleRegion.topLeft()));
    // Keep the first sample exact. Once requests overlap, include a small
    // neighborhood so successive dabs can reuse the same composited pixels.
    constexpr qint64 maxBytes   = 256 * 1024;
    const int        pixelBytes = m_channelBits == 16 ? 8 : 4;
    const QRect      window =
        m_frozenSampleRegion.intersects(region)
            ? expanded(region, qBound(8, qMin(region.width(), region.height()) / 2, 32), bounds)
            : region;
    const auto fits = [&](const QRect &_rect) {
        return qint64(_rect.width()) * _rect.height() * pixelBytes <= maxBytes;
    };
    const QRect sampleRegion = fits(window) ? window : region;
    QImage      sample       = compositeRegion(sampleRegion);
    if (sample.isNull() || !m_cacheDirty || !fits(sampleRegion)) {
        m_frozenSample       = {};
        m_frozenSampleRegion = {};
    } else {
        m_frozenSample       = sample;
        m_frozenSampleRegion = sampleRegion;
    }
    return sampleRegion == region || sample.isNull()
               ? sample
               : sample.copy(region.translated(-sampleRegion.topLeft()));
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
        if ((m_transform && m_transform->job.maskView) ||
            (!m_layers[m_active].directType.isEmpty() && m_layers[m_active].directMask)) {
            QMutexLocker locker(&m_previewImageMutex);
            if (!m_previewComposite.isNull())
                return m_previewComposite;
        }
        const auto pixels = m_textMaskEdit && !m_textFontPreview.isNull()
                                ? ImageUtils::expandedPlane(m_textFontPreview, m_size)
                                : maskImage(m_active);
        QImage mask = pixels.convertToFormat(QImage::Format_RGB32);
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
    if (displayed.isNull() && !m_previewRenderActive.load())
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
