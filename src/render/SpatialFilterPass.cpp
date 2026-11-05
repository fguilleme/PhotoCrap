#include "SpatialFilterPass.h"
#ifdef PHOTOCRAP_RHI_STACK
#include "LayerStack.h"
#include <QFile>
#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <vector>
namespace
{
struct Step {
    int                                         target = 0;
    quint64                                     sourceId = 0;
    QRhiTexture                                *source = nullptr, *original = nullptr;
    std::unique_ptr<QRhiBuffer>                 uniform;
    std::unique_ptr<QRhiShaderResourceBindings> bindings;
    std::optional<std::array<char, 96>>         uploadedUniform;
};
struct Job {
    std::vector<Step> steps;
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
    std::unique_ptr<QRhiShaderResourceBindings>              layoutBindings;
    std::vector<Job>                                         jobs;
    size_t                                                   preparedJobs = 0;
    bool                                                     secondTargetUsed = false;
    PhotoCrap::Render::StackStatistics                      *statistics;

    void setBindings(QRhiShaderResourceBindings &bindings, QRhiBuffer *buffer, QRhiTexture *source,
                     QRhiTexture *original) const
    {
        const auto stage = QRhiShaderResourceBinding::FragmentStage;
        bindings.setBindings(
            {QRhiShaderResourceBinding::uniformBuffer(
                 0, stage | QRhiShaderResourceBinding::VertexStage, buffer),
             QRhiShaderResourceBinding::sampledTexture(1, stage, source, sampler.get()),
             QRhiShaderResourceBinding::sampledTexture(2, stage, original, sampler.get())});
    }

    bool initLayout()
    {
        if (layoutBindings)
            return true;
        auto bindings =
            std::unique_ptr<QRhiShaderResourceBindings>(rhi->newShaderResourceBindings());
        setBindings(*bindings, nullptr, nullptr, nullptr);
        if (!bindings->create())
            return false;
        layoutBindings = std::move(bindings);
        return true;
    }
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
        return true;
    }

    bool ensureTarget(int index)
    {
        if (targets[index])
            return true;
        auto texture = std::unique_ptr<QRhiTexture>(
            rhi->newTexture(bits == 16 || index == 0 ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8,
                            size, 1, QRhiTexture::RenderTarget));
        if (!texture->create())
            return false;
        std::unique_ptr<QRhiRenderPassDescriptor> descriptor;
        auto target = std::unique_ptr<QRhiTextureRenderTarget>(rhi->newTextureRenderTarget(
            QRhiTextureRenderTargetDescription(QRhiColorAttachment(texture.get()))));
        descriptor.reset(target->newCompatibleRenderPassDescriptor());
        if (!descriptor)
            return false;
        target->setRenderPassDescriptor(descriptor.get());
        if (!target->create())
            return false;

        static const QShader vertexShader   = loadShader("stack.vert");
        static const QShader fragmentShader = loadShader("spatial.frag");
        auto pipeline = std::unique_ptr<QRhiGraphicsPipeline>(rhi->newGraphicsPipeline());
        pipeline->setShaderStages(
            {{QRhiShaderStage::Vertex, vertexShader}, {QRhiShaderStage::Fragment, fragmentShader}});
        QRhiVertexInputLayout layout;
        layout.setBindings({QRhiVertexInputBinding(16)});
        layout.setAttributes({QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
                              QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 8)});
        pipeline->setVertexInputLayout(layout);
        pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        pipeline->setShaderResourceBindings(layoutBindings.get());
        pipeline->setRenderPassDescriptor(descriptor.get());
        if (!pipeline->create())
            return false;

