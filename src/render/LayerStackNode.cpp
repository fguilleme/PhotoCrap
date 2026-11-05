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
            colors[i].reset(rhi->newTexture(bits == 16 ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8,
                                            size, 1, QRhiTexture::RenderTarget));
            clips[i].reset(rhi->newTexture(QRhiTexture::R8, size, 1, QRhiTexture::RenderTarget));
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
    QSize                                      tableSize{1, 1}, blendSize{1, 1};
    QSize                                      colorSize{1, 1}, maskSize{1, 1};
    QRectF                                     geometry;
    PhotoCrap::Render::LayerStack              stack;
    QByteArray                                 renderedKey;
    quint64                                    snapshotRevision = 0, renderedRevision = 0;
    int                                        renderedDisplayLevel = -1;
    std::shared_ptr<Workspace>                 work;
    bool                                       sharedWork = false;
    std::shared_ptr<DisplayPyramidScratchPool> reductionPool;
    std::unique_ptr<QRhiTexture>               retained;
    std::unique_ptr<QRhiTextureRenderTarget>   retainedTarget;
    std::unique_ptr<QRhiSampler>               sampler;
    std::unique_ptr<QRhiBuffer>                vertices;
    std::unique_ptr<QRhiGraphicsPipeline>      pipeline;
    std::vector<Pass>                          passes;
    QSGImageNode                              *image          = nullptr;
    QSGTexture                                *wrapper        = nullptr;
    int                                        retainedCoverage = -1;
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
    void setStack(const PhotoCrap::Render::LayerStack &value, quint64 revision,
                  const QRect &content, const QRectF &rect)
    {
        stack            = value;
        snapshotRevision = revision;
        displayLevel     = stack.displayLevel;
        if (pyramidDocument != stack.size) {
            pyramid.reset();
            pyramidDocument = stack.size;
        }
        const int last       = stack.layers.isEmpty() ? 0 : int(stack.layers.size()) % 2;
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
                renderedKey.clear();
                renderedRevision = 0;
                retained.reset(
                    rhi->newTexture(bits == 16 ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8,
                                    region.size(), 1, QRhiTexture::RenderTarget));
                if (!retained->create()) {
                    reject(__LINE__);
                    return;
                }
            }
            if (!retainedTarget || retainedCoverage != last) {
                // The last pass owns its color output. Coverage remains in the
                // shared scratch slot opposite the coverage being sampled.
                QRhiTextureRenderTargetDescription description;
                description.setColorAttachments({QRhiColorAttachment(retained.get()),
                                                 QRhiColorAttachment(work->clips[last].get())});
                retainedTarget.reset(rhi->newTextureRenderTarget(description));
                retainedTarget->setRenderPassDescriptor(work->descriptor.get());
                if (!retainedTarget->create()) {
                    reject(__LINE__);
                    return;
                }
                retainedCoverage = last;
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
        if (sharedWork && displayLevel > 0) {
            retainedTarget.reset();
            retained.reset(); // Reduced views retain only the reduced output.
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
                                 QRhiResourceUpdateBatch *&updates, QSet<QByteArray> &used,
                                 int whole = 0, bool stageOnly = false)
    {
        QByteArray key = whole == 2 ? "b" : whole ? "t" : mask ? "m" : "c";
        const bool compact = !whole && PhotoCrap::Render::compactStackInput(plane, stack.size);
        // Whole work->inputs are immutable LUT QImages reused by the document cache.
        // Freezing a fresh wrapper would manufacture new tile versions each frame.
        key += whole ? QByteArray::number(plane.cacheKey())
                     : plane.regionKey(compact ? QRect(0, 0, 1, 1) : region);
        const QByteArray contentKey = key;
        if (streaming)
            key = whole == 2 ? "stream-blend"
                  : whole    ? "stream-table"
                  : mask     ? "stream-mask"
                             : "stream-color";
        used.insert(key);
        auto value = work->inputs.value(key);
        if (value && (!streaming || stageOnly || value->contentKey == contentKey))
            return value;
        const QSize extent = streaming ? (whole == 2 ? blendSize
                                          : whole    ? tableSize
                                          : mask     ? maskSize
                                                     : colorSize)
                             : compact ? QSize(1, 1)
                             : whole   ? plane.size()
                                       : region.size();
        if (!value) {
            value = std::make_shared<Input>();
            value->texture.reset(rhi->newTexture(mask                       ? QRhiTexture::R8
                                                 : bits == 16 || whole == 2 ? QRhiTexture::RGBA32F
                                                                            : QRhiTexture::RGBA8,
                                                 extent));
            if (!value->texture->create())
                return {};
            value->bytes = qint64(extent.width()) * extent.height() *
                           (mask                       ? 1
                            : bits == 16 || whole == 2 ? 16
                                                       : 4);
            work->inputs.insert(key, value);
        }
        if (streaming && stageOnly)
            return value;
        QImage     pixels;
        const auto format = whole == 2   ? QImage::Format_RGBA32FPx4
                            : mask       ? QImage::Format_Grayscale8
                            : bits == 16 ? QImage::Format_RGBA32FPx4_Premultiplied
                                         : QImage::Format_RGBA8888_Premultiplied;
        if (plane.isNull()) {
            pixels = QImage(1, 1, format);
            pixels.fill(mask ? Qt::white : Qt::transparent);
        } else {
            pixels = whole || plane.size() == QSize(1, 1) ? QImage(plane)
                     : compact                            ? plane.tiles().backgroundPixel()
                                                          : plane.region(region);
            pixels = pixels.convertToFormat(format);
        }
        if (pixels.isNull())
            return {};
        QRhiTextureSubresourceUploadDescription data(pixels.constBits(),
                                                     quint32(pixels.sizeInBytes()));
        data.setDataStride(quint32(pixels.bytesPerLine()));
        data.setSourceSize(pixels.size());
        if (!updates)
            updates = rhi->nextResourceUpdateBatch();
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
        const auto reduce = [&](QRhiCommandBuffer *cb) {
            if (pyramid && displayLevel > 0)
                counters->reductions.fetch_add(pyramid->render(cb, displayLevel, generation));
        };
        // A geometry-only frame supplies the same immutable source snapshot.
        // Shared scratch must still be regenerated for a different zoom level.
        if (renderedRevision != 0 && snapshotRevision == renderedRevision &&
            (!sharedWork || displayLevel == renderedDisplayLevel)) {
            reduce(commandBuffer());
            return;
        }
        QByteArray key;
        for (const auto &layer : layers) {
            for (const auto &part :
                 {layer.needsColorInput() ? layer.image.regionKey(region) : QByteArray{},
                  layer.needsMaskInput() ? layer.mask.regionKey(region) : QByteArray{},
                  QByteArray::number(layer.toneTable.cacheKey()),
                  QByteArray::number(layer.blendTable.cacheKey())}) {
                const qint64 size = part.size();
                key.append(reinterpret_cast<const char *>(&size), sizeof(size));
                key += part;
            }
            key.append(reinterpret_cast<const char *>(&layer.opacity), sizeof(layer.opacity));
            key.append(reinterpret_cast<const char *>(&layer.density), sizeof(layer.density));
            key.append(char(layer.blend));
            key.append(char(layer.clipped));
            key.append(char(layer.reset));
            key.append(char(layer.tonal));
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
            renderedRevision     = snapshotRevision;
            renderedDisplayLevel = displayLevel;
            reduce(commandBuffer());
            return;
        }
        if (streaming) {
            tableSize = blendSize = colorSize = maskSize = QSize(1, 1);
            for (const auto &layer : layers) {
                tableSize = tableSize.expandedTo(layer.toneTable.size());
                blendSize = blendSize.expandedTo(layer.blendTable.size());
                if (layer.needsColorInput() &&
                    !PhotoCrap::Render::compactStackInput(layer.image, stack.size))
                    colorSize = region.size();
                if (layer.needsMaskInput() &&
                    !PhotoCrap::Render::compactStackInput(layer.mask, stack.size))
                    maskSize = region.size();
            }
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
        if (passes.size() > size_t(layers.size()))
            passes.resize(size_t(layers.size()));
        if (streaming) {
            const std::pair<const char *, QSize> extents[] = {{"stream-table", tableSize},
                                                              {"stream-blend", blendSize},
                                                              {"stream-color", colorSize},
                                                              {"stream-mask", maskSize}};
            for (const auto &[key, extent] : extents) {
                const auto slot = work->inputs.value(key);
                if (slot && slot->texture->pixelSize() != extent)
                    work->inputs.remove(key);
            }
        }
        bool ok = true;
        for (int index = 0; index < layers.size(); ++index) {
            const auto &layer = layers[index];
            const auto  color = layer.needsColorInput()
                                    ? input(layer.image, false, updates, used, false, streaming)
                                    : nullptr;
            const auto  mask  = layer.needsMaskInput()
                                    ? input(layer.mask, true, updates, used, false, streaming)
                                    : nullptr;
            const auto  table = layer.toneTable.isNull()
                                    ? nullptr
                                    : input(layer.toneTable, false, updates, used, true, streaming);
            const auto  blend = layer.blendTable.isNull()
                                    ? nullptr
                                    : input(layer.blendTable, false, updates, used, 2, streaming);
            if ((layer.needsColorInput() && !color) || (layer.needsMaskInput() && !mask) ||
                (!layer.toneTable.isNull() && !table) || (!layer.blendTable.isNull() && !blend)) {
                ok = false;
                break;
            }
            // The shader still needs valid bindings for samplers it does not read.
            auto        *backdrop   = work->colors[index % 2].get();
            auto        *coverage   = work->clips[index % 2].get();
            QRhiTexture *maskSource = mask ? mask->texture.get() : coverage;
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
            QRhiTexture *source = color ? color->texture.get() : backdrop;
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
            // Keep the per-tile uniform buffers across edits. Bindings are
            // rebuilt because a texture can change independently of the slot.
            if (size_t(index) == passes.size()) {
                Pass created;
                created.uniform.reset(
                    rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 256));
                if (!created.uniform->create()) {
                    ok = false;
                    break;
                }
                passes.push_back(std::move(created));
            }
            auto                 &pass = passes[size_t(index)];
            std::array<char, 256> uniform{};
            const auto matrix = rhi->clipSpaceCorrMatrix();
            std::memcpy(uniform.data(), matrix.constData(), 64);
            const qint32 modes[] = {
                layer.blend, layer.clipped,
                int(layer.reset) + (run >= 0 ? 2 : 0) + (layer.tonal ? 4 : 0) +
                    (maskRun >= 0 ? 8 : 0) +
                    (streaming && PhotoCrap::Render::compactStackInput(layer.image, stack.size) &&
                             run < 0
                         ? 16
                         : 0) +
                    (streaming && PhotoCrap::Render::compactStackInput(layer.mask, stack.size) &&
                             maskRun < 0
                         ? 32
                         : 0) +
                    (!layer.needsMaskInput() ? 64 : 0),
                region.x() | (region.y() << 14) | (index == 0 ? 1 << 30 : 0)};
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
            updates->updateDynamicBuffer(pass.uniform.get(), 0, 256, uniform.data());
            pass.bindings.reset(rhi->newShaderResourceBindings());
            const auto stage = QRhiShaderResourceBinding::FragmentStage;
            pass.bindings->setBindings(
                {QRhiShaderResourceBinding::uniformBuffer(
                     0, stage | QRhiShaderResourceBinding::VertexStage, pass.uniform.get()),
                 QRhiShaderResourceBinding::sampledTexture(1, stage, backdrop, sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(2, stage, source, sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(3, stage, maskSource, sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(4, stage, coverage, sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(
                     5, stage, table ? table->texture.get() : backdrop, sampler.get()),
                 QRhiShaderResourceBinding::sampledTexture(
                     6, stage, blend ? blend->texture.get() : backdrop, sampler.get())});
            if (!pass.bindings->create()) {
                ok = false;
                break;
            }
        }
        if (ok && !pipeline) {
            static const QShader vertexShader   = shader("stack_tonal.vert");
            static const QShader fragmentShader = shader("stack.frag");
            pipeline.reset(rhi->newGraphicsPipeline());
            pipeline->setShaderStages({{QRhiShaderStage::Vertex, vertexShader},
                                       {QRhiShaderStage::Fragment, fragmentShader}});
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
            pipeline->setShaderResourceBindings(passes.front().bindings.get());
            pipeline->setRenderPassDescriptor(work->descriptor.get());
            ok = pipeline->create();
        }
        if (!ok) {
            updateBytes();
            updates->release();
            reject(__LINE__);
            return;
        }
        pipeline->setShaderResourceBindings(passes.front().bindings.get());
        auto *cb = commandBuffer();
        cb->resourceUpdate(updates);
        if (!filterRuns.empty() && (filterRuns.front() >= 0 || layers.front().tonal)) {
            cb->beginPass(work->targets[0].get(), Qt::transparent, {1.f, 0});
            cb->endPass();
        }
        for (size_t index = 0; index < passes.size(); ++index) {
            if (streaming) {
                QRhiResourceUpdateBatch *upload = nullptr;
                const auto &layer  = layers[qsizetype(index)];
                const bool  valid =
                    (!layer.needsColorInput() || input(layer.image, false, upload, used)) &&
                    (!layer.needsMaskInput() || input(layer.mask, true, upload, used)) &&
                    (layer.toneTable.isNull() ||
                     input(layer.toneTable, false, upload, used, true)) &&
                    (layer.blendTable.isNull() || input(layer.blendTable, false, upload, used, 2));
                if (!valid) {
                    if (upload)
                        upload->release();
                    reject(__LINE__);
                    return;
                }
                if (upload)
                    cb->resourceUpdate(upload);
            }
            if (maskRuns[index] >= 0)
                counters->passes.fetch_add(work->masks->render(cb, maskRuns[index]));
            if (filterRuns[index] >= 0)
                counters->passes.fetch_add(work->spatial->render(cb, filterRuns[index]));
            auto *target = index + 1 == passes.size() && retainedTarget
                               ? retainedTarget.get()
                               : work->targets[(index + 1) % 2].get();
            cb->beginPass(target, Qt::transparent, {1.f, 0});
            cb->setGraphicsPipeline(pipeline.get());
            cb->setViewport(QRhiViewport(0, 0, region.width(), region.height()));
            cb->setShaderResources(passes[index].bindings.get());
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
        renderedRevision     = snapshotRevision;
        renderedDisplayLevel = displayLevel;
        ++generation;
        reduce(cb);
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
    quint64                                     clock     = 0;
    int                                         bits      = 0;
    bool                                        streaming = false, sharing = false;
    QSize                                       document;
    QHash<QSize, std::weak_ptr<Workspace>>      workspaces;
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
bool LayerStackNode::update(const PhotoCrap::Render::LayerStack &stack, quint64 snapshotRevision,
                            const QRectF &item, const QRectF &visible, qint64 cacheBytes)
{
    const double sx = item.width() / stack.size.width(), sy = item.height() / stack.size.height();
    const QRect  bounds(QPoint{}, stack.size);
    const QRect  area =
        QRectF(visible.x() / sx, visible.y() / sy, visible.width() / sx, visible.height() / sy)
            .toAlignedRect()
            .intersected(bounds);
    if (d->document != stack.size || d->bits != stack.bits || d->streaming != stack.streamInputs ||
        d->sharing != stack.shareScratch) {
        for (auto *tile : std::as_const(d->tiles)) {
            if (tile->parent())
                removeChildNode(tile);
            delete tile;
        }
        d->tiles.clear();
        d->lastUse.clear();
        d->workspaces.clear();
        d->reductions.reset();
        d->document  = stack.size;
        d->bits      = stack.bits;
        d->streaming = stack.streamInputs;
        d->sharing   = stack.shareScratch;
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
                // Table slots resize in prepare(). Their dimensions do not
                // change the color/coverage targets or their render passes.
                const QSize extent = padded.size();
                workspace          = d->workspaces.value(extent).lock();
                if (!workspace) {
                    workspace = std::make_shared<Workspace>(d->window->rhi(), extent, stack.bits,
                                                            d->counters.get());
                    d->workspaces.insert(extent, workspace);
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
            tile->setStack(stack, snapshotRevision, content, item);
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
bool LayerStackNode::update(const PhotoCrap::Render::LayerStack &, quint64, const QRectF &,
                            const QRectF &, qint64)
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
