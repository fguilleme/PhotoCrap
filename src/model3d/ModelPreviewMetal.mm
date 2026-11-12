#include "Model3DPrivate.h"

#import <Metal/Metal.h>
#include <QColorSpace>
#include <QMatrix4x4>
#include <QMutex>
#include <cstddef>
#include <cstring>
#import <simd/simd.h>
#include <unordered_map>
#include <vector>

namespace PhotoCrap::Model3D
{
namespace
{
constexpr auto ColorFormat = MTLPixelFormatBGRA8Unorm_sRGB;
constexpr auto DepthFormat = MTLPixelFormatDepth32Float;

constexpr const char *Shader = R"metal(
#include <metal_stdlib>
using namespace metal;

struct Vertex {
    float3 position [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float2 uv [[attribute(2)]];
};
struct View {
    float4x4 clipFromModel, rotation;
    float4 directionAndIntensity, colorAndAmbient;
};
struct Material {
    float4 color;
    float cutoff;
    uint unlit, vertexColors, mirrored;
};
struct Varying {
    float4 clip [[position]];
    float3 position, normal;
    float2 uv;
    float4 color;
};

vertex Varying modelVertex(Vertex v [[stage_in]], uint index [[vertex_id]],
                           constant View &view [[buffer(1)]],
                           constant Material &material [[buffer(2)]],
                           device const float4 *colors [[buffer(3)]])
{
    return {view.clipFromModel * float4(v.position, 1.0),
            (view.rotation * float4(v.position, 1.0)).xyz,
            (view.rotation * float4(v.normal, 0.0)).xyz, v.uv,
            material.vertexColors ? colors[index] : float4(1.0)};
}

fragment float4 modelFragment(Varying v [[stage_in]], bool front [[front_facing]],
                              constant View &view [[buffer(1)]],
                              constant Material &material [[buffer(2)]],
                              texture2d<float> image [[texture(0)]],
                              sampler imageSampler [[sampler(0)]])
{
    const float4 texel = image.sample(imageSampler, v.uv);
    const float4 base = material.vertexColors ? material.color * v.color : material.color;
    if (texel.a * base.a < material.cutoff)
        discard_fragment();
    float3 light(1.0);
    if (!material.unlit) {
        // Screen-space derivatives give a camera-facing normal for meshes with no normals.
        const float3 flat = cross(dfdy(v.position), dfdx(v.position));
        const float3 normal = dot(v.normal, v.normal) > 1e-12
            ? normalize(v.normal) * (front ? 1.0 : -1.0)
            : normalize(flat) * (material.mirrored ? -1.0 : 1.0);
        const float diffuse = view.directionAndIntensity.w
                            * max(0.0, dot(normal, view.directionAndIntensity.xyz));
        light = view.colorAndAmbient.w + diffuse * view.colorAndAmbient.xyz;
    }
    return float4(saturate(base.rgb * texel.rgb * light), 1.0);
}
)metal";

struct ViewUniforms {
    simd_float4x4 clipFromModel, rotation;
    simd_float4   directionAndIntensity, colorAndAmbient;
};
struct MaterialUniforms {
    simd_float4 color;
    float       cutoff;
    quint32     unlit, vertexColors, mirrored;
};

struct MeshPrimitive {
    id<MTLBuffer>       vertices, indices, colors;
    id<MTLTexture>      texture;
    id<MTLSamplerState> sampler;
    NSUInteger          indexCount;
    MaterialUniforms    material;
    bool                doubleSided;
};

MTLSamplerAddressMode addressMode(Material::Wrap _wrap)
{
    switch (_wrap) {
    case Material::Wrap::clamp:
        return MTLSamplerAddressModeClampToEdge;
    case Material::Wrap::mirror:
        return MTLSamplerAddressModeMirrorRepeat;
    case Material::Wrap::repeat:
        return MTLSamplerAddressModeRepeat;
    }
}

id<MTLTexture> uploadTexture(id<MTLDevice> _device, const QImage &_image)
{
    auto *desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:ColorFormat
                                                           width:NSUInteger(_image.width())
                                                          height:NSUInteger(_image.height())
                                                       mipmapped:NO];
    desc.storageMode = _device.hasUnifiedMemory ? MTLStorageModeShared : MTLStorageModeManaged;
    desc.usage       = MTLTextureUsageShaderRead;
    auto texture     = [_device newTextureWithDescriptor:desc];
    [texture replaceRegion:MTLRegionMake2D(0, 0, desc.width, desc.height)
               mipmapLevel:0
                 withBytes:_image.constBits()
               bytesPerRow:NSUInteger(_image.bytesPerLine())];
    return texture;
}

id<MTLTexture> renderTarget(id<MTLDevice> _device, MTLPixelFormat _format, const QSize &_canvas)
{
    auto *desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:_format
                                                           width:NSUInteger(_canvas.width())
                                                          height:NSUInteger(_canvas.height())
                                                       mipmapped:NO];
    desc.storageMode = MTLStorageModePrivate;
    desc.usage       = MTLTextureUsageRenderTarget;
    return [_device newTextureWithDescriptor:desc];
}