        textures[index]    = std::move(texture);
        targets[index]     = std::move(target);
        descriptors[index] = std::move(descriptor);
        pipelines[index]   = std::move(pipeline);
        return true;
    }
    bool prepareStep(Step &step, QRhiTexture *source, QRhiTexture *original, quint64 sourceId,
                     int target, int operation, int radius, bool vertical, float a, float b,
                     QRhiResourceUpdateBatch *updates, bool roundBox2D, bool uploaded)
    {
        step.target = target;
        if (!step.uniform) {
            auto uniform = std::unique_ptr<QRhiBuffer>(
                rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 96));
            if (!uniform->create())
                return false;
            step.uniform = std::move(uniform);
        }
        std::array<char, 96> data{};
        const auto matrix = rhi->clipSpaceCorrMatrix();
        std::memcpy(data.data(), matrix.constData(), 64);
        const qint32 modes[]  = {operation, radius, vertical,
                                 rhi->isYUpInFramebuffer() && !uploaded};
        const float  values[] = {bits == 16 ? 65535.f : 255.f, a, b, roundBox2D ? 1.f : 0.f};
        std::memcpy(data.data() + 64, modes, sizeof(modes));
        std::memcpy(data.data() + 80, values, sizeof(values));
        if (step.uploadedUniform != data) {
            updates->updateDynamicBuffer(step.uniform.get(), 0, data.size(), data.data());
            step.uploadedUniform = data;
            ++statistics->uniformUpdates;
        }
        // The mask input may be replaced by an allocation at the same address.
        if (!step.bindings || step.sourceId != sourceId || step.source != source ||
            step.original != original) {
            auto bindings =
                std::unique_ptr<QRhiShaderResourceBindings>(rhi->newShaderResourceBindings());
            setBindings(*bindings, step.uniform.get(), source, original);
            if (!bindings->create())
                return false;
            step.bindings = std::move(bindings);
            step.sourceId = sourceId;
            step.source   = source;
            step.original = original;
            ++statistics->bindingCreations;
        }
        return true;
    }
};
SpatialFilterPass::SpatialFilterPass(QRhi *rhi, const QSize &size, int bits,
                                     PhotoCrap::Render::StackStatistics &statistics)
    : d(std::make_unique<Data>())
{
    d->rhi  = rhi;
    d->size = size;
    d->bits = bits;
    d->statistics = &statistics;
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
void SpatialFilterPass::resetFrame(bool reuseSteps)
{
    d->preparedJobs = 0;
    d->secondTargetUsed = false;
    // A shared workspace can be prepared by several tiles in one command
    // buffer. Each tile then needs independent dynamic uniform storage.
    if (!reuseSteps)
        d->jobs.clear();
}
void SpatialFilterPass::finishPreparation()
{
    d->jobs.resize(d->preparedJobs);
    if (!d->secondTargetUsed && d->targets[1]) {
        d->pipelines[1].release()->deleteLater();
        d->targets[1].release()->deleteLater();
        d->descriptors[1].release()->deleteLater();
        d->textures[1].release()->deleteLater();
    }
}
SpatialFilterPass::Run
SpatialFilterPass::prepare(QRhiTexture                                    *source,
                           const PhotoCrap::Render::SpatialFilterSettings &settings,
                           QRhiResourceUpdateBatch *updates, bool uploaded, quint64 sourceId)
{
    using PhotoCrap::Render::SpatialFilter;
    if (!source || !d->init(updates) || !d->initLayout())
        return {};
    if (d->preparedJobs == d->jobs.size())
        d->jobs.emplace_back();
    auto        &steps         = d->jobs[d->preparedJobs].steps;
    size_t       preparedSteps = 0;
    QRhiTexture *current = source;
    const auto   append  = [&](int operation, int radius, bool vertical, float a = 0, float b = 0) {
        const int target = current == d->textures[0].get() ? 1 : 0;
        if (!d->ensureTarget(target))
            return false;
        d->secondTargetUsed |= target == 1;
        if (preparedSteps == steps.size())
            steps.emplace_back();
        if (!d->prepareStep(steps[preparedSteps], current, source, sourceId, target, operation,
                            radius, vertical, a, b, updates, settings.roundBox2D,
                            uploaded && current == source))
            return false;
        ++preparedSteps;
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
    steps.resize(preparedSteps);
    return {int(d->preparedJobs++), current};
}
quint64 SpatialFilterPass::render(QRhiCommandBuffer *cb, int run)
{
    if (run < 0 || run >= int(d->jobs.size()))
        return 0;
    for (const auto &step : d->jobs[run].steps) {
        cb->beginPass(d->targets[step.target].get(), Qt::transparent, {1.f, 0});
        cb->setGraphicsPipeline(d->pipelines[step.target].get());
        cb->setViewport(QRhiViewport(0, 0, d->size.width(), d->size.height()));
        cb->setShaderResources(step.bindings.get());
        const QRhiCommandBuffer::VertexInput input{d->vertices.get(), 0};
        cb->setVertexInput(0, 1, &input);
        cb->draw(4);
        cb->endPass();
    }
    return d->jobs[run].steps.size();
}
qint64 SpatialFilterPass::bytes() const
{
    const qint64 pixels = qint64(d->size.width()) * d->size.height();
    qint64       result = (d->vertices ? 64 : 0) + (d->targets[0] ? pixels * 16 : 0) +
                          (d->targets[1] ? pixels * (d->bits == 16 ? 16 : 4) : 0);
    for (const auto &job : d->jobs)
        result += qint64(job.steps.size()) * 96;
    return result;
}
#endif
