#include "CropShadeNode.h"
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <cstring>

namespace
{
class CropShadeShader : public QSGMaterialShader
{
public:
    CropShadeShader()
    {
        setShaderFileName(VertexStage,
                          QStringLiteral(":/PhotoCrap/src/render/shaders/crop.vert.qsb"));
        setShaderFileName(FragmentStage,
                          QStringLiteral(":/PhotoCrap/src/render/shaders/crop.frag.qsb"));
    }
    bool updateUniformData(RenderState &_state, QSGMaterial *_new, QSGMaterial *_old) override;
};
class CropShadeMaterial : public QSGMaterial
{
public:
    PhotoCrap::Render::CropShadeParameters parameters;
    CropShadeMaterial()
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
        return new CropShadeShader;
    }
    int compare(const QSGMaterial *_other) const override
    {
        const auto &other = static_cast<const CropShadeMaterial *>(_other)->parameters;
        const int   edges = std::memcmp(parameters.edges.data(), other.edges.data(), 64);
        if (edges)
            return edges;
        return parameters.valid == other.valid ? 0 : parameters.valid < other.valid ? -1 : 1;
    }
};
bool CropShadeShader::updateUniformData(RenderState &_state, QSGMaterial *_new, QSGMaterial *_old)
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
    const auto &parameters = static_cast<const CropShadeMaterial *>(_new)->parameters;
    if (!_old || std::memcmp(data->constData() + 80, parameters.edges.data(), 64)) {
        static_assert(sizeof(QVector4D) == 16);
        std::memcpy(data->data() + 80, parameters.edges.data(), 64);
        changed = true;
    }
    if (!_old || std::memcmp(data->constData() + 144, &parameters.valid, sizeof(float))) {
        std::memcpy(data->data() + 144, &parameters.valid, sizeof(float));
        changed = true;
    }
    return changed;
}
} // namespace
CropShadeNode::CropShadeNode(QQuickWindow *_owner)
    : m_owner(_owner), m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    m_geometry.setDrawingMode(QSGGeometry::DrawTriangleStrip);
    m_geometry.setVertexDataPattern(QSGGeometry::StaticPattern);
    setGeometry(&m_geometry);
    setMaterial(new CropShadeMaterial);
    setFlag(OwnsMaterial);
}
bool CropShadeNode::setRect(const QRectF &_rect)
{
    if (m_rect == _rect)
        return false;
    m_rect = _rect;
    // Keep viewport-local coordinates in a separate attribute: Qt may pretransform
    // the position stream while batching or clipping a rotated item.
    QSGGeometry::updateTexturedRectGeometry(&m_geometry, _rect, _rect);
    m_geometry.markVertexDataDirty();
    markDirty(DirtyGeometry);
    return true;
}
bool CropShadeNode::setParameters(const PhotoCrap::Render::CropShadeParameters &_parameters)
{
    auto *shade = static_cast<CropShadeMaterial *>(material());
    if (shade->parameters == _parameters)
        return false;
    shade->parameters = _parameters;
    markDirty(DirtyMaterial);
    return true;
}
