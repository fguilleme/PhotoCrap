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
    QByteArray                   contentKey;
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
struct Workspace {
    QRhi                                                   *rhi;
    QSize                                                   size;
    int                                                     bits;
    Counters                                               *counters;
    qint64                                                  accounted = 0;
    std::array<std::unique_ptr<QRhiTexture>, 2>             colors, clips;
    std::array<std::unique_ptr<QRhiTextureRenderTarget>, 2> targets;
    std::unique_ptr<QRhiRenderPassDescriptor>               descriptor;
    std::unique_ptr<SpatialFilterPass>                      spatial, masks;
    QHash<QByteArray, std::shared_ptr<Input>>               inputs;
    Workspace(QRhi *device, QSize extent, int depth, Counters *stats)
        : rhi(device), size(extent), bits(depth), counters(stats)
    {
    }
    ~Workspace()
    {
        spatial.reset();
        masks.reset();
        for (auto &target : targets)
            target.reset();
        descriptor.reset();
        counters->bytes.fetch_sub(quint64(accounted));
        if (counters->changed)
            counters->changed(counters->bytes.load());
    }
    bool init()
    {
        if (descriptor)
            return true;
        for (int i = 0; i < 2; ++i) {
            colors[i].reset(
                rhi->newTexture(bits == 16 ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8, size, 1,
                                QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
            clips[i].reset(
                rhi->newTexture(QRhiTexture::R8, size, 1,
                                QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
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
        updateBytes();
        return true;
    }
    qint64 bytes() const
    {
        qint64 result = qint64(size.width()) * size.height() * (bits == 16 ? 34 : 10);
        for (const auto &input : inputs)
            result += input->bytes;
        return result + (spatial ? spatial->bytes() : 0) + (masks ? masks->bytes() : 0);
    }
    void updateBytes()
    {
        const qint64 next = bytes();
        if (next >= accounted)
            counters->bytes.fetch_add(quint64(next - accounted));
        else
            counters->bytes.fetch_sub(quint64(accounted - next));
        accounted = next;
        if (counters->changed)
            counters->changed(counters->bytes.load());
    }
};
class Tile final : public QSGRenderNode
{
public:
    QQuickWindow                              *window;
    QRhi                                      *rhi;
    Counters                                  *counters;
    std::function<void()>                      failure;
    bool                                       failed = false;
    QRect                                      region;
    int                                        bits;
    bool                                       streaming;
    QSize                                      tableSize{1, 1};
    QRectF                                     geometry;
    PhotoCrap::Render::LayerStack              stack;
    QByteArray                                 renderedKey;
    std::shared_ptr<Workspace>                 work;
    bool                                       sharedWork = false;
    std::shared_ptr<DisplayPyramidScratchPool> reductionPool;
    std::unique_ptr<QRhiTexture>               retained;
    std::unique_ptr<QRhiSampler>               sampler;
    std::unique_ptr<QRhiBuffer>                vertices;
    std::unique_ptr<QRhiGraphicsPipeline>      pipeline;
    std::vector<std::unique_ptr<Pass>>         passes;
    QSGImageNode                              *image          = nullptr;
    QSGTexture                                *wrapper        = nullptr;
    int                                        output         = -1;
    qint64                                     accountedBytes = 0;
    std::unique_ptr<DisplayPyramidPass>        pyramid;
    QSize                                      pyramidDocument;
    int                                        displayLevel = 0;
    quint64                                    generation   = 0;
    QRhiTexture                               *displayed    = nullptr;
    Tile(QQuickWindow *owner, const QRect &bounds, int depth, bool stream, Counters *stats,
         std::shared_ptr<Workspace> shared, std::shared_ptr<DisplayPyramidScratchPool> reductions,
         std::function<void()> rejected)
        : window(owner), rhi(owner->rhi()), counters(stats), failure(std::move(rejected)),
          region(bounds), bits(depth), streaming(stream)
    {
        reductionPool = std::move(reductions);
        sharedWork    = bool(shared);
        work          = shared ? std::move(shared)
                               : std::make_shared<Workspace>(rhi, region.size(), bits, counters);
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
    }
    bool init()
    {
        sampler.reset(rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        vertices.reset(
            rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 16 * sizeof(float)));
        if (!sampler->create() || !vertices->create())
            return false;
        if (!work->init())
            return false;
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
        QRhiTexture *texture = work->colors[last].get();
        QSize        size    = stack.size;
        QRect        box = content, textureRegion = region;
        if (displayLevel > 0) {
            if (!pyramid)
                pyramid = std::make_unique<DisplayPyramidPass>(rhi, region, stack.size, bits,
                                                               reductionPool);
            texture = pyramid->ensure(texture, displayLevel);
            if (!texture) {
                reject(__LINE__);
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
        if (sharedWork && displayLevel == 0) {
            if (!retained) {
                retained.reset(rhi->newTexture(
                    bits == 16 ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8, region.size()));
                if (!retained->create()) {
                    reject(__LINE__);
                    return;
                }
            }
            texture = retained.get();
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
    void reject(int line = 0)
    {
        if (!failed) {
            failed = true;
            qWarning() << "GPU stack failed" << line << region << streaming;
            failure();
        }
    }
    std::shared_ptr<Input> input(const PixelPlane &plane, bool mask,
                                 QRhiResourceUpdateBatch *updates, QSet<QByteArray> &used,
                                 bool whole = false, bool stageOnly = false)
    {
        QByteArray key = whole ? "t" : mask ? "m" : "c";
        // Whole work->inputs are immutable LUT QImages reused by the document cache.
        // Freezing a fresh wrapper would manufacture new tile versions each frame.
        key += whole ? QByteArray::number(plane.cacheKey()) : plane.regionKey(region);
        const QByteArray contentKey = key;
        if (streaming)
            key = whole ? "stream-table" : mask ? "stream-mask" : "stream-color";
        used.insert(key);
        auto value = work->inputs.value(key);
        if (value && (!streaming || stageOnly || value->contentKey == contentKey))
            return value;
        const QSize extent = streaming ? (whole ? tableSize : region.size())
                             : plane.isNull() || plane.size() == QSize(1, 1) ? QSize(1, 1)
                             : whole                                         ? plane.size()
                                                                             : region.size();
        if (!value) {
            value = std::make_shared<Input>();
            value->texture.reset(rhi->newTexture(mask         ? QRhiTexture::R8
                                                 : bits == 16 ? QRhiTexture::RGBA32F
                                                              : QRhiTexture::RGBA8,
                                                 extent));
            if (!value->texture->create())
                return {};
            value->bytes = qint64(extent.width()) * extent.height() *
                           (mask         ? 1
                            : bits == 16 ? 16
                                         : 4);
            work->inputs.insert(key, value);
        }
        if (streaming && stageOnly)
            return value;
        QImage     pixels;
        const auto format = mask         ? QImage::Format_Grayscale8
                            : bits == 16 ? QImage::Format_RGBA32FPx4_Premultiplied
                                         : QImage::Format_RGBA8888_Premultiplied;
        if (plane.isNull()) {
            pixels = QImage(1, 1, format);
            pixels.fill(mask ? Qt::white : Qt::transparent);
        } else {
            pixels = whole || plane.size() == QSize(1, 1) ? QImage(plane) : plane.region(region);
            pixels = pixels.convertToFormat(format);
        }
        if (pixels.isNull())
            return {};
        QRhiTextureSubresourceUploadDescription data(pixels.constBits(),
                                                     quint32(pixels.sizeInBytes()));
        data.setDataStride(quint32(pixels.bytesPerLine()));
        data.setSourceSize(pixels.size());
        updates->uploadTexture(value->texture.get(),
                               QRhiTextureUploadDescription({QRhiTextureUploadEntry(0, 0, data)}));
        value->contentKey = contentKey;
        ++counters->uploads;
        return value;
    }
    void prepare() override
    {
        if (failed || stack.layers.isEmpty())
            return;
        const auto layers = std::move(stack.layers);
        tableSize         = {1, 1};
        for (const auto &layer : layers)
            tableSize = tableSize.expandedTo(layer.toneTable.size());
        QByteArray key;
        for (const auto &layer : layers) {
            for (const auto &part : {layer.image.regionKey(region), layer.mask.regionKey(region),
                                     QByteArray::number(layer.toneTable.cacheKey())}) {
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
            key += layer.maskFilter.key();
            const auto  &tone = layer.tone;
            const qint32 mode = qint32(tone.mode);
            key.append(reinterpret_cast<const char *>(&mode), sizeof(mode));
            for (const auto &vector :
                 {tone.color, tone.families0, tone.families1, tone.matrix[0], tone.matrix[1],
                  tone.matrix[2], tone.selective[0], tone.selective[1], tone.selective[2],
                  tone.selective[3], tone.selective[4], tone.selective[5], tone.selective[6],
                  tone.selective[7], tone.selective[8]})
                for (int channel = 0; channel < 4; ++channel) {
                    const float value = vector[channel];
                    key.append(reinterpret_cast<const char *>(&value), sizeof(value));
                }
        }
        key += QByteArray::number(stack.bits);
        if (sharedWork)
            key += "/" + QByteArray::number(displayLevel);
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
        std::vector<int> filterRuns, maskRuns;
        if (work->spatial)
            work->spatial->resetFrame();
        if (work->masks)
            work->masks->resetFrame();
        passes.clear();
        if (streaming) {
            const auto table = work->inputs.value("stream-table");
            if (table && table->texture->pixelSize() != tableSize)
                work->inputs.remove("stream-table");
        }
        bool ok = true;
        for (int index = 0; index < layers.size(); ++index) {
            const auto &layer = layers[index];
            auto        color = input(layer.image, false, updates, used, false, streaming);
            auto        mask  = input(layer.mask, true, updates, used, false, streaming);
            auto        table = layer.toneTable.isNull()
                                    ? color
                                    : input(layer.toneTable, false, updates, used, true, streaming);
            if (!color || !mask || !table) {
                ok = false;
                break;
            }
            QRhiTexture *maskSource = mask->texture.get();
            int          maskRun    = -1;
            if (layer.maskFilter.type != PhotoCrap::Render::SpatialFilter::None) {
                if (!work->masks)
                    work->masks = std::make_unique<SpatialFilterPass>(rhi, region.size(), 8);
                const auto prepared =
                    work->masks->prepare(maskSource, layer.maskFilter, updates, true);
                if (!prepared.texture) {
                    ok = false;
                    break;
                }
                maskSource = prepared.texture;
                maskRun    = prepared.index;
            }
            maskRuns.push_back(maskRun);
            QRhiTexture *source = color->texture.get();
            int          run    = -1;
            if (layer.spatial.type != PhotoCrap::Render::SpatialFilter::None) {
                if (!work->spatial)
                    work->spatial = std::make_unique<SpatialFilterPass>(rhi, region.size(), bits);
                const auto prepared =
                    work->spatial->prepare(work->colors[index % 2].get(), layer.spatial, updates);
                if (!prepared.texture) {
                    ok = false;
                    break;
                }
                source = prepared.texture;
                run    = prepared.index;
            }
            filterRuns.push_back(run);
            auto pass = std::make_unique<Pass>();
            pass->uniform.reset(
                rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 256));
            if (!pass->uniform->create()) {
                ok = false;
                break;
            }
            QByteArray uniform(256, 0);
            const auto matrix = rhi->clipSpaceCorrMatrix();
            std::memcpy(uniform.data(), matrix.constData(), 64);
            const qint32 modes[] = {
                layer.blend, layer.clipped,
                int(layer.reset) + (run >= 0 ? 2 : 0) + (!layer.toneTable.isNull() ? 4 : 0) +
                    (maskRun >= 0 ? 8 : 0) +
                    (streaming && (layer.image.isNull() || layer.image.size() == QSize(1, 1)) &&
                             run < 0
                         ? 16
                         : 0) +
                    (streaming && (layer.mask.isNull() || layer.mask.size() == QSize(1, 1)) &&
                             maskRun < 0
                         ? 32
                         : 0),
                index == 0};
            const float values[] = {layer.opacity, layer.density,
                                    stack.bits == 16 ? 65535.f : 255.f,
                                    rhi->isYUpInFramebuffer() ? 1.f : 0.f};
            std::memcpy(uniform.data() + 64, modes, sizeof(modes));
            std::memcpy(uniform.data() + 80, values, sizeof(values));
            const qint32 toneMode = int(layer.tone.mode);
            std::memcpy(uniform.data() + 96, &toneMode, 4);
            auto parameters = layer.tone.selective;
            if (layer.tone.mode != PhotoCrap::Filters::ToneMappingMode::SelectiveColorRelative &&
                layer.tone.mode != PhotoCrap::Filters::ToneMappingMode::SelectiveColorAbsolute)
                parameters = {layer.tone.color,
                              layer.tone.families0,
                              layer.tone.families1,
                              layer.tone.matrix[0],
                              layer.tone.matrix[1],
                              layer.tone.matrix[2],
                              {},
                              {},
                              {}};
            for (size_t i = 0; i < parameters.size(); ++i)
                for (int c = 0; c < 4; ++c) {
                    const float v = parameters[i][c];
                    std::memcpy(uniform.data() + 112 + i * 16 + c * 4, &v, 4);
                }
            updates->updateDynamicBuffer(pass->uniform.get(), 0, 256, uniform.constData());
            pass->bindings.reset(rhi->newShaderResourceBindings());
            const auto stage = QRhiShaderResourceBinding::FragmentStage;
            pass->bindings->setBindings(
                {QRhiShaderResourceBinding::uniformBuffer(
                     0, stage | QRhiShaderResourceBinding::VertexStage, pass->uniform.get()),
                 QRhiShaderResourceBinding::sampledTexture(1, stage, work->colors[index % 2].get(),
                                                           sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(2, stage, source, sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(3, stage, maskSource, sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(4, stage, work->clips[index % 2].get(),
                                                           sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(5, stage, table->texture.get(),
                                                           sampler.get())});
            if (!pass->bindings->create()) {
                ok = false;
                break;
            }
            passes.push_back(std::move(pass));
        }
        if (ok && !pipeline) {
            pipeline.reset(rhi->newGraphicsPipeline());
            pipeline->setShaderStages({{QRhiShaderStage::Vertex, shader("stack_tonal.vert")},
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
            pipeline->setRenderPassDescriptor(work->descriptor.get());
            ok = pipeline->create();
        }
        if (!ok) {
            updateBytes();
            updates->release();
            reject(__LINE__);
            return;
        }
        pipeline->setShaderResourceBindings(passes.front()->bindings.get());
        auto *cb = commandBuffer();
        cb->resourceUpdate(updates);
        if (!filterRuns.empty() &&
            (filterRuns.front() >= 0 || !layers.front().toneTable.isNull())) {
            cb->beginPass(work->targets[0].get(), Qt::transparent, {1.f, 0});
            cb->endPass();
        }
        for (size_t index = 0; index < passes.size(); ++index) {
            if (streaming) {
                auto       *upload = rhi->nextResourceUpdateBatch();
                const auto &layer  = layers[qsizetype(index)];
                const bool  valid =
                    input(layer.image, false, upload, used) &&
                    input(layer.mask, true, upload, used) &&
                    (layer.toneTable.isNull() || input(layer.toneTable, false, upload, used, true));
                if (!valid) {
                    upload->release();
                    reject(__LINE__);
                    return;
                }
                cb->resourceUpdate(upload);
            }
            if (maskRuns[index] >= 0)
                counters->passes.fetch_add(work->masks->render(cb, maskRuns[index]));
            if (filterRuns[index] >= 0)
                counters->passes.fetch_add(work->spatial->render(cb, filterRuns[index]));
            cb->beginPass(work->targets[(index + 1) % 2].get(), Qt::transparent, {1.f, 0});
            cb->setGraphicsPipeline(pipeline.get());
            cb->setViewport(QRhiViewport(0, 0, region.width(), region.height()));
            cb->setShaderResources(passes[index]->bindings.get());
            const QRhiCommandBuffer::VertexInput input{vertices.get(), 0};
            cb->setVertexInput(0, 1, &input);
            cb->draw(4);
            cb->endPass();
            ++counters->passes;
        }
        for (auto it = work->inputs.begin(); it != work->inputs.end();)
            if (!used.contains(it.key()))
                it = work->inputs.erase(it);
            else
                ++it;
        renderedKey = key;
        ++generation;
        if (pyramid && displayLevel > 0)
            counters->reductions.fetch_add(pyramid->render(cb, displayLevel, generation));
        if (sharedWork && displayLevel == 0) {
            auto *copy = rhi->nextResourceUpdateBatch();
            copy->copyTexture(retained.get(), work->colors[output].get());
            cb->resourceUpdate(copy);
        }
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
    qint64 bytes(bool includeWorkspace = true) const
    {
        qint64 result = 64 + qint64(passes.size()) * 256 + (pyramid ? pyramid->bytes() : 0);
        if (retained)
            result += qint64(region.width()) * region.height() * (bits == 16 ? 16 : 4);
        return result + (includeWorkspace ? work->bytes() : 0);
    }
    void updateBytes()
    {
        work->updateBytes();
        const auto current = bytes(false);
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
    QQuickWindow                               *window;
    std::function<void()>                       failure;
    QHash<quint64, Tile *>                      tiles;
    QHash<quint64, quint64>                     lastUse;
    quint64                                     clock = 0;
    QSize                                       document;
    QHash<QByteArray, std::weak_ptr<Workspace>> workspaces;
    std::shared_ptr<DisplayPyramidScratchPool>  reductions;

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
    if (d->document != stack.size) {
        for (auto *tile : std::as_const(d->tiles)) {
            if (tile->parent())
                removeChildNode(tile);
            delete tile;
        }
        d->tiles.clear();
        d->lastUse.clear();
        d->workspaces.clear();
        d->reductions.reset();
        d->document = stack.size;
    }
    if (stack.shareScratch && !d->reductions) {
        const auto counters = d->counters;
        d->reductions =
            std::make_shared<DisplayPyramidScratchPool>(d->window->rhi(), [counters](qint64 delta) {
                if (delta >= 0)
                    counters->bytes.fetch_add(quint64(delta));
                else
                    counters->bytes.fetch_sub(quint64(-delta));
                if (counters->changed)
                    counters->changed(counters->bytes.load());
            });
    } else if (!stack.shareScratch)
        d->reductions.reset();
    QSet<quint64> used;
    for (auto it = d->workspaces.begin(); it != d->workspaces.end();)
        if (it.value().expired())
            it = d->workspaces.erase(it);
        else
            ++it;
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
            std::shared_ptr<Workspace> workspace;
            if (stack.shareScratch) {
                QSize tableSize(1, 1);
                for (const auto &layer : stack.layers)
                    tableSize = tableSize.expandedTo(layer.toneTable.size());
                const QByteArray workKey = QByteArray::number(padded.width()) + "/" +
                                           QByteArray::number(padded.height()) + "/" +
                                           QByteArray::number(stack.bits) + "/" +
                                           QByteArray::number(tableSize.width()) + "/" +
                                           QByteArray::number(tableSize.height());
                workspace                = d->workspaces.value(workKey).lock();
                if (!workspace) {
                    workspace = std::make_shared<Workspace>(d->window->rhi(), padded.size(),
                                                            stack.bits, d->counters.get());
                    d->workspaces.insert(workKey, workspace);
                }
            }
            auto *tile = d->tiles.value(key);
            if (tile &&
                (tile->region != padded || tile->bits != stack.bits ||
                 tile->streaming != stack.streamInputs || tile->sharedWork != stack.shareScratch ||
                 (workspace && tile->work != workspace))) {
                if (tile->parent())
                    removeChildNode(tile);
                delete tile;
                d->tiles.remove(key);
                tile = nullptr;
            }
            if (!tile) {
                tile =
                    new Tile(d->window, padded, stack.bits, stack.streamInputs, d->counters.get(),
                             workspace, stack.shareScratch ? d->reductions : nullptr, d->failure);
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