ViewUniforms uniforms(const RenderView &_view, const QSize &_canvas)
{
    const auto &map = _view.placement;
    const float sx  = 2.f * CameraDistance * _view.scale / _canvas.width();
    const float sy  = 2.f * CameraDistance * _view.scale / _canvas.height();
    const float dx  = float(2. * map.dx() / _canvas.width() - 1.);
    const float dy  = float(1. - 2. * map.dy() / _canvas.height());
    // w = camera distance; NDC depth is 1 - 1/w, with near surfaces first.
    const QMatrix4x4 projection(sx * float(map.m11()), -sx *float(map.m21()), -dx,
                                CameraDistance *dx, -sy *float(map.m12()), sy *float(map.m22()),
                                -dy, CameraDistance *dy, 0.f, 0.f, -1.f, CameraDistance - 1.f, 0.f,
                                0.f, -1.f, CameraDistance);
    QMatrix4x4       rotation;
    rotation.rotate(_view.rotation);
    const auto   clipFromModel = projection * rotation;
    ViewUniforms view{};
    std::memcpy(&view.clipFromModel, clipFromModel.constData(), sizeof(view.clipFromModel));
    std::memcpy(&view.rotation, rotation.constData(), sizeof(view.rotation));
    const auto &d = _view.lightDirection, &c = _view.lightColor;
    view.directionAndIntensity = {d.x(), d.y(), d.z(), _view.intensity};
    view.colorAndAmbient       = {c.x(), c.y(), c.z(), _view.ambient};
    return view;
}

struct Renderer {
    QMutex                     lock;
    std::weak_ptr<const Asset> cachedAsset;
    id<MTLDevice>              device;
    id<MTLCommandQueue>        queue;
    id<MTLRenderPipelineState> pipeline;
    id<MTLDepthStencilState>   depth;
    id<MTLTexture>             white;

