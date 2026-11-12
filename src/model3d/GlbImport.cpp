#include "Model3DPrivate.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <QBuffer>
#include <QColorSpace>
#include <QCoreApplication>
#include <QHash>
#include <QImageReader>
#include <QMatrix4x4>
#include <QSet>
#include <QTemporaryFile>
#include <QtEndian>
#include <meshoptimizer.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>

namespace PhotoCrap::Model3D
{
namespace
{
constexpr qsizetype MaxVertices      = 8'000'000;
constexpr qsizetype MaxTexturePixels = 16 * 1024 * 1024;
constexpr size_t    MaxParserBytes   = 32 * 1024 * 1024;
constexpr qsizetype MiB              = 1024 * 1024;
// Leave room for the maximum supported textured mesh with no vertex colors.
constexpr qsizetype MaxDecodedBytes = 400 * MiB;
static_assert(MaxVertices * sizeof(Vertex) + MaxTriangles * sizeof(Face) + MaxTexturePixels * 4 <=
              MaxDecodedBytes);

struct MeshBudget {
    qsizetype vertices = 0, triangles = 0, geometryBytes = 0;
};

struct TextureCache {
    QHash<const cgltf_image *, QImage> images;
    QVariantList                       statistics;
    qsizetype                          pixelsPerImage = MaxTexturePixels;
    qsizetype                          decodedPixels  = 0;
};

bool finite(const QVector3D &_value)
{
    return std::isfinite(_value.x()) && std::isfinite(_value.y()) && std::isfinite(_value.z()) &&
           qAbs(_value.x()) <= 1e8 && qAbs(_value.y()) <= 1e8 && qAbs(_value.z()) <= 1e8;
}

// No external URLs or paths are resolved. Only a GLB's embedded BIN and PNG/JPEG
// images are read, and all mesh ranges are validated before the accessors run.
bool validAccessor(const cgltf_accessor *_accessor)
{
    if (!_accessor || !_accessor->buffer_view || _accessor->is_sparse || !_accessor->count ||
        _accessor->count > MaxVertices * 3)
        return false;
    const auto &view      = *_accessor->buffer_view;
    const auto  element   = cgltf_calc_size(_accessor->type, _accessor->component_type);
    const auto  alignment = cgltf_component_size(_accessor->component_type);
    if (!element || !alignment || !view.buffer || !view.buffer->data ||
        view.has_meshopt_compression || view.offset > view.buffer->size ||
        view.size > view.buffer->size - view.offset || _accessor->offset > view.size ||
        (_accessor->offset + view.offset) % alignment || _accessor->stride < element ||
        _accessor->stride % alignment)
        return false;
    const auto available = view.size - _accessor->offset;
    return element <= available &&
           _accessor->count - 1 <= (available - element) / _accessor->stride;
}

QImage textureImage(const cgltf_image &_image, qsizetype _pixelLimit, QSize &_originalSize)
{
    const auto *view = _image.buffer_view;
    if (_image.uri || !view || !view->buffer || !view->buffer->data ||
        view->offset > view->buffer->size || view->size > view->buffer->size - view->offset ||
        view->size == 0 || view->size > MaxFileBytes)
        return {};
    QByteArray bytes = QByteArray::fromRawData(
        reinterpret_cast<const char *>(cgltf_buffer_view_data(view)), view->size);
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const auto   format = reader.format();
    const auto   size   = reader.size();
    if ((format != "png" && format != "jpeg") || size.isEmpty() || size.width() > 4096 ||
        size.height() > 4096)
        return {};
    _originalSize = size;
    const qsizetype pixels      = qsizetype(size.width()) * size.height();
    auto            decodedSize = size;
    if (pixels > _pixelLimit) {
        const double ratio = std::sqrt(double(_pixelLimit) / pixels);
        decodedSize =
            QSize(qMax(1, int(size.width() * ratio)), qMax(1, int(size.height() * ratio)));
        reader.setScaledSize(decodedSize);
    }
    auto image = reader.read();
    if (image.isNull() || image.size() != decodedSize)
        return {};
    if (image.colorSpace().isValid())
        image.convertToColorSpace(QColorSpace::SRgb);
    return image.convertToFormat(QImage::Format_ARGB32);
}

Material::Wrap wrap(cgltf_int _wrap)
{
    if (_wrap == cgltf_wrap_mode_clamp_to_edge)
        return Material::Wrap::clamp;
    if (_wrap == cgltf_wrap_mode_mirrored_repeat)
        return Material::Wrap::mirror;
    return Material::Wrap::repeat;
}

bool readMaterial(const cgltf_material *_source, Material &_material, TextureCache &_textures)
{
    if (!_source)
        return true;
    _material.doubleSided = _source->double_sided;
    _material.unlit       = _source->unlit;
    _material.alphaMask   = _source->alpha_mode != cgltf_alpha_mode_opaque;
    _material.alphaCutoff =
        _source->alpha_mode == cgltf_alpha_mode_blend ? .5f : _source->alpha_cutoff;
    if (!std::isfinite(_material.alphaCutoff))
        return false;
    if (!_source->has_pbr_metallic_roughness)
        return !_source->has_pbr_specular_glossiness;
    const auto &base  = _source->pbr_metallic_roughness;
    const auto *color = base.base_color_factor;
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(color[i]) || color[i] < 0.f || color[i] > 1.f)
            return false;
        _material.color[i] = color[i];
    }
    const auto &texture = base.base_color_texture;
    if (!texture.texture)
        return true;
    const auto *image = texture.texture->image;
    if (!image)
        return false;
    if (!_textures.images.contains(image)) {
        QSize      originalSize;
        const auto decoded = textureImage(*image, _textures.pixelsPerImage, originalSize);
        if (decoded.isNull())
            return false;
        _textures.decodedPixels += qsizetype(decoded.width()) * decoded.height();
        _textures.images.insert(image, decoded);
        _textures.statistics.append(QVariantMap{{"width", originalSize.width()},
                                                {"height", originalSize.height()},
                                                {"loadedWidth", decoded.width()},
                                                {"loadedHeight", decoded.height()},
                                                {"bytes", qsizetype(image->buffer_view->size)}});
    }
    _material.texture = _textures.images.value(image);
    if (const auto *sampler = texture.texture->sampler) {
        _material.u = wrap(sampler->wrap_s);
        _material.v = wrap(sampler->wrap_t);
    }
    return true;
}

