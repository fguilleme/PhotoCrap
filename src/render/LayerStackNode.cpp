#include "LayerStackNode.h"
#include "DisplayPyramidPass.h"
#include <QQuickWindow>
#ifdef PHOTOCRAP_RHI_STACK
#include "SpatialFilterPass.h"
#include "TileCache.h"
#include <QFile>
#include <QOpenGLContext>
#include <QSGImageNode>
#include <QSGRenderNode>
#include <QSGTexture>
#include <QSet>
#include <array>
#include <cstring>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <vector>

namespace
{
constexpr int tileSize = 512;
QShader       shader(const char *name)
{
    QFile file(QStringLiteral(":/PhotoCrap/src/render/shaders/") + QString::fromLatin1(name) +
               ".qsb");
    return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader{};
}
using Counters = PhotoCrap::Render::StackStatistics;
struct Input {
    std::unique_ptr<QRhiTexture> texture;
    qint64                       bytes = 0;
};
struct Pass {
    std::unique_ptr<QRhiBuffer>                 uniform;
    std::unique_ptr<QRhiShaderResourceBindings> bindings;
};
// QQuickWindow::createTextureFromRhiTexture transfers ownership. These ping-pong
// targets instead remain owned by the tile, including when the final parity changes.
class BorrowedTexture final : public QSGTexture
{
public:
    explicit BorrowedTexture(QRhiTexture *texture) : m_texture(texture)
    {
    }
    qint64 comparisonKey() const override
    {
        return qint64(quintptr(m_texture));
    }
    QSize textureSize() const override
    {
        return m_texture->pixelSize();
    }
    bool hasAlphaChannel() const override
    {
        return true;
    }
    bool hasMipmaps() const override
    {
        return false;
    }
    QRhiTexture *rhiTexture() const override
    {
        return m_texture;
    }

private:
    QRhiTexture *m_texture;
};
class Tile final : public QSGRenderNode
{
public:
    QQuickWindow                                           *window;
    QRhi                                                   *rhi;
    Counters                                               *counters;
    std::function<void()>                                   failure;
    bool                                                    failed = false;
    QRect                                                   region;
    int                                                     bits;
    QRectF                                                  geometry;
    PhotoCrap::Render::LayerStack                           stack;
    QByteArray                                              renderedKey;
    std::array<std::unique_ptr<QRhiTexture>, 2>             colors, clips;
    std::array<std::unique_ptr<QRhiTextureRenderTarget>, 2> targets;
    std::unique_ptr<QRhiRenderPassDescriptor>               descriptor;
    std::unique_ptr<QRhiSampler>                            sampler;
    std::unique_ptr<QRhiBuffer>                             vertices;
    std::unique_ptr<QRhiGraphicsPipeline>                   pipeline;
    std::unique_ptr<SpatialFilterPass>                      spatial;
    QHash<QByteArray, std::shared_ptr<Input>>               inputs;
    std::vector<std::unique_ptr<Pass>>                      passes;
    QSGImageNode                                           *image          = nullptr;
    QSGTexture                                             *wrapper        = nullptr;
    int                                                     output         = -1;
    qint64                                                  accountedBytes = 0;
    std::unique_ptr<DisplayPyramidPass>                     pyramid;
    QSize                                                   pyramidDocument;
    int                                                     displayLevel = 0;
    quint64                                                 generation   = 0;
    QRhiTexture                                            *displayed    = nullptr;
    Tile(QQuickWindow *owner, const QRect &bounds, int depth, Counters *stats,
         std::function<void()> rejected)
        : window(owner), rhi(owner->rhi()), counters(stats), failure(std::move(rejected)),
          region(bounds), bits(depth)
    {
    }
    ~Tile() override
    {
        counters->bytes.fetch_sub(quint64(accountedBytes));
        if (counters->changed)
            counters->changed(counters->bytes.load());
        if (image && image->parent())
            image->parent()->removeChildNode(image);
        delete image;
        delete wrapper;
        passes.clear();
        pipeline.reset();
        for (auto &target : targets)
            target.reset();
        descriptor.reset();
    }
    bool init()
    {
        sampler.reset(rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        vertices.reset(
            rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 16 * sizeof(float)));
        if (!sampler->create() || !vertices->create())
            return false;
        for (int i = 0; i < 2; ++i) {
            colors[i].reset(rhi->newTexture(bits == 16 ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8,
                                            region.size(), 1, QRhiTexture::RenderTarget));
            clips[i].reset(
                rhi->newTexture(QRhiTexture::R8, region.size(), 1, QRhiTexture::RenderTarget));
            if (!colors[i]->create() || !clips[i]->create())
                return false;
            QRhiTextureRenderTargetDescription description;
            description.setColorAttachments(
                {QRhiColorAttachment(colors[i].get()), QRhiColorAttachment(clips[i].get())});
            targets[i].reset(rhi->newTextureRenderTarget(description));
            if (!descriptor)
                descriptor.reset(targets[i]->newCompatibleRenderPassDescriptor());
            targets[i]->setRenderPassDescriptor(descriptor.get());
            if (!targets[i]->create())
                return false;
        }
        image = window->createImageNode();
        updateBytes();
        return image;
    }
    void setStack(const PhotoCrap::Render::LayerStack &value, const QRect &content,
                  const QRectF &rect)
    {
        stack        = value;
        displayLevel = stack.displayLevel;
        if (pyramidDocument != stack.size) {
            pyramid.reset();
            pyramidDocument = stack.size;
        }
        const int last       = stack.layers.isEmpty() ? 0 : int(stack.layers.size()) % 2;
        output               = last;
        QRhiTexture *texture = colors[last].get();
        QSize        size    = stack.size;
        QRect        box = content, textureRegion = region;
        if (displayLevel > 0) {
            if (!pyramid)
                pyramid = std::make_unique<DisplayPyramidPass>(rhi, region, stack.size, bits);
            texture = pyramid->ensure(texture, displayLevel);
            if (!texture) {
                reject();
                return;
            }
            size           = QSize(qMax(1, size.width() >> displayLevel),
                                   qMax(1, size.height() >> displayLevel));
            const int left = qint64(content.x()) * size.width() / stack.size.width();
            const int top  = qint64(content.y()) * size.height() / stack.size.height();
            box            = QRect(
                left, top,
                qint64(content.x() + content.width()) * size.width() / stack.size.width() - left,
                qint64(content.y() + content.height()) * size.height() / stack.size.height() - top);
            textureRegion = pyramid->region(displayLevel);
        }
        if (displayed != texture) {
            auto *next = new BorrowedTexture(texture);
            image->setTexture(next);
            delete wrapper;
            wrapper   = next;
            displayed = texture;
        }
        QRect source = box.translated(-textureRegion.topLeft());
        if (rhi->isYUpInFramebuffer())
            source.moveTop(textureRegion.height() - source.y() - source.height());
        image->setSourceRect(source);
        geometry = QRectF(rect.x() + box.x() * rect.width() / size.width(),
                          rect.y() + box.y() * rect.height() / size.height(),
                          box.width() * rect.width() / size.width(),
                          box.height() * rect.height() / size.height());
        image->setRect(geometry);
        image->setFiltering(geometry.width() < box.width() ? QSGTexture::Linear
                                                           : QSGTexture::Nearest);
        image->setTextureCoordinatesTransform(
            rhi->isYUpInFramebuffer() ? QSGImageNode::MirrorVertically : QSGImageNode::NoTransform);
        updateBytes();
        markDirty(DirtyGeometry);
    }
    void reject()
    {
        if (!failed) {
            failed = true;
            failure();
        }
    }
    std::shared_ptr<Input> input(const PixelPlane &plane, bool mask,
                                 QRhiResourceUpdateBatch *updates, QSet<QByteArray> &used)
    {
        QByteArray key = mask ? "m" : "c";
        key += plane.regionKey(region);
        used.insert(key);
        if (auto cached = inputs.value(key))
            return cached;
        QImage     pixels;
        const auto format = mask         ? QImage::Format_Grayscale8
                            : bits == 16 ? QImage::Format_RGBA32FPx4_Premultiplied
                                         : QImage::Format_RGBA8888_Premultiplied;
        if (plane.isNull()) {
            pixels = QImage(1, 1, format);
            pixels.fill(mask ? Qt::white : Qt::transparent);
        } else {
            pixels = plane.size() == QSize(1, 1) ? QImage(plane) : plane.region(region);
            pixels = pixels.convertToFormat(format);
        }
        if (pixels.isNull())
            return {};
        auto value = std::make_shared<Input>();
        value->texture.reset(rhi->newTexture(mask         ? QRhiTexture::R8
                                             : bits == 16 ? QRhiTexture::RGBA32F
                                                          : QRhiTexture::RGBA8,
                                             pixels.size()));
        if (!value->texture->create())
            return {};
        QRhiTextureSubresourceUploadDescription data(pixels.constBits(),
                                                     quint32(pixels.sizeInBytes()));
        data.setDataStride(quint32(pixels.bytesPerLine()));
        data.setSourceSize(pixels.size());
        updates->uploadTexture(value->texture.get(),
                               QRhiTextureUploadDescription({QRhiTextureUploadEntry(0, 0, data)}));
        value->bytes = qint64(pixels.width()) * pixels.height() * (mask ? 1 : bits == 16 ? 16 : 4);
        inputs.insert(key, value);
        ++counters->uploads;
        return value;
    }
    void prepare() override
    {
        if (failed || stack.layers.isEmpty())
            return;
        const auto layers = std::move(stack.layers);
        QByteArray key;
        for (const auto &layer : layers) {
            for (const auto &part : {layer.image.regionKey(region), layer.mask.regionKey(region)}) {
                const qint64 size = part.size();
                key.append(reinterpret_cast<const char *>(&size), sizeof(size));
                key += part;
            }
            key.append(reinterpret_cast<const char *>(&layer.opacity), sizeof(layer.opacity));
            key.append(reinterpret_cast<const char *>(&layer.density), sizeof(layer.density));
            key.append(char(layer.blend));
            key.append(char(layer.clipped));
            key.append(char(layer.reset));
            key += layer.spatial.key();
        }
        key += QByteArray::number(stack.bits);
        if (key == renderedKey) {
            if (pyramid && displayLevel > 0) {
                const auto count = pyramid->render(commandBuffer(), displayLevel, generation);
                counters->reductions.fetch_add(count);
            }
            return;
        }
        auto       *updates = rhi->nextResourceUpdateBatch();
        const float quad[]  = {-1, -1, 0, 1, 1, -1, 1, 1, -1, 1, 0, 0, 1, 1, 1, 0};
        if (renderedKey.isEmpty())
            updates->uploadStaticBuffer(vertices.get(), quad);
        QSet<QByteArray> used;
        std::vector<int> filterRuns;
        if (spatial)
            spatial->resetFrame();
        passes.clear();
        bool ok = true;
        for (int index = 0; index < layers.size(); ++index) {
            const auto &layer = layers[index];
            auto        color = input(layer.image, false, updates, used);
            auto        mask  = input(layer.mask, true, updates, used);
            if (!color || !mask) {
                ok = false;
                break;
            }
            QRhiTexture *source = color->texture.get();
            int          run    = -1;
            if (layer.spatial.type != PhotoCrap::Render::SpatialFilter::None) {
                if (!spatial)
                    spatial = std::make_unique<SpatialFilterPass>(rhi, region.size(), bits);
                const auto prepared =
                    spatial->prepare(colors[index % 2].get(), layer.spatial, updates);
                if (!prepared.texture) {
                    ok = false;
                    break;
                }
                source = prepared.texture;
                run    = prepared.index;
            }
            filterRuns.push_back(run);
            auto pass = std::make_unique<Pass>();
            pass->uniform.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 96));
            if (!pass->uniform->create()) {
                ok = false;
                break;
            }
            QByteArray uniform(96, 0);
            const auto matrix = rhi->clipSpaceCorrMatrix();
            std::memcpy(uniform.data(), matrix.constData(), 64);
            const qint32 modes[]  = {layer.blend, layer.clipped,
                                     int(layer.reset) + (run >= 0 ? 2 : 0), index == 0};
            const float  values[] = {layer.opacity, layer.density,
                                     stack.bits == 16 ? 65535.f : 255.f,
                                     rhi->isYUpInFramebuffer() ? 1.f : 0.f};
            std::memcpy(uniform.data() + 64, modes, sizeof(modes));
            std::memcpy(uniform.data() + 80, values, sizeof(values));
            updates->updateDynamicBuffer(pass->uniform.get(), 0, 96, uniform.constData());
            pass->bindings.reset(rhi->newShaderResourceBindings());
            const auto stage = QRhiShaderResourceBinding::FragmentStage;
            pass->bindings->setBindings(
                {QRhiShaderResourceBinding::uniformBuffer(
                     0, stage | QRhiShaderResourceBinding::VertexStage, pass->uniform.get()),
                 QRhiShaderResourceBinding::sampledTexture(1, stage, colors[index % 2].get(),
                                                           sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(2, stage, source, sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(3, stage, mask->texture.get(),
                                                           sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(4, stage, clips[index % 2].get(),
                                                           sampler.get())});
            if (!pass->bindings->create()) {
                ok = false;
                break;
            }
            passes.push_back(std::move(pass));
        }
        if (ok && !pipeline) {
            pipeline.reset(rhi->newGraphicsPipeline());
            pipeline->setShaderStages({{QRhiShaderStage::Vertex, shader("stack.vert")},
                                       {QRhiShaderStage::Fragment, shader("stack.frag")}});
            QRhiVertexInputLayout layout;
            layout.setBindings({QRhiVertexInputBinding(4 * sizeof(float))});
            layout.setAttributes(
                {QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
                 QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2,
                                          2 * sizeof(float))});
            pipeline->setVertexInputLayout(layout);
            pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
            pipeline->setTargetBlends(
                {QRhiGraphicsPipeline::TargetBlend{}, QRhiGraphicsPipeline::TargetBlend{}});
            pipeline->setShaderResourceBindings(passes.front()->bindings.get());
            pipeline->setRenderPassDescriptor(descriptor.get());
            ok = pipeline->create();
        }
        if (!ok) {
            updateBytes();
            updates->release();
            reject();
            return;
        }
        pipeline->setShaderResourceBindings(passes.front()->bindings.get());
        auto *cb = commandBuffer();
        cb->resourceUpdate(updates);
        if (!filterRuns.empty() && filterRuns.front() >= 0) {
            cb->beginPass(targets[0].get(), Qt::transparent, {1.f, 0});
            cb->endPass();
        }
        for (size_t index = 0; index < passes.size(); ++index) {
            if (filterRuns[index] >= 0)
                counters->passes.fetch_add(spatial->render(cb, filterRuns[index]));
            cb->beginPass(targets[(index + 1) % 2].get(), Qt::transparent, {1.f, 0});
            cb->setGraphicsPipeline(pipeline.get());
            cb->setViewport(QRhiViewport(0, 0, region.width(), region.height()));
            cb->setShaderResources(passes[index]->bindings.get());
            const QRhiCommandBuffer::VertexInput input{vertices.get(), 0};
            cb->setVertexInput(0, 1, &input);
            cb->draw(4);
            cb->endPass();
            ++counters->passes;
        }
        for (auto it = inputs.begin(); it != inputs.end();)
            if (!used.contains(it.key()))
                it = inputs.erase(it);
            else
                ++it;
        renderedKey = key;
        ++generation;
        if (pyramid && displayLevel > 0)
            counters->reductions.fetch_add(pyramid->render(cb, displayLevel, generation));
        updateBytes();
    }
    void render(const RenderState *) override
    {
    }
    StateFlags changedStates() const override
    {
        return {};
    }
    RenderingFlags flags() const override
    {
        return NoExternalRendering | DepthAwareRendering | BoundedRectRendering;
    }
    QRectF rect() const override
    {
        return geometry;
    }
    qint64 bytes() const
    {
        qint64 result = qint64(region.width()) * region.height() * (bits == 16 ? 34 : 10) + 64;
        for (const auto &value : inputs)
            result += value->bytes;
        return result + qint64(passes.size()) * 96 + (spatial ? spatial->bytes() : 0) +
               (pyramid ? pyramid->bytes() : 0);
    }
    void updateBytes()
    {
        const auto current = bytes();
        if (current > accountedBytes)
            counters->bytes.fetch_add(quint64(current - accountedBytes));
        else
            counters->bytes.fetch_sub(quint64(accountedBytes - current));
        accountedBytes = current;
        if (counters->changed)
            counters->changed(counters->bytes.load());
    }
};
} // namespace