    Renderer()
    {
        device = MTLCreateSystemDefaultDevice();
        if (!device)
            return;
        queue    = [device newCommandQueue];
        auto lib = [device newLibraryWithSource:[NSString stringWithUTF8String:Shader]
                                        options:nil
                                          error:nullptr];
        if (!lib)
            return;
        auto *desc                           = [MTLRenderPipelineDescriptor new];
        desc.vertexFunction                  = [lib newFunctionWithName:@"modelVertex"];
        desc.fragmentFunction                = [lib newFunctionWithName:@"modelFragment"];
        desc.colorAttachments[0].pixelFormat = ColorFormat;
        desc.depthAttachmentPixelFormat      = DepthFormat;
        auto *               layout          = [MTLVertexDescriptor vertexDescriptor];
        constexpr std::array offsets{offsetof(Vertex, position), offsetof(Vertex, normal),
                                     offsetof(Vertex, uv)};
        constexpr std::array formats{MTLVertexFormatFloat3, MTLVertexFormatFloat3,
                                     MTLVertexFormatFloat2};
        for (NSUInteger i = 0; i < offsets.size(); ++i) {
            layout.attributes[i].format      = formats[i];
            layout.attributes[i].offset      = offsets[i];
            layout.attributes[i].bufferIndex = 0;
        }
        layout.layouts[0].stride = sizeof(Vertex);
        desc.vertexDescriptor    = layout;
        pipeline                 = [device newRenderPipelineStateWithDescriptor:desc error:nullptr];
        auto *depthDesc          = [MTLDepthStencilDescriptor new];
        depthDesc.depthCompareFunction = MTLCompareFunctionLess;
        depthDesc.depthWriteEnabled    = YES;
        depth                          = [device newDepthStencilStateWithDescriptor:depthDesc];
        QImage pixel(1, 1, QImage::Format_ARGB32);
        pixel.fill(Qt::white);
        white = uploadTexture(device, pixel);
    }

    bool prepare(const std::shared_ptr<const Asset> &_asset);
};
} // namespace

struct MetalMesh {
    std::vector<MeshPrimitive> primitives;
};

bool Renderer::prepare(const std::shared_ptr<const Asset> &_asset)
{
    if (_asset->metalMesh)
        return true;
    // Retain buffers for the active asset and release them when its last document reference goes.
    if (const auto previous = cachedAsset.lock())
        previous->metalMesh.reset();
    auto                                       mesh = std::make_shared<MetalMesh>();
    std::unordered_map<qint64, id<MTLTexture>> textures;
    mesh->primitives.reserve(_asset->primitives.size());
    for (const auto &p : _asset->primitives) {
        if (p.triangles.isEmpty())
            continue;
        auto vertices = [device newBufferWithBytes:p.vertices.constData()
                                            length:NSUInteger(p.vertices.size()) * sizeof(Vertex)
                                           options:MTLResourceStorageModeShared];
        auto indices =
            [device newBufferWithBytes:p.triangles.constData()
                                length:NSUInteger(p.triangles.size()) * sizeof(p.triangles.first())
                               options:MTLResourceStorageModeShared];
        id<MTLBuffer> colors = nil;
        if (!p.vertexColors.isEmpty()) {
            colors =
                [device newBufferWithBytes:p.vertexColors.constData()
                                    length:NSUInteger(p.vertexColors.size()) * sizeof(QVector4D)
                                   options:MTLResourceStorageModeShared];
            if (!colors)
                return false;
        }
        auto texture = white;
        if (!p.material.texture.isNull()) {
            auto [entry, inserted] = textures.try_emplace(p.material.texture.cacheKey());
            if (inserted)
                entry->second = uploadTexture(device, p.material.texture);
            texture = entry->second;
        }
        auto *samplerDesc        = [MTLSamplerDescriptor new];
        samplerDesc.sAddressMode = addressMode(p.material.u);
        samplerDesc.tAddressMode = addressMode(p.material.v);
        auto sampler             = [device newSamplerStateWithDescriptor:samplerDesc];
        if (!vertices || !indices || !texture || !sampler)
            return false;
        const auto &           c      = p.material.color;
        const float            cutoff = p.material.alphaMask ? p.material.alphaCutoff : 0.f;
        const MaterialUniforms material{
            {c.x(), c.y(), c.z(), c.w()}, cutoff, p.material.unlit, colors != nil, 0};
        mesh->primitives.push_back({vertices, indices, colors, texture, sampler,
                                    NSUInteger(p.triangles.size()) * 3, material,
                                    p.material.doubleSided});
    }
    _asset->metalMesh = std::move(mesh);
    cachedAsset       = _asset;
    return true;
}

