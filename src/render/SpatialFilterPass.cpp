#include "SpatialFilterPass.h"
#ifdef PHOTOCRAP_RHI_STACK
#include <QFile>
#include <array>
#include <cmath>
#include <cstring>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <vector>
namespace
{
struct Step {
    int                                         target = 0;
    std::unique_ptr<QRhiBuffer>                 uniform;
    std::unique_ptr<QRhiShaderResourceBindings> bindings;
};
struct Job {
    std::vector<std::unique_ptr<Step>> steps;
};
QShader loadShader(const char *name)
{
    QFile file(QStringLiteral(":/PhotoCrap/src/render/shaders/") + QString::fromLatin1(name) +
               ".qsb");
    return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader{};
}
} // namespace
struct SpatialFilterPass::Data {
    QRhi                                                    *rhi;
    QSize                                                    size;
    int                                                      bits;
    std::array<std::unique_ptr<QRhiTexture>, 2>              textures;
    std::array<std::unique_ptr<QRhiTextureRenderTarget>, 2>  targets;
    std::array<std::unique_ptr<QRhiRenderPassDescriptor>, 2> descriptors;
    std::unique_ptr<QRhiBuffer>                              vertices;
    std::unique_ptr<QRhiSampler>                             sampler;
    std::array<std::unique_ptr<QRhiGraphicsPipeline>, 2>     pipelines;
    std::vector<Job>                                         jobs;
    bool                                                     init(QRhiResourceUpdateBatch *updates)
    {
        if (vertices)
            return true;
        sampler.reset(rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        vertices.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 64));
        if (!sampler->create() || !vertices->create())
            return false;
        const float quad[] = {-1, -1, 0, 1, 1, -1, 1, 1, -1, 1, 0, 0, 1, 1, 1, 0};
        updates->uploadStaticBuffer(vertices.get(), quad);
        for (int i = 0; i < 2; ++i) {
            textures[i].reset(
                rhi->newTexture(bits == 16 || i == 0 ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8,
                                size, 1, QRhiTexture::RenderTarget));
            if (!textures[i]->create())
                return false;
            targets[i].reset(rhi->newTextureRenderTarget(
                QRhiTextureRenderTargetDescription(QRhiColorAttachment(textures[i].get()))));
            descriptors[i].reset(targets[i]->newCompatibleRenderPassDescriptor());
            targets[i]->setRenderPassDescriptor(descriptors[i].get());
            if (!targets[i]->create())
                return false;
        }
        return true;
    }
    std::unique_ptr<Step> step(QRhiTexture *source, QRhiTexture *original, int target,
                               int operation, int radius, bool vertical, float a, float b,
                               QRhiResourceUpdateBatch *updates, bool roundBox2D, bool uploaded)
    {
        auto result    = std::make_unique<Step>();
        result->target = target;
        result->uniform.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 96));
        if (!result->uniform->create())
            return {};
        QByteArray data(96, 0);
        const auto matrix = rhi->clipSpaceCorrMatrix();
        std::memcpy(data.data(), matrix.constData(), 64);
        const qint32 modes[]  = {operation, radius, vertical,
                                 rhi->isYUpInFramebuffer() && !uploaded};
        const float  values[] = {bits == 16 ? 65535.f : 255.f, a, b, roundBox2D ? 1.f : 0.f};
        std::memcpy(data.data() + 64, modes, sizeof(modes));
        std::memcpy(data.data() + 80, values, sizeof(values));
        updates->updateDynamicBuffer(result->uniform.get(), 0, 96, data.constData());
        result->bindings.reset(rhi->newShaderResourceBindings());
        const auto stage = QRhiShaderResourceBinding::FragmentStage;
        result->bindings->setBindings(
            {QRhiShaderResourceBinding::uniformBuffer(
                 0, stage | QRhiShaderResourceBinding::VertexStage, result->uniform.get()),
             QRhiShaderResourceBinding::sampledTexture(1, stage, source, sampler.get()),
             QRhiShaderResourceBinding::sampledTexture(2, stage, original, sampler.get())});
        if (!result->bindings->create())
            return {};
        return result;
    }
};
SpatialFilterPass::SpatialFilterPass(QRhi *rhi, const QSize &size, int bits)
    : d(std::make_unique<Data>())
{
    d->rhi  = rhi;
    d->size = size;
    d->bits = bits;
}
SpatialFilterPass::~SpatialFilterPass()
{
    d->jobs.clear();
    for (auto &pipeline : d->pipelines)
        pipeline.reset();
    for (auto &target : d->targets)
        target.reset();
    for (auto &descriptor : d->descriptors)
        descriptor.reset();
}
void SpatialFilterPass::resetFrame()
{
    d->jobs.clear();
}
SpatialFilterPass::Run
SpatialFilterPass::prepare(QRhiTexture                                    *source,
                           const PhotoCrap::Render::SpatialFilterSettings &settings,
                           QRhiResourceUpdateBatch *updates, bool uploaded)
{
    using PhotoCrap::Render::SpatialFilter;
    if (!source || !d->init(updates))
        return {};
    Job          job;
    QRhiTexture *current = source;
    const auto   append  = [&](int operation, int radius, bool vertical, float a = 0, float b = 0) {
        const int target = current == d->textures[0].get() ? 1 : 0;
        auto step = d->step(current, source, target, operation, radius, vertical, a, b, updates,
                            settings.roundBox2D, uploaded && current == source);
        if (!step)
            return false;
        job.steps.push_back(std::move(step));
        current = d->textures[target].get();
        return true;
    };
    if (settings.type == SpatialFilter::Motion) {
        const int    samples = qBound(3, qRound(settings.distance / 4.) * 2 + 1, 33);
        const double angle   = qDegreesToRadians(double(settings.angle));
        if (!append(1, samples / 2, false,
                    float(std::cos(angle) * settings.distance / (samples - 1)),
                    float(std::sin(angle) * settings.distance / (samples - 1))))
            return {};
    } else {
        const int count = settings.type == SpatialFilter::Box ? 1 : 3;
        for (int i = 0; i < count; ++i)
            if (!append(0, settings.radius, false) || !append(0, settings.radius, true))
                return {};
        if (settings.type == SpatialFilter::Unsharp || settings.type == SpatialFilter::HighPass)
            if (!append(settings.type == SpatialFilter::Unsharp ? 2 : 3, 0, false,
                        settings.amount / 100.f,
                        settings.threshold * (d->bits == 16 ? 257.f : 1.f)))
                return {};
    }
    for (int target = 0; target < 2; ++target)
        if (!d->pipelines[target]) {
            static const QShader vertexShader   = loadShader("stack.vert");
            static const QShader fragmentShader = loadShader("spatial.frag");
            d->pipelines[target].reset(d->rhi->newGraphicsPipeline());
            d->pipelines[target]->setShaderStages({{QRhiShaderStage::Vertex, vertexShader},
                                                   {QRhiShaderStage::Fragment, fragmentShader}});
            QRhiVertexInputLayout layout;
            layout.setBindings({QRhiVertexInputBinding(16)});
            layout.setAttributes(
                {QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
                 QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 8)});
            d->pipelines[target]->setVertexInputLayout(layout);
            d->pipelines[target]->setTopology(QRhiGraphicsPipeline::TriangleStrip);
            d->pipelines[target]->setShaderResourceBindings(job.steps.front()->bindings.get());
            d->pipelines[target]->setRenderPassDescriptor(d->descriptors[target].get());
            if (!d->pipelines[target]->create())
                return {};
        }
    const int index = int(d->jobs.size());
    d->jobs.push_back(std::move(job));
    return {index, current};
}
quint64 SpatialFilterPass::render(QRhiCommandBuffer *cb, int run)
{
    if (run < 0 || run >= int(d->jobs.size()))
        return 0;
    for (const auto &step : d->jobs[run].steps) {
        cb->beginPass(d->targets[step->target].get(), Qt::transparent, {1.f, 0});
        d->pipelines[step->target]->setShaderResourceBindings(step->bindings.get());
        cb->setGraphicsPipeline(d->pipelines[step->target].get());
        cb->setViewport(QRhiViewport(0, 0, d->size.width(), d->size.height()));
        cb->setShaderResources(step->bindings.get());
        const QRhiCommandBuffer::VertexInput input{d->vertices.get(), 0};
        cb->setVertexInput(0, 1, &input);
        cb->draw(4);
        cb->endPass();
    }
    return d->jobs[run].steps.size();
}
qint64 SpatialFilterPass::bytes() const
{
    qint64 result =
        d->vertices ? qint64(d->size.width()) * d->size.height() * (d->bits == 16 ? 32 : 20) + 64
                    : 0;
    for (const auto &job : d->jobs)
        result += qint64(job.steps.size()) * 96;
    return result;
}
#endif