struct LayerStackNode::Data {
    QQuickWindow             *window;
    std::function<void()>     failure;
    QHash<quint64, Tile *>    tiles;
    QHash<quint64, quint64>   lastUse;
    quint64                   clock = 0;
    std::shared_ptr<Counters> counters;
};
LayerStackNode::LayerStackNode(QQuickWindow *window, std::function<void()> failure,
                               std::shared_ptr<Counters> statistics)
    : d(std::make_unique<Data>())
{
    d->window   = window;
    d->failure  = std::move(failure);
    d->counters = std::move(statistics);
}
LayerStackNode::~LayerStackNode()
{
    for (auto *tile : std::as_const(d->tiles)) {
        if (tile->parent())
            removeChildNode(tile);
        delete tile;
    }
}
bool LayerStackNode::supported(QQuickWindow *window, int bits, bool spatial)
{
    const auto *rhi = window ? window->rhi() : nullptr;
    if (rhi && rhi->backend() == QRhi::OpenGLES2) {
#if QT_CONFIG(opengl)
        const auto *context = QOpenGLContext::currentContext();
        if (!context)
            return false;
        const auto format = context->format();
        if (context->isOpenGLES() ? format.majorVersion() < 3
                                  : format.majorVersion() < 3 ||
                                        (format.majorVersion() == 3 && format.minorVersion() < 3))
            return false;
#else
        return false;
#endif
    }
    return rhi && rhi->resourceLimit(QRhi::TextureSizeMax) >= tileSize + 2 &&
           rhi->isFeatureSupported(QRhi::TexelFetch) &&
           rhi->resourceLimit(QRhi::MaxColorAttachments) >= 2 &&
           rhi->isTextureFormatSupported(bits == 16 || spatial ? QRhiTexture::RGBA32F
                                                               : QRhiTexture::RGBA8,
                                         QRhiTexture::RenderTarget) &&
           rhi->isTextureFormatSupported(QRhiTexture::R8, QRhiTexture::RenderTarget);
}
bool LayerStackNode::update(const PhotoCrap::Render::LayerStack &stack, const QRectF &item,
                            const QRectF &visible, qint64 cacheBytes)
{
    const double sx = item.width() / stack.size.width(), sy = item.height() / stack.size.height();
    const QRect  bounds(QPoint{}, stack.size);
    const QRect  area =
        QRectF(visible.x() / sx, visible.y() / sy, visible.width() / sx, visible.height() / sy)
            .toAlignedRect()
            .intersected(bounds);
    QSet<quint64> used;
    ++d->clock;
    for (int y = area.top() / tileSize; !area.isEmpty() && y <= area.bottom() / tileSize; ++y)
        for (int x = area.left() / tileSize; x <= area.right() / tileSize; ++x) {
            const quint64 key = (quint64(quint32(y)) << 32) | quint32(x);
            const QRect   content =
                QRect(x * tileSize, y * tileSize, tileSize, tileSize).intersected(bounds);
            const QRect padded =
                content
                    .adjusted(-stack.halo - 33, -stack.halo - 33, stack.halo + 33, stack.halo + 33)
                    .intersected(bounds);
            auto *tile = d->tiles.value(key);
            if (tile && (tile->region != padded || tile->bits != stack.bits)) {
                if (tile->parent())
                    removeChildNode(tile);
                delete tile;
                d->tiles.remove(key);
                tile = nullptr;
            }
            if (!tile) {
                tile = new Tile(d->window, padded, stack.bits, d->counters.get(), d->failure);
                if (!tile->init()) {
                    delete tile;
                    return false;
                }
                appendChildNode(tile);
                appendChildNode(tile->image);
                d->tiles.insert(key, tile);
            } else {
                if (!tile->parent())
                    appendChildNode(tile);
                if (!tile->image->parent())
                    appendChildNode(tile->image);
            }
            tile->setStack(stack, content, item);
            used.insert(key);
            d->lastUse.insert(key, d->clock);
        }
    QList<PhotoCrap::Render::TileCacheEntry> entries;
    for (auto it = d->tiles.cbegin(); it != d->tiles.cend(); ++it)
        entries.append(
            {it.key(), it.value()->bytes(), d->lastUse.value(it.key()), used.contains(it.key())});
    const auto retained = PhotoCrap::Render::retainedTiles(entries, cacheBytes);
    for (auto it = d->tiles.begin(); it != d->tiles.end();) {
        auto *tile = it.value();
        if (!retained.contains(it.key())) {
            if (tile->parent())
                removeChildNode(tile);
            delete tile;
            d->lastUse.remove(it.key());
            it = d->tiles.erase(it);
        } else {
            if (!used.contains(it.key())) {
                tile->stack.layers.clear();
                if (tile->parent())
                    removeChildNode(tile);
                if (tile->image->parent())
                    removeChildNode(tile->image);
            }
            ++it;
        }
    }
    return true;
}
quint64 LayerStackNode::uploads() const
{
    return d->counters->uploads.load();
}
quint64 LayerStackNode::passes() const
{
    return d->counters->passes.load();
}
quint64 LayerStackNode::bytes() const
{
    quint64 result = 0;
    for (auto *tile : std::as_const(d->tiles))
        result += tile->bytes();
    return result;
}
#else
struct LayerStackNode::Data {};
LayerStackNode::LayerStackNode(QQuickWindow *, std::function<void()>,
                               std::shared_ptr<PhotoCrap::Render::StackStatistics>)
    : d(std::make_unique<Data>())
{
}
LayerStackNode::~LayerStackNode() = default;
bool LayerStackNode::supported(QQuickWindow *, int, bool)
{
    return false;
}
bool LayerStackNode::update(const PhotoCrap::Render::LayerStack &, const QRectF &, const QRectF &,
                            qint64)
{
    return false;
}
quint64 LayerStackNode::uploads() const
{
    return 0;
}
quint64 LayerStackNode::passes() const
{
    return 0;
}
quint64 LayerStackNode::bytes() const
{
    return 0;
}
#endif
