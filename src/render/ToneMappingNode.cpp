#include "ToneMappingNode.h"
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <cstring>

namespace
{
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
    bool updateUniformData(RenderState &_state, QSGMaterial *, QSGMaterial *_old) override
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
        return changed;
    }
    void updateSampledImage(RenderState &_state, int _binding, QSGTexture **_texture,
                            QSGMaterial *_new, QSGMaterial *) override;
};

class ToneMaterial : public QSGMaterial
{
public:
    QSGTexture           *source = nullptr, *table = nullptr;
    QSGTexture::Filtering filtering = QSGTexture::Linear;
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
        return int(filtering) - int(other->filtering);
    }
};

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

ToneMappingNode::ToneMappingNode(QSGTexture *_source, QSGTexture *_table)
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    m_geometry.setDrawingMode(QSGGeometry::DrawTriangleStrip);
    setGeometry(&m_geometry);
    auto *mapping   = new ToneMaterial;
    mapping->source = _source;
    mapping->table  = _table;
    setMaterial(mapping);
    setFlag(OwnsMaterial);
}
void ToneMappingNode::setTable(QSGTexture *_table)
{
    auto *mapping = static_cast<ToneMaterial *>(material());
    if (mapping->table != _table) {
        mapping->table = _table;
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