const cgltf_accessor *attribute(const cgltf_primitive &_primitive, cgltf_attribute_type _type,
                                int _index = 0)
{
    return cgltf_find_accessor(&_primitive, _type, _index);
}

QVector3D normal(const QMatrix3x3 &_transform, const QVector3D &_normal)
{
    const auto row = [&](int i) {
        return _transform(i, 0) * _normal.x() + _transform(i, 1) * _normal.y() +
               _transform(i, 2) * _normal.z();
    };
    return QVector3D(row(0), row(1), row(2)).normalized();
}

bool jointTransforms(const cgltf_skin &_skin, const QHash<const cgltf_node *, QMatrix4x4> &_nodes,
                     QVector<QMatrix4x4> &_joints)
{
    const auto *bind = _skin.inverse_bind_matrices;
    if (!_skin.joints_count || _skin.joints_count > size_t(_nodes.size()) ||
        (bind &&
         (!validAccessor(bind) || bind->type != cgltf_type_mat4 ||
          bind->component_type != cgltf_component_type_r_32f || bind->count < _skin.joints_count)))
        return false;
    _joints.reserve(_skin.joints_count);
    for (size_t i = 0; i < _skin.joints_count; ++i) {
        const auto world = _nodes.constFind(_skin.joints[i]);
        if (world == _nodes.cend())
            return false;
        QMatrix4x4 inverseBind;
        if (bind) {
            float matrix[16];
            if (!cgltf_accessor_read_float(bind, i, matrix, 16) ||
                !std::all_of(std::begin(matrix), std::end(matrix),
                             [](float v) { return std::isfinite(v); }) ||
                matrix[3] != 0.f || matrix[7] != 0.f || matrix[11] != 0.f || matrix[15] != 1.f)
                return false;
            inverseBind = QMatrix4x4(matrix).transposed();
        }
        _joints.append(*world * inverseBind);
    }
    return true;
}

