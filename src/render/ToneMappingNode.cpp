#include "ToneMappingNode.h"
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <cstring>

namespace
{
using PhotoCrap::Filters::ToneMappingMode;
using PhotoCrap::Filters::ToneMappingSettings;
std::array<QVector4D, 9> shaderParameters(const ToneMappingSettings &_settings)
{
    if (_settings.mode == ToneMappingMode::SelectiveColorRelative ||
        _settings.mode == ToneMappingMode::SelectiveColorAbsolute)
        return _settings.selective;
    return {_settings.color,
            _settings.families0,
            _settings.families1,
            _settings.matrix[0],
            _settings.matrix[1],
            _settings.matrix[2],
            {},
            {},
            {}};
}

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
        if (settings.effectOpacity != other->settings.effectOpacity)
            return settings.effectOpacity < other->settings.effectOpacity ? -1 : 1;
        if (settings.maskCoverage != other->settings.maskCoverage)
            return settings.maskCoverage < other->settings.maskCoverage ? -1 : 1;
        const auto left = shaderParameters(settings), right = shaderParameters(other->settings);
        for (size_t vector = 0; vector < left.size(); ++vector)
            for (int channel = 0; channel < 4; ++channel)
                if (left[vector][channel] != right[vector][channel])
                    return left[vector][channel] < right[vector][channel] ? -1 : 1;
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
    // Reuse the two std140 padding slots before the first vec4.
    const float composition[2] = {material->settings.effectOpacity,
                                  material->settings.maskCoverage};
    if (!_old || std::memcmp(data->constData() + 72, composition, sizeof(composition))) {
        std::memcpy(data->data() + 72, composition, sizeof(composition));
        changed = true;
    }
    // Nine packed vec4s reuse slots between algorithms instead of growing one
    // uniform block per filter. The std140 payload stays at 224 bytes in total.
    const auto parameters = shaderParameters(material->settings);
    for (size_t index = 0; index < parameters.size(); ++index) {
        const auto &vector    = parameters[index];
        const float values[4] = {vector.x(), vector.y(), vector.z(), vector.w()};
        const int   offset    = 80 + int(index) * 16;
        if (!_old || std::memcmp(data->constData() + offset, values, sizeof(values))) {
            std::memcpy(data->data() + offset, values, sizeof(values));
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
