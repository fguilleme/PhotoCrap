#include "SelectionOutlineNode.h"
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <cmath>
#include <cstring>

namespace
{
class SelectionShader : public QSGMaterialShader
{
public:
    SelectionShader()
    {
        setShaderFileName(VertexStage,
                          QStringLiteral(":/PhotoCrap/src/render/shaders/selection.vert.qsb"));
        setShaderFileName(FragmentStage,
                          QStringLiteral(":/PhotoCrap/src/render/shaders/selection.frag.qsb"));
    }
    bool updateUniformData(RenderState &_state, QSGMaterial *_new, QSGMaterial *_old) override;
};
class SelectionMaterial : public QSGMaterial
{
public:
    float phase = 0;
    SelectionMaterial()
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
        return new SelectionShader;
    }
    int compare(const QSGMaterial *_other) const override
    {
        const auto *other = static_cast<const SelectionMaterial *>(_other);
        return phase == other->phase ? 0 : phase < other->phase ? -1 : 1;
    }
};
bool SelectionShader::updateUniformData(RenderState &_state, QSGMaterial *_new, QSGMaterial *_old)
{
    auto *data    = _state.uniformData();
    bool  changed = false;
    if (!_old || _state.isMatrixDirty()) {
        std::memcpy(data->data(), _state.combinedMatrix().constData(), 64);
        changed = true;
    }
    if (!_old || _state.isOpacityDirty()) {
        const float opacity = _state.opacity();
        std::memcpy(data->data() + 64, &opacity, sizeof(opacity));
        changed = true;
    }
    const auto phase = static_cast<const SelectionMaterial *>(_new)->phase;
    if (!_old || std::memcmp(data->constData() + 68, &phase, sizeof(phase))) {
        std::memcpy(data->data() + 68, &phase, sizeof(phase));
        changed = true;
    }
    return changed;
}
const QSGGeometry::AttributeSet &attributes()
{
    static const QSGGeometry::Attribute attrs[]{
        QSGGeometry::Attribute::create(0, 2, QSGGeometry::FloatType, true),
        QSGGeometry::Attribute::create(1, 2, QSGGeometry::FloatType)};
    static const QSGGeometry::AttributeSet set{2, sizeof(PhotoCrap::Render::SelectionVertex),
                                               attrs};
    return set;
}
} // namespace
SelectionOutlineNode::SelectionOutlineNode() : m_geometry(attributes(), 0)
{
    m_geometry.setDrawingMode(QSGGeometry::DrawTriangles);
    m_geometry.setVertexDataPattern(QSGGeometry::StaticPattern);
    setGeometry(&m_geometry);
    setMaterial(new SelectionMaterial);
    setFlag(OwnsMaterial);
}
void SelectionOutlineNode::setMesh(const QVector<PhotoCrap::Render::SelectionVertex> &_vertices)
{
    m_geometry.allocate(int(_vertices.size()));
    if (!_vertices.isEmpty())
        std::memcpy(m_geometry.vertexData(), _vertices.constData(),
                    size_t(_vertices.size()) * sizeof(PhotoCrap::Render::SelectionVertex));
    m_geometry.markVertexDataDirty();
    markDirty(DirtyGeometry);
}
void SelectionOutlineNode::setPhase(qreal _phase)
{
    auto       *selection = static_cast<SelectionMaterial *>(material());
    const float phase     = float(std::fmod(_phase, 10.) * 1.5);
    if (selection->phase == phase)
        return;
    selection->phase = phase;
    markDirty(DirtyMaterial);
}