struct SkinAttributes {
    const cgltf_accessor *joints;
    const cgltf_accessor *weights;
};

bool skinTransform(const QVector<SkinAttributes> &_attributes, const QVector<QMatrix4x4> &_joints,
                   qsizetype _vertex, QMatrix4x4 &_result)
{
    _result.fill(0);
    float total = 0.f;
    for (const auto &set : _attributes) {
        cgltf_uint indices[4];
        float      weights[4];
        if (!cgltf_accessor_read_uint(set.joints, _vertex, indices, 4) ||
            !cgltf_accessor_read_float(set.weights, _vertex, weights, 4))
            return false;
        for (int i = 0; i < 4; ++i) {
            if (indices[i] >= size_t(_joints.size()) || !std::isfinite(weights[i]) ||
                weights[i] < 0.f || weights[i] > 1.f)
                return false;
            if (weights[i] > 0.f) {
                _result += _joints[indices[i]] * weights[i];
                total += weights[i];
            }
        }
    }
    if (total <= 1e-6f)
        return false;
    _result *= 1.f / total;
    // A weighted sum of affine joint matrices stays affine.
    _result(3, 3) = 1.f;
    return true;
}

bool readPrimitive(const cgltf_primitive &_source, const QMatrix4x4 &_world, Primitive &_target,
                   const QVector<QMatrix4x4> &_joints, TextureCache &_textures, MeshBudget &_budget,
                   const Continue &_continue, QString &_error)
{
    if (_source.type != cgltf_primitive_type_triangles || _source.has_draco_mesh_compression) {
        _error = QCoreApplication::translate("Model3D",
                                             "Le GLB doit contenir des triangles non compressés.");
        return false;
    }
    if (!readMaterial(_source.material, _target.material, _textures)) {
        _error = QCoreApplication::translate(
            "Model3D", "Un matériau ou une texture du GLB n’est pas pris en charge. Les textures "
                       "de couleur doivent être intégrées en PNG ou JPEG, avec un maximum de 4 096 "
                       "px par côté.");
        return false;
    }
    const auto *positions = attribute(_source, cgltf_attribute_type_position);
    const auto *normals   = attribute(_source, cgltf_attribute_type_normal);
    const auto *colors    = attribute(_source, cgltf_attribute_type_color);
    if (!validAccessor(positions) || positions->type != cgltf_type_vec3 ||
        (normals && (!validAccessor(normals) || normals->type != cgltf_type_vec3 ||
                     normals->count != positions->count)) ||
        (colors && (!validAccessor(colors) ||
                    (colors->type != cgltf_type_vec3 && colors->type != cgltf_type_vec4) ||
                    colors->count != positions->count)))
        return false;
    QVector<SkinAttributes> skin;
    if (!_joints.isEmpty()) {
        for (int index = 0; index < 4; ++index) {
            const auto *joints  = attribute(_source, cgltf_attribute_type_joints, index);
            const auto *weights = attribute(_source, cgltf_attribute_type_weights, index);
            if (index && !joints && !weights)
                break;
            if (!validAccessor(joints) || joints->type != cgltf_type_vec4 ||
                (joints->component_type != cgltf_component_type_r_8u &&
                 joints->component_type != cgltf_component_type_r_16u) ||
                joints->count != positions->count || !validAccessor(weights) ||
                weights->type != cgltf_type_vec4 || weights->count != positions->count ||
                (weights->component_type != cgltf_component_type_r_32f &&
                 !(weights->normalized && (weights->component_type == cgltf_component_type_r_8u ||
                                           weights->component_type == cgltf_component_type_r_16u))))
                return false;
            skin.append(SkinAttributes{joints, weights});
        }
    }
    const cgltf_accessor *    coordinates = nullptr;
    const cgltf_texture_view *texture     = nullptr;
    if (!_target.material.texture.isNull()) {
        texture      = &_source.material->pbr_metallic_roughness.base_color_texture;
        const int uv = texture->has_transform && texture->transform.has_texcoord
                           ? texture->transform.texcoord
                           : texture->texcoord;
        coordinates  = attribute(_source, cgltf_attribute_type_texcoord, uv);
        if (!validAccessor(coordinates) || coordinates->type != cgltf_type_vec2 ||
            coordinates->count != positions->count)
            return false;
    }
    const auto *indices = _source.indices;
    if (indices && (!validAccessor(indices) || indices->type != cgltf_type_scalar ||
                    (indices->component_type != cgltf_component_type_r_8u &&
                     indices->component_type != cgltf_component_type_r_16u &&
                     indices->component_type != cgltf_component_type_r_32u)))
        return false;
    const qsizetype count = indices ? indices->count : positions->count;
    if (count % 3)
        return false;
    if (qsizetype(positions->count) > MaxVertices - _budget.vertices ||
        count / 3 > MaxTriangles - _budget.triangles) {
        _error = QCoreApplication::translate(
                     "Model3D", "Le GLB dépasse la limite de %L1 sommets ou %L2 triangles.")
                     .arg(MaxVertices)
                     .arg(MaxTriangles);
        return false;
    }
    const qsizetype vertexBytes = sizeof(Vertex) + (colors ? sizeof(QVector4D) : 0);
    const auto geometryBytes = _budget.geometryBytes + qsizetype(positions->count) * vertexBytes +
                               (count / 3) * qsizetype(sizeof(Face));
    const auto decodedBytes = geometryBytes + _textures.decodedPixels * 4;
    if (decodedBytes > MaxDecodedBytes) {
        _error =
            QCoreApplication::translate(
                "Model3D", "Le modèle 3D nécessite %L1 Mio de mémoire décodée (limite %L2 Mio).")
                .arg((decodedBytes + MiB - 1) / MiB)
                .arg(MaxDecodedBytes / MiB);
        return false;
    }
    _budget.vertices += positions->count;
    _budget.triangles += count / 3;
    _budget.geometryBytes = geometryBytes;
    _target.vertices.resize(positions->count);
    if (colors)
        _target.vertexColors.resize(positions->count);
    const auto normalMatrix = _world.normalMatrix();
    for (qsizetype i = 0; i < _target.vertices.size(); ++i) {
        if (i % 2048 == 0 && _continue && !_continue())
            return false;
        float p[3], n[3], uv[2];
        if (!cgltf_accessor_read_float(positions, i, p, 3))
            return false;
        QMatrix4x4 skinned;
        if (!skin.isEmpty() && !skinTransform(skin, _joints, i, skinned)) {
            _error = QCoreApplication::translate(
                "Model3D", "Le squelette du GLB contient des influences de sommet invalides.");
            return false;
        }
        auto &vertex    = _target.vertices[i];
        vertex.position = (skin.isEmpty() ? _world : skinned).map(QVector3D(p[0], p[1], p[2]));
        if (!finite(vertex.position))
            return false;
        if (normals) {
            if (!cgltf_accessor_read_float(normals, i, n, 3) || !finite({n[0], n[1], n[2]}))
                return false;
            vertex.normal =
                normal(skin.isEmpty() ? normalMatrix : skinned.normalMatrix(), {n[0], n[1], n[2]});
        }
        if (colors) {
            float      rgba[4]{1, 1, 1, 1};
            const auto components = cgltf_num_components(colors->type);
            if (!cgltf_accessor_read_float(colors, i, rgba, components))
                return false;
            for (const float channel : rgba)
                if (!std::isfinite(channel) || channel < 0 || channel > 1)
                    return false;
            _target.vertexColors[i] = QVector4D(rgba[0], rgba[1], rgba[2], rgba[3]);
        }
        if (coordinates) {
            if (!cgltf_accessor_read_float(coordinates, i, uv, 2))
                return false;
            if (texture->has_transform) {
                const auto &t = texture->transform;
                const float x = uv[0] * t.scale[0], y = uv[1] * t.scale[1];
                uv[0] = t.offset[0] + std::cos(t.rotation) * x - std::sin(t.rotation) * y;
                uv[1] = t.offset[1] + std::sin(t.rotation) * x + std::cos(t.rotation) * y;
            }
            if (!std::isfinite(uv[0]) || !std::isfinite(uv[1]) || qAbs(uv[0]) > 1e5f ||
                qAbs(uv[1]) > 1e5f)
                return false;
            vertex.uv = {uv[0], uv[1]};
        }
    }
    const bool mirrored =
        skin.isEmpty() ? _world.determinant() < 0
                       : std::all_of(_joints.cbegin(), _joints.cend(), [](const QMatrix4x4 &joint) {
                             return joint.determinant() < 0;
                         });
    _target.triangles.resize(count / 3);
    for (qsizetype i = 0; i < count; i += 3) {
        auto &triangle = _target.triangles[i / 3];
        for (int corner = 0; corner < 3; ++corner) {
            const auto index =
                indices ? cgltf_accessor_read_index(indices, i + corner) : i + corner;
            if (index >= positions->count)
                return false;
            triangle[corner] = quint32(index);
        }
        if (mirrored)
            std::swap(triangle[1], triangle[2]);
    }
    return true;
}

