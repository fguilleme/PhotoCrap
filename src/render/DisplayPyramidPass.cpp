#include "DisplayPyramidPass.h"
#ifdef PHOTOCRAP_RHI_STACK
#include "LayerStack.h"
#include <QFile>
#include <QMap>
#include <array>
#include <cmath>
#include <cstring>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <vector>
namespace
{
QShader load(const char *name)
{
    QFile file(QStringLiteral(":/PhotoCrap/src/render/shaders/") + QString::fromLatin1(name) +
               ".qsb");
    return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader{};
}
struct PyramidTarget {
    std::unique_ptr<QRhiTexture>              texture;
    std::unique_ptr<QRhiTextureRenderTarget>  target;
    std::unique_ptr<QRhiRenderPassDescriptor> descriptor;
    std::function<void()>                     releaseScratch;
    ~PyramidTarget()
    {
        target.reset();
        descriptor.reset();
        texture.reset();
        if (releaseScratch)
            releaseScratch();
    }
};
std::shared_ptr<PyramidTarget> pyramidTarget(QRhi *rhi, const QSize &size,
                                             QRhiTexture::Format format)
{
    auto value = std::make_shared<PyramidTarget>();
    value->texture.reset(rhi->newTexture(format, size, 1, QRhiTexture::RenderTarget));
    if (!value->texture->create())
        return {};
    value->target.reset(rhi->newTextureRenderTarget(
        QRhiTextureRenderTargetDescription(QRhiColorAttachment(value->texture.get()))));
    value->descriptor.reset(value->target->newCompatibleRenderPassDescriptor());
    value->target->setRenderPassDescriptor(value->descriptor.get());
    if (!value->target->create())
        return {};
    return value;
}
struct Level {
    std::array<std::shared_ptr<PyramidTarget>, 2>              surfaces;
    bool                                                       borrowedScratch = false;
    bool                                                       uploaded        = false;
    std::array<QRhiGraphicsPipeline *, 2>                      pipelines{};
    std::array<std::unique_ptr<QRhiBuffer>, 2>                 uniforms;
    std::array<std::unique_ptr<QRhiShaderResourceBindings>, 2> bindings;
    std::array<QByteArray, 2>                                  data;
    std::array<QRect, 2>                                       regions;
    QRhiTexture                                               *source     = nullptr;
    quint64                                                    generation = 0;
};
void setPyramidBindings(QRhiShaderResourceBindings &bindings, QRhiBuffer *uniform,
                        QRhiTexture *source, QRhiSampler *sampler)
{
    const auto stage = QRhiShaderResourceBinding::FragmentStage;
    bindings.setBindings({QRhiShaderResourceBinding::uniformBuffer(
                              0, QRhiShaderResourceBinding::VertexStage | stage, uniform),
                          QRhiShaderResourceBinding::sampledTexture(1, stage, source, sampler)});
}
// An output pixel is valid only when its whole area is inside this native tile.
// Extra native padding provides the neighboring output pixel needed for filtering.
QPair<int, int> extent(int first, int length, int original, int reduced)
{
    return {int(std::ceil(double(first) * reduced / original)),
            int(std::floor(double(first + length) * reduced / original))};
}
} // namespace
struct DisplayPyramidPipelines::Data {
    struct Entry {
        std::unique_ptr<QRhiRenderPassDescriptor> descriptor;
        std::unique_ptr<QRhiGraphicsPipeline>     pipeline;
    };
    std::unique_ptr<QRhiShaderResourceBindings> layout;
    std::vector<Entry>                          entries;
};
DisplayPyramidPipelines::DisplayPyramidPipelines()  = default;
DisplayPyramidPipelines::~DisplayPyramidPipelines() = default;
QRhiGraphicsPipeline *DisplayPyramidPipelines::get(QRhiTextureRenderTarget            &target,
                                                   PhotoCrap::Render::StackStatistics &statistics)
{
    if (!d)
        d = std::make_unique<Data>();
    for (const auto &entry : d->entries)
        if (entry.descriptor->isCompatible(target.renderPassDescriptor()))
            return entry.pipeline.get();

    auto *rhi = target.rhi();
    if (!d->layout) {
        auto layout = std::unique_ptr<QRhiShaderResourceBindings>(rhi->newShaderResourceBindings());
        setPyramidBindings(*layout, nullptr, nullptr, nullptr);
        if (!layout->create())
            return nullptr;
        d->layout = std::move(layout);
    }
    Data::Entry entry;
    entry.descriptor.reset(target.newCompatibleRenderPassDescriptor());
    if (!entry.descriptor)
        return nullptr;
    static const QShader vertexShader   = load("pyramid.vert");
    static const QShader fragmentShader = load("pyramid.frag");
    auto pipeline = std::unique_ptr<QRhiGraphicsPipeline>(rhi->newGraphicsPipeline());
    pipeline->setShaderStages(
        {{QRhiShaderStage::Vertex, vertexShader}, {QRhiShaderStage::Fragment, fragmentShader}});
    QRhiVertexInputLayout layout;
    layout.setBindings({QRhiVertexInputBinding(4 * sizeof(float))});
    layout.setAttributes(
        {QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
         QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float))});
    pipeline->setVertexInputLayout(layout);
    pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    pipeline->setTargetBlends({QRhiGraphicsPipeline::TargetBlend{}});
    pipeline->setShaderResourceBindings(d->layout.get());
    pipeline->setRenderPassDescriptor(entry.descriptor.get());
    if (!pipeline->create())
        return nullptr;

    entry.pipeline = std::move(pipeline);
    d->entries.push_back(std::move(entry));
    ++statistics.pipelineCreations;
    return d->entries.back().pipeline.get();
}
struct DisplayPyramidScratchPool::Data {
    QRhi                                          *rhi;
    QHash<quint64, std::weak_ptr<PyramidTarget>>   targets;
    qint64                                         bytes = 0;
    std::function<void(qint64)>                    delta;
};
DisplayPyramidScratchPool::DisplayPyramidScratchPool(QRhi *rhi, std::function<void(qint64)> delta)
    : d(std::make_unique<Data>())
{
    d->rhi   = rhi;
    d->delta = std::move(delta);
}
DisplayPyramidScratchPool::~DisplayPyramidScratchPool() = default;
qint64 DisplayPyramidScratchPool::bytes() const
{
    return d->bytes;
}
struct DisplayPyramidPass::Data {
    QRhi                                      *rhi;
    QRect                                      native;
    QSize                                      document;
    int                                        bits;
    PhotoCrap::Render::StackStatistics        *statistics;
    DisplayPyramidPipelines                   *pipelines;
    std::shared_ptr<DisplayPyramidScratchPool> pool;
    QRhiSampler                               *sampler;
    QRhiBuffer                                *vertices;
    QMap<int, std::shared_ptr<Level>>          levels;
    bool binding(Level &level, int axis, QRhiTexture *source)
    {
        level.bindings[axis].reset(rhi->newShaderResourceBindings());
        setPyramidBindings(*level.bindings[axis], level.uniforms[axis].get(), source, sampler);
        return level.bindings[axis]->create();
    }
};
DisplayPyramidPass::DisplayPyramidPass(QRhi *rhi, const QRect &native, const QSize &document,
                                       int bits, QRhiBuffer &vertices, QRhiSampler &sampler,
                                       DisplayPyramidPipelines                   &pipelines,
                                       PhotoCrap::Render::StackStatistics        &statistics,
                                       std::shared_ptr<DisplayPyramidScratchPool> pool)
    : d(std::make_unique<Data>())
{
    d->rhi      = rhi;
    d->native   = native;
    d->document = document;
    d->bits     = bits;
    d->statistics = &statistics;
    d->vertices   = &vertices;
    d->sampler    = &sampler;
    d->pipelines  = &pipelines;
    d->pool     = std::move(pool);
}
DisplayPyramidPass::~DisplayPyramidPass() = default;
QRhiTexture *DisplayPyramidPass::ensure(QRhiTexture *source, int index)
{
    if (index < 1 || index > 4)
        return nullptr;
    if (auto level = d->levels.value(index)) {
        if (level->source != source) {
            if (!d->binding(*level, 0, source))
                return nullptr;
            level->source     = source;
            level->generation = 0;
        }
        return level->surfaces[1]->texture.get();
    }
    const QSize reduced(qMax(1, d->document.width() >> index),
                        qMax(1, d->document.height() >> index));
    auto        level = std::make_shared<Level>();
    const auto  x = extent(d->native.x(), d->native.width(), d->document.width(), reduced.width());
    const auto  y =
        extent(d->native.y(), d->native.height(), d->document.height(), reduced.height());
    level->regions[0] = QRect(x.first, d->native.y(), x.second - x.first, d->native.height());
    level->regions[1] = QRect(x.first, y.first, x.second - x.first, y.second - y.first);
    for (int axis = 0; axis < 2; ++axis) {
        const QRect target = level->regions[axis];
        if (target.isEmpty())
            return nullptr;
        if (axis == 0 && d->pool) {
            const quint64 key = (quint64(quint32(target.width())) << 32) | quint32(target.height());
            auto         &scratch = *d->pool->d;
            auto          targetTexture = scratch.targets.value(key).lock();
            if (!targetTexture) {
                targetTexture = pyramidTarget(d->rhi, target.size(), QRhiTexture::RGBA32F);
                if (!targetTexture)
                    return nullptr;
                const qint64 bytes = qint64(target.width()) * target.height() * 16;
                targetTexture->releaseScratch = [pool = d->pool, key, bytes] {
                    pool->d->targets.remove(key);
                    pool->d->bytes -= bytes;
                    pool->d->delta(-bytes);
                };
                scratch.targets.insert(key, targetTexture);
                scratch.bytes += bytes;
                scratch.delta(bytes);
            }
            level->surfaces[axis]  = std::move(targetTexture);
            level->borrowedScratch = true;
        } else
            level->surfaces[axis] = pyramidTarget(d->rhi, target.size(),
                                                  axis == 0 || d->bits == 16 ? QRhiTexture::RGBA32F
                                                                             : QRhiTexture::RGBA8);
        if (!level->surfaces[axis])
            return nullptr;
        level->uniforms[axis].reset(
            d->rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 128));
        if (!level->uniforms[axis]->create() ||
            !d->binding(*level, axis, axis == 0 ? source : level->surfaces[0]->texture.get()))
            return nullptr;
        auto &data        = level->data[axis];
        data              = QByteArray(128, 0);
        const auto matrix = d->rhi->clipSpaceCorrMatrix();
        std::memcpy(data.data(), matrix.constData(), 64);
        const QRect  input   = axis == 0 ? d->native : level->regions[0];
        const qint32 sr[]    = {input.x(), input.y(), input.width(), input.height()};
        const qint32 dr[]    = {target.x(), target.y(), target.width(), target.height()};
        const float  scale[] = {axis == 0 ? float(d->document.width()) / reduced.width() : 1.f,
                                axis == 0 ? 1.f : float(d->document.height()) / reduced.height(), 0,
                                0};
        const qint32 options[] = {axis, int(d->rhi->isYUpInFramebuffer()), axis,
                                  d->bits == 16 ? 65535 : 255};
        std::memcpy(data.data() + 64, sr, 16);
        std::memcpy(data.data() + 80, dr, 16);
        std::memcpy(data.data() + 96, scale, 16);
        std::memcpy(data.data() + 112, options, 16);
        level->pipelines[axis] = d->pipelines->get(*level->surfaces[axis]->target, *d->statistics);
        if (!level->pipelines[axis])
            return nullptr;
    }
    level->source = source;
    d->levels.insert(index, level);
    return level->surfaces[1]->texture.get();
}
QRect DisplayPyramidPass::region(int level) const
{
    auto value = d->levels.value(level);
    return value ? value->regions[1] : QRect{};
}
quint64 DisplayPyramidPass::render(QRhiCommandBuffer *cb, int index, quint64 generation)
{
    auto level = d->levels.value(index);
    if (!level || level->generation == generation)
        return 0;
    if (!level->uploaded) {
        auto *updates = d->rhi->nextResourceUpdateBatch();
        for (int axis = 0; axis < 2; ++axis)
            updates->updateDynamicBuffer(level->uniforms[axis].get(), 0, 128,
                                         level->data[axis].constData());
        cb->resourceUpdate(updates);
        level->uploaded = true;
        d->statistics->uniformUpdates.fetch_add(2);
        // Each level has a fixed sampling grid, even when its input changes.
        level->data = {};
    }
    for (int axis = 0; axis < 2; ++axis) {
        cb->beginPass(level->surfaces[axis]->target.get(), Qt::transparent, {1.f, 0});
        cb->setGraphicsPipeline(level->pipelines[axis]);
        const auto size = level->regions[axis].size();
        cb->setViewport(QRhiViewport(0, 0, size.width(), size.height()));
        cb->setShaderResources(level->bindings[axis].get());
        const QRhiCommandBuffer::VertexInput input{d->vertices, 0};
        cb->setVertexInput(0, 1, &input);
        cb->draw(4);
        cb->endPass();
    }
    level->generation = generation;
    return 2;
}
qint64 DisplayPyramidPass::bytes() const
{
    qint64 result = 0;
    for (const auto &level : d->levels)
        for (int axis = 0; axis < 2; ++axis) {
            const auto size = level->regions[axis].size();
            result += 128;
            if (axis != 0 || !level->borrowedScratch)
                result +=
                    qint64(size.width()) * size.height() * (axis == 0 || d->bits == 16 ? 16 : 4);
        }
    return result;
}
#else
struct DisplayPyramidPipelines::Data {};
DisplayPyramidPipelines::DisplayPyramidPipelines()  = default;
DisplayPyramidPipelines::~DisplayPyramidPipelines() = default;
struct DisplayPyramidScratchPool::Data {};
DisplayPyramidScratchPool::DisplayPyramidScratchPool(QRhi *, std::function<void(qint64)>)
    : d(std::make_unique<Data>())
{
}
DisplayPyramidScratchPool::~DisplayPyramidScratchPool() = default;
qint64 DisplayPyramidScratchPool::bytes() const
{
    return 0;
}
struct DisplayPyramidPass::Data {};
DisplayPyramidPass::DisplayPyramidPass(QRhi *, const QRect &, const QSize &, int, QRhiBuffer &,
                                       QRhiSampler &, DisplayPyramidPipelines &,
                                       PhotoCrap::Render::StackStatistics &,
                                       std::shared_ptr<DisplayPyramidScratchPool>)
    : d(std::make_unique<Data>())
{
}
DisplayPyramidPass::~DisplayPyramidPass() = default;
QRhiTexture *DisplayPyramidPass::ensure(QRhiTexture *, int)
{
    return nullptr;
}
QRect DisplayPyramidPass::region(int) const
{
    return {};
}
quint64 DisplayPyramidPass::render(QRhiCommandBuffer *, int, quint64)
{
    return 0;
}
qint64 DisplayPyramidPass::bytes() const
{
    return 0;
}
#endif