PixelPlane renderMetalPreview(const std::shared_ptr<const Asset> &_asset, const RenderView &_view,
                              const QSize &_canvas)
{
    @autoreleasepool {
        static Renderer    renderer;
        const QMutexLocker guard(&renderer.lock);
        if (!renderer.queue || !renderer.pipeline || !renderer.depth || !renderer.white ||
            !renderer.prepare(_asset))
            return {};
        const auto color  = renderTarget(renderer.device, ColorFormat, _canvas);
        const auto depth  = renderTarget(renderer.device, DepthFormat, _canvas);
        const auto row    = (NSUInteger(_canvas.width()) * 4 + 255) & ~NSUInteger(255);
        const auto output = [renderer.device newBufferWithLength:row * NSUInteger(_canvas.height())
                                                         options:MTLResourceStorageModeShared];
        if (!color || !depth || !output)
            return {};

        auto *pass                           = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture     = color;
        pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor  = MTLClearColorMake(0, 0, 0, 0);
        pass.depthAttachment.texture         = depth;
        pass.depthAttachment.loadAction      = MTLLoadActionClear;
        pass.depthAttachment.storeAction     = MTLStoreActionDontCare;
        pass.depthAttachment.clearDepth      = 1.0;
        const auto command                   = [renderer.queue commandBuffer];
        const auto encoder                   = [command renderCommandEncoderWithDescriptor:pass];
        if (!encoder)
            return {};
        const auto view     = uniforms(_view, _canvas);
        const bool mirrored = _view.placement.determinant() < 0.;
        [encoder setRenderPipelineState:renderer.pipeline];
        [encoder setDepthStencilState:renderer.depth];
        [encoder setFrontFacingWinding:mirrored ? MTLWindingClockwise : MTLWindingCounterClockwise];
        [encoder setVertexBytes:&view length:sizeof(view) atIndex:1];
        [encoder setFragmentBytes:&view length:sizeof(view) atIndex:1];
        constexpr simd_float4 white = {1.f, 1.f, 1.f, 1.f};
        for (const auto &p : _asset->metalMesh->primitives) {
            auto material     = p.material;
            material.mirrored = mirrored;
            [encoder setCullMode:p.doubleSided ? MTLCullModeNone : MTLCullModeBack];
            [encoder setVertexBuffer:p.vertices offset:0 atIndex:0];
            [encoder setVertexBytes:&material length:sizeof(material) atIndex:2];
            if (p.colors)
                [encoder setVertexBuffer:p.colors offset:0 atIndex:3];
            else
                [encoder setVertexBytes:&white length:sizeof(white) atIndex:3];
            [encoder setFragmentBytes:&material length:sizeof(material) atIndex:2];
            [encoder setFragmentTexture:p.texture atIndex:0];
            [encoder setFragmentSamplerState:p.sampler atIndex:0];
            [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:p.indexCount
                                 indexType:MTLIndexTypeUInt32
                               indexBuffer:p.indices
                         indexBufferOffset:0];
        }
        [encoder endEncoding];
        const auto blit = [command blitCommandEncoder];
        if (!blit)
            return {};
        [blit copyFromTexture:color
                         sourceSlice:0
                         sourceLevel:0
                        sourceOrigin:MTLOriginMake(0, 0, 0)
                          sourceSize:MTLSizeMake(NSUInteger(_canvas.width()),
                                                 NSUInteger(_canvas.height()), 1)
                            toBuffer:output
                   destinationOffset:0
              destinationBytesPerRow:row
            destinationBytesPerImage:row * NSUInteger(_canvas.height())];
        [blit endEncoding];
        [command commit];
        [command waitUntilCompleted];
        if (command.status != MTLCommandBufferStatusCompleted)
            return {};
        QImage image(static_cast<const uchar *>(output.contents), _canvas.width(), _canvas.height(),
                     qsizetype(row), QImage::Format_ARGB32_Premultiplied);
        image = image.copy();
        image.setColorSpace(QColorSpace::SRgb);
        return image;
    }
}
} // namespace PhotoCrap::Model3D