void simplify(Primitive &_mesh, qsizetype _target)
{
    if (_mesh.triangles.size() <= _target)
        return;
    static_assert(sizeof(Face) == 3 * sizeof(unsigned int));
    static_assert(offsetof(Vertex, normal) == 3 * sizeof(float));
    static_assert(offsetof(Vertex, uv) == 6 * sizeof(float));
    const auto *          vertices   = reinterpret_cast<const float *>(_mesh.vertices.constData());
    const auto            indexCount = _mesh.triangles.size() * 3;
    const float           uvWeight   = _mesh.material.texture.isNull() ? 0.f : 1.f;
    const float           weights[]{.4f, .4f, .4f, uvWeight, uvWeight};
    QVector<unsigned int> indices(indexCount);
    // Preserve seams and folds. The target may be missed when further collapses
    // would introduce visible error in geometry, normals or texture coordinates.
    const auto reducedCount = meshopt_simplifyWithAttributes(
        indices.data(), _mesh.triangles.constFirst().data(), indexCount, vertices,
        _mesh.vertices.size(), sizeof(Vertex), vertices + 3, sizeof(Vertex), weights,
        std::size(weights), nullptr, _target * 3, .01f,
        meshopt_SimplifyPreserveFolds | meshopt_SimplifyErrorClamped);
    if (reducedCount == 0 || qsizetype(reducedCount) >= indexCount)
        return;

    QVector<unsigned int> remap(_mesh.vertices.size());
    const auto            used = meshopt_optimizeVertexFetchRemap(remap.data(), indices.constData(),
                                                       reducedCount, _mesh.vertices.size());
    QVector<Vertex>       compactVertices(static_cast<qsizetype>(used));
    QVector<QVector4D>    compactColors;
    if (!_mesh.vertexColors.isEmpty())
        compactColors.resize(qsizetype(used));
    for (qsizetype i = 0; i < remap.size(); ++i) {
        const auto index = remap[i];
        if (index == std::numeric_limits<unsigned int>::max())
            continue;
        compactVertices[index] = _mesh.vertices.at(i);
        if (!compactColors.isEmpty())
            compactColors[index] = _mesh.vertexColors.at(i);
    }
    QVector<Face> faces(qsizetype(reducedCount / 3));
    for (qsizetype i = 0; i < faces.size(); ++i) {
        const auto at = i * 3;
        faces[i]      = {remap[indices[at]], remap[indices[at + 1]], remap[indices[at + 2]]};
    }
    _mesh.vertices     = std::move(compactVertices);
    _mesh.vertexColors = std::move(compactColors);
    _mesh.triangles    = std::move(faces);
}
} // namespace

