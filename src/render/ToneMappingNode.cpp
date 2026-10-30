#include "ToneMappingNode.h"
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <cstring>
#include <utility>

namespace
{
using PhotoCrap::Filters::ToneMappingSettings;
class ToneShader : public QSGMaterialShader
{
public:
    ToneShader()
    {
        setShaderFileName(VertexStage,
                          QStringLiteral(":/PhotoCrap/src/render/shaders/tone.vert.qsb"));
        setShaderFileName(FragmentStage,
                          QStringLiteral(":/PhotoCrap/src/render/shaders/tone.frag.qsb"));
    }
    bool updateUniformData(RenderState &_state, QSGMaterial *_new, QSGMaterial *_old) override;
    void updateSampledImage(RenderState &_state, int _binding, QSGTexture **_texture,
                            QSGMaterial *_new, QSGMaterial *) override;
};

class ToneMaterial : public QSGMaterial
{
public:
    QSGTexture           *source = nullptr, *table = nullptr;
    QSGTexture::Filtering filtering = QSGTexture::Linear;
    ToneMappingSettings   settings;
    ToneMaterial()
    {
        setFlag(Blending);
    }
    QSGMaterialType *type() const override
    {
        static QSGMaterialType type;
        return &type;
    }
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override
    {
        return new ToneShader;
    }
    int compare(const QSGMaterial *_other) const override
    {
        const auto *other = static_cast<const ToneMaterial *>(_other);
        if (source->comparisonKey() != other->source->comparisonKey())
            return source->comparisonKey() < other->source->comparisonKey() ? -1 : 1;
        if (table->comparisonKey() != other->table->comparisonKey())
            return table->comparisonKey() < other->table->comparisonKey() ? -1 : 1;
        if (filtering != other->filtering)
            return int(filtering) - int(other->filtering);
        if (settings.mode != other->settings.mode)
            return int(settings.mode) - int(other->settings.mode);
        for (const auto vectors : {std::pair{settings.color, other->settings.color},
                                   std::pair{settings.families0, other->settings.families0},
                                   std::pair{settings.families1, other->settings.families1},
                                   std::pair{settings.matrix[0], other->settings.matrix[0]},
                                   std::pair{settings.matrix[1], other->settings.matrix[1]},
                                   std::pair{settings.matrix[2], other->settings.matrix[2]}})
            for (int i = 0; i < 4; ++i)
                if (vectors.first[i] != vectors.second[i])
                    return vectors.first[i] < vectors.second[i] ? -1 : 1;
        return 0;
    }
};

bool ToneShader::updateUniformData(RenderState &_state, QSGMaterial *_new, QSGMaterial *_old)
{
    bool  changed = false;
    auto *data    = _state.uniformData();
    if (!_old || _state.isMatrixDirty()) {
        const auto matrix = _state.combinedMatrix();
        std::memcpy(data->data(), matrix.constData(), 64);
        changed = true;
    }
    if (!_old || _state.isOpacityDirty()) {
        const float opacity = _state.opacity();
        std::memcpy(data->data() + 64, &opacity, sizeof(opacity));
        changed = true;
    }
    const auto *material = static_cast<const ToneMaterial *>(_new);
    // Qt may pass the same material object after an in-place mode change.
    const qint32 mode = qint32(material->settings.mode);
    if (!_old || std::memcmp(data->constData() + 68, &mode, sizeof(mode))) {
        std::memcpy(data->data() + 68, &mode, sizeof(mode));
        changed = true;
    }
    const auto &color      = material->settings.color;
    const auto &first      = material->settings.families0;
    const auto &second     = material->settings.families1;
    const float values[12] = {color.x(), color.y(), color.z(),  color.w(),  first.x(),  first.y(),
                              first.z(), first.w(), second.x(), second.y(), second.z(), second.w()};
    if (!_old || std::memcmp(data->constData() + 80, values, sizeof(values))) {
        std::memcpy(data->data() + 80, values, sizeof(values));
        changed = true;
    }
    for (int row = 0; row < 3; ++row) {
        const auto &vector          = material->settings.matrix[row];
        const float coefficients[4] = {vector.x(), vector.y(), vector.z(), vector.w()};
        const int   offset          = 128 + row * 16;
        if (!_old || std::memcmp(data->constData() + offset, coefficients, sizeof(coefficients))) {
            std::memcpy(data->data() + offset, coefficients, sizeof(coefficients));
            changed = true;
        }
    }
    return changed;
}

void ToneShader::updateSampledImage(RenderState &_state, int _binding, QSGTexture **_texture,
                                    QSGMaterial *_new, QSGMaterial *)
{
    auto *material = static_cast<ToneMaterial *>(_new);
    if (_binding == 1) {
        *_texture = material->source;
        (*_texture)->setFiltering(material->filtering);
    } else if (_binding == 2) {
        *_texture = material->table;
        (*_texture)->setFiltering(QSGTexture::Nearest);
    } else
        return;
    (*_texture)->commitTextureOperations(_state.rhi(), _state.resourceUpdateBatch());
}
} // namespace

ToneMappingNode::ToneMappingNode(QSGTexture *_source, QSGTexture *_table,
                                 const ToneMappingSettings &_settings)
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    m_geometry.setDrawingMode(QSGGeometry::DrawTriangleStrip);
    setGeometry(&m_geometry);
    auto *mapping     = new ToneMaterial;
    mapping->source   = _source;
    mapping->table    = _table;
    mapping->settings = _settings;
    setMaterial(mapping);
    setFlag(OwnsMaterial);
}
void ToneMappingNode::setTable(QSGTexture *_table, const ToneMappingSettings &_settings)
{
    auto *mapping = static_cast<ToneMaterial *>(material());
    if (mapping->table != _table || mapping->settings != _settings) {
        mapping->table    = _table;
        mapping->settings = _settings;
        markDirty(DirtyMaterial);
    }
}
void ToneMappingNode::setImageRect(const QRectF &_rect, const QRectF &_sourceRect)
{
    const auto *mapping = static_cast<const ToneMaterial *>(material());
    QSGGeometry::updateTexturedRectGeometry(
        &m_geometry, _rect, mapping->source->convertToNormalizedSourceRect(_sourceRect));
    markDirty(DirtyGeometry);
}
void ToneMappingNode::setFiltering(QSGTexture::Filtering _filtering)
{
    auto *mapping = static_cast<ToneMaterial *>(material());
    if (mapping->filtering != _filtering) {
        mapping->filtering = _filtering;
        markDirty(DirtyMaterial);
    }
}
