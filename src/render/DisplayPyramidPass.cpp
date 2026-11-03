#include "DisplayPyramidPass.h"
#ifdef PHOTOCRAP_RHI_STACK
#include <QFile>
#include <QMap>
#include <array>
#include <cmath>
#include <cstring>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
namespace
{
QShader load(const char *name)
{
    QFile file(QStringLiteral(":/PhotoCrap/src/render/shaders/") + QString::fromLatin1(name) +
               ".qsb");
    return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader{};
}
struct Level {
    std::array<std::unique_ptr<QRhiTexture>, 2>                textures;
    std::array<std::unique_ptr<QRhiTextureRenderTarget>, 2>    targets;
    std::array<std::unique_ptr<QRhiRenderPassDescriptor>, 2>   descriptors;
    std::array<std::unique_ptr<QRhiGraphicsPipeline>, 2>       pipelines;
    std::array<std::unique_ptr<QRhiBuffer>, 2>                 uniforms;
    std::array<std::unique_ptr<QRhiShaderResourceBindings>, 2> bindings;
    std::array<QByteArray, 2>                                  data;
    std::array<QRect, 2>                                       regions;
    QRhiTexture                                               *source     = nullptr;
    quint64                                                    generation = 0;
    ~Level()
    {
        for (auto &p : pipelines)
            p.reset();
        for (auto &b : bindings)
            b.reset();
        for (auto &t : targets)
            t.reset();
        for (auto &d : descriptors)
            d.reset();
    }
};
// An output pixel is valid only when its whole area is inside this native tile.
// Extra native padding provides the neighboring output pixel needed for filtering.
QPair<int, int> extent(int first, int length, int original, int reduced)
{
    return {int(std::ceil(double(first) * reduced / original)),
            int(std::floor(double(first + length) * reduced / original))};
}
} // namespace
struct DisplayPyramidPass::Data {
    QRhi                             *rhi;
    QRect                             native;
    QSize                             document;
    int                               bits;
    std::unique_ptr<QRhiSampler>      sampler;
    std::unique_ptr<QRhiBuffer>       vertices;
    QMap<int, std::shared_ptr<Level>> levels;
    bool                              uploaded = false;
    bool                              init()
    {
        if (vertices)
            return true;
        vertices.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 64));
        sampler.reset(rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        return vertices->create() && sampler->create();
    }
    bool binding(Level &level, int axis, QRhiTexture *source)
    {
        level.bindings[axis].reset(rhi->newShaderResourceBindings());
        const auto stages =
            QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
        level.bindings[axis]->setBindings(
            {QRhiShaderResourceBinding::uniformBuffer(0, stages, level.uniforms[axis].get()),
             QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                       source, sampler.get())});
        return level.bindings[axis]->create();
    }
};
DisplayPyramidPass::DisplayPyramidPass(QRhi *rhi, const QRect &native, const QSize &document,
                                       int bits)
    : d(std::make_unique<Data>())
{
    d->rhi      = rhi;
    d->native   = native;
    d->document = document;
    d->bits     = bits;
}
DisplayPyramidPass::~DisplayPyramidPass() = default;
QRhiTexture *DisplayPyramidPass::ensure(QRhiTexture *source, int index)
{
    if (index < 1 || index > 4 || !d->init())
        return nullptr;
    if (auto level = d->levels.value(index)) {
        if (level->source != source) {
            if (!d->binding(*level, 0, source))
                return nullptr;
            level->source     = source;
            level->generation = 0;
        }
        return level->textures[1].get();
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
        level->textures[axis].reset(d->rhi->newTexture(
            axis == 0 || d->bits == 16 ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8, target.size(),
            1, QRhiTexture::RenderTarget));
        if (!level->textures[axis]->create())
            return nullptr;
        level->targets[axis].reset(d->rhi->newTextureRenderTarget(
            QRhiTextureRenderTargetDescription(QRhiColorAttachment(level->textures[axis].get()))));
        level->descriptors[axis].reset(level->targets[axis]->newCompatibleRenderPassDescriptor());
        level->targets[axis]->setRenderPassDescriptor(level->descriptors[axis].get());
        level->uniforms[axis].reset(
            d->rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 128));
        if (!level->targets[axis]->create() || !level->uniforms[axis]->create() ||
            !d->binding(*level, axis, axis == 0 ? source : level->textures[0].get()))
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
        auto &pipeline = level->pipelines[axis];
        pipeline.reset(d->rhi->newGraphicsPipeline());
        pipeline->setShaderStages({{QRhiShaderStage::Vertex, load("pyramid.vert")},
                                   {QRhiShaderStage::Fragment, load("pyramid.frag")}});
        QRhiVertexInputLayout layout;
        layout.setBindings({QRhiVertexInputBinding(4 * sizeof(float))});
        layout.setAttributes(
            {QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
             QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float))});
        pipeline->setVertexInputLayout(layout);
        pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        pipeline->setTargetBlends({QRhiGraphicsPipeline::TargetBlend{}});
        pipeline->setShaderResourceBindings(level->bindings[axis].get());
        pipeline->setRenderPassDescriptor(level->descriptors[axis].get());
        if (!pipeline->create())
            return nullptr;
    }
    level->source = source;
    d->levels.insert(index, level);
    return level->textures[1].get();
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
    auto *updates = d->rhi->nextResourceUpdateBatch();
    if (!d->uploaded) {
        const float quad[] = {-1, -1, 0, 1, 1, -1, 1, 1, -1, 1, 0, 0, 1, 1, 1, 0};
        updates->uploadStaticBuffer(d->vertices.get(), quad);
        d->uploaded = true;
    }
    for (int axis = 0; axis < 2; ++axis)
        updates->updateDynamicBuffer(level->uniforms[axis].get(), 0, 128,
                                     level->data[axis].constData());
    cb->resourceUpdate(updates);
    for (int axis = 0; axis < 2; ++axis) {
        cb->beginPass(level->targets[axis].get(), Qt::transparent, {1.f, 0});
        cb->setGraphicsPipeline(level->pipelines[axis].get());
        const auto size = level->regions[axis].size();
        cb->setViewport(QRhiViewport(0, 0, size.width(), size.height()));
        cb->setShaderResources(level->bindings[axis].get());
        const QRhiCommandBuffer::VertexInput input{d->vertices.get(), 0};
        cb->setVertexInput(0, 1, &input);
        cb->draw(4);
        cb->endPass();
    }
    level->generation = generation;
    return 2;
}
qint64 DisplayPyramidPass::bytes() const
{
    qint64 result = d->vertices ? 64 : 0;
    for (const auto &level : d->levels)
        for (int axis = 0; axis < 2; ++axis) {
            const auto size = level->regions[axis].size();
            result +=
                qint64(size.width()) * size.height() * (axis == 0 || d->bits == 16 ? 16 : 4) + 128;
        }
    return result;
}
#else
struct DisplayPyramidPass::Data {};
DisplayPyramidPass::DisplayPyramidPass(QRhi *, const QRect &, const QSize &, int)
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