static std::shared_ptr<Asset> loadAsset(QByteArray _bytes, const Continue &_continue,
                                        QString *_error, int _triangleTarget)
{
    QString  detail;
    QString &error = _error ? *_error : detail;
    error.clear();
    if (!validTriangleTarget(_triangleTarget) || _bytes.size() < 20 ||
        _bytes.size() > MaxFileBytes || !_bytes.startsWith("glTF"))
        return {};
    if (qFromLittleEndian<quint32>(_bytes.constData() + 12) % 4 != 0)
        return {};
    size_t        remaining = MaxParserBytes;
    cgltf_options options{};
    options.memory.user_data  = &remaining;
    options.memory.alloc_func = [](void *_context, size_t _size) -> void * {
        auto &budget = *static_cast<size_t *>(_context);
        if (_size > budget)
            return nullptr;
        budget -= _size;
        return std::malloc(_size);
    };
    options.memory.free_func = [](void *, void *_data) { std::free(_data); };
    cgltf_data *parsed       = nullptr;
    if (cgltf_parse(&options, _bytes.constData(), _bytes.size(), &parsed) != cgltf_result_success)
        return {};
    const std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(parsed, cgltf_free);
    if (data->file_type != cgltf_file_type_glb || !data->bin || data->buffers_count != 1 ||
        data->buffers[0].uri || data->buffers[0].size > data->bin_size ||
        data->nodes_count > 4096 ||
        cgltf_load_buffers(&options, data.get(), nullptr) != cgltf_result_success)
        return {};
    for (size_t i = 0; i < data->extensions_required_count; ++i) {
        const QByteArrayView extension(data->extensions_required[i]);
        if (extension != "KHR_texture_transform" && extension != "KHR_materials_unlit" &&
            extension != "KHR_mesh_quantization") {
            error = QCoreApplication::translate(
                        "Model3D", "Le GLB exige l’extension %1, qui n’est pas prise en charge.")
                        .arg(QString::fromUtf8(extension));
            return {};
        }
    }

    // Traverse the default scene iteratively with depth and cycle checks.
    struct Node {
        const cgltf_node *source;
        QMatrix4x4        parent;
        int               depth;
    };
    QVector<Node> stack;
    const auto *  scene = data->scene ? data->scene : (data->scenes_count ? data->scenes : nullptr);
    if (scene) {
        for (size_t i = 0; i < scene->nodes_count; ++i)
            stack.append(Node{scene->nodes[i], {}, 0});
    } else {
        for (size_t i = 0; i < data->nodes_count; ++i)
            if (!data->nodes[i].parent)
                stack.append(Node{&data->nodes[i], {}, 0});
    }
    QHash<const cgltf_node *, QMatrix4x4> worldTransforms;
    QVector<const cgltf_node *>           meshNodes;
    while (!stack.isEmpty()) {
        const auto node = stack.takeLast();
        if (!node.source || node.depth > 64 || worldTransforms.contains(node.source) ||
            node.source->has_mesh_gpu_instancing || (_continue && !_continue()))
            return {};
        float matrix[16];
        cgltf_node_transform_local(node.source, matrix);
        const auto world = node.parent * QMatrix4x4(matrix).transposed();
        worldTransforms.insert(node.source, world);
        if (node.source->mesh)
            meshNodes.append(node.source);
        for (size_t i = 0; i < node.source->children_count; ++i)
            stack.append(Node{node.source->children[i], world, node.depth + 1});
    }
    // Divide the decoded texture budget among the unique base-color images.
    // Multiple 4K materials then keep their colors within the same memory limit.
    QSet<const cgltf_image *> baseImages;
    size_t                    primitiveCount = 0;
    for (const auto *node : meshNodes) {
        if (node->mesh->primitives_count > 1024 - primitiveCount)
            return {};
        primitiveCount += node->mesh->primitives_count;
        for (size_t i = 0; i < node->mesh->primitives_count; ++i) {
            const auto *material = node->mesh->primitives[i].material;
            if (material && material->has_pbr_metallic_roughness) {
                const auto *texture = material->pbr_metallic_roughness.base_color_texture.texture;
                if (texture && texture->image)
                    baseImages.insert(texture->image);
            }
        }
    }
    auto         asset = std::make_shared<Asset>();
    TextureCache textures;
    textures.pixelsPerImage = MaxTexturePixels / qMax(qsizetype(1), baseImages.size());
    MeshBudget budget;
    QVector3D  minimum(1e9f, 1e9f, 1e9f), maximum(-1e9f, -1e9f, -1e9f);
    for (const auto *node : meshNodes) {
        QVector<QMatrix4x4> joints;
        // Capture the default pose. The joint hierarchy and inverse bind matrices
        // determine world-space geometry, independent of the mesh node transform.
        if (node->skin && !jointTransforms(*node->skin, worldTransforms, joints)) {
            error = QCoreApplication::translate(
                "Model3D", "Le squelette ou ses matrices de liaison sont invalides.");
            return {};
        }
        const auto  world = worldTransforms.value(node);
        const auto *mesh  = node->mesh;
        for (size_t i = 0; i < mesh->primitives_count; ++i) {
            Primitive primitive;
            if (!readPrimitive(mesh->primitives[i], world, primitive, joints, textures, budget,
                               _continue, error))
                return {};
            for (const auto &vertex : primitive.vertices)
                for (int axis = 0; axis < 3; ++axis) {
                    minimum[axis] = qMin(minimum[axis], vertex.position[axis]);
                    maximum[axis] = qMax(maximum[axis], vertex.position[axis]);
                }
            asset->primitives.append(std::move(primitive));
        }
    }
    const auto  center   = (minimum + maximum) * .5f;
    const float diameter = (maximum - minimum).length();
    if (asset->primitives.isEmpty() || diameter < 1e-8f || !std::isfinite(diameter))
        return {};
    for (auto &primitive : asset->primitives)
        for (auto &vertex : primitive.vertices)
            vertex.position = (vertex.position - center) / diameter;
    const auto sourceTriangles = budget.triangles;
    if (_triangleTarget > 0 && sourceTriangles > _triangleTarget) {
        budget = {};
        for (auto &primitive : asset->primitives) {
            const auto target = qMax(qint64(1), qint64(primitive.triangles.size()) *
                                                    _triangleTarget / sourceTriangles);
            simplify(primitive, target);
            if (_continue && !_continue())
                return {};
            budget.vertices += primitive.vertices.size();
            budget.triangles += primitive.triangles.size();
            budget.geometryBytes += primitive.vertices.size() * qsizetype(sizeof(Vertex)) +
                                    primitive.triangles.size() * qsizetype(sizeof(Face)) +
                                    primitive.vertexColors.size() * qsizetype(sizeof(QVector4D));
        }
    }
    asset->triangleTarget = _triangleTarget;
    asset->statistics     = {{"triangles", budget.triangles},
                         {"sourceTriangles", sourceTriangles},
                         {"triangleTarget", _triangleTarget},
                         {"vertices", budget.vertices},
                         {"fileBytes", _bytes.size()},
                         {"decodedBytes", budget.geometryBytes + textures.decodedPixels * 4},
                         {"textures", std::move(textures.statistics)}};
    asset->bytesInMemory  = _bytes.size() + budget.geometryBytes + textures.decodedPixels * 4;
    asset->bytes          = std::move(_bytes);
    return asset;
}

std::shared_ptr<const Asset> load(QByteArray _bytes, const Continue &_continue, QString *_error,
                                  int _triangleTarget)
{
    return loadAsset(std::move(_bytes), _continue, _error, _triangleTarget);
}

std::shared_ptr<const Asset> loadTemporary(const std::shared_ptr<QTemporaryFile> &_file,
                                           const Continue &_continue, QString *_error,
                                           int _triangleTarget)
{
    const auto size = _file->size();
    if (size < 20 || size > MaxFileBytes)
        return {};
    auto *mapping = _file->map(0, size, QFileDevice::MapPrivateOption);
    if (!mapping) {
        if (_error)
            *_error = _file->errorString();
        return {};
    }
    auto asset = loadAsset(QByteArray::fromRawData(reinterpret_cast<const char *>(mapping), size),
                           _continue, _error, _triangleTarget);
    if (asset)
        asset->backing = _file;
    else
        _file->unmap(mapping);
    return asset;
}

const QByteArray &encoded(const Asset &_asset)
{
    return _asset.bytes;
}
qsizetype storageBytes(const Asset &_asset)
{
    return _asset.bytesInMemory;
}
const QVariantMap &statistics(const Asset &_asset)
{
    return _asset.statistics;
}
int triangleTarget(const Asset &_asset)
{
    return _asset.triangleTarget;
}
} // namespace PhotoCrap::Model3D
