#include "DashedGuideNode.h"
#include <QLineF>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <cmath>
#include <cstring>

namespace
{
struct Vertex {
    float x, y, distance, side, cap;
};
class Shader : public QSGMaterialShader
{
public:
    Shader()
    {
        setShaderFileName(VertexStage,
                          QStringLiteral(":/PhotoCrap/src/render/shaders/dashedguide.vert.qsb"));
        setShaderFileName(FragmentStage,
                          QStringLiteral(":/PhotoCrap/src/render/shaders/dashedguide.frag.qsb"));
    }
    bool updateUniformData(RenderState &_state, QSGMaterial *_new, QSGMaterial *_old) override;
};
class Material : public QSGMaterial
{
public:
    float phase = 0, ratio = 1, pass = 0;
    explicit Material(bool _white) : pass(_white ? 1.f : 0.f)
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
        return new Shader;
    }
    int compare(const QSGMaterial *_other) const override
    {
        const auto *other = static_cast<const Material *>(_other);
        if (pass != other->pass)
            return pass < other->pass ? -1 : 1;
        if (phase != other->phase)
            return phase < other->phase ? -1 : 1;
        return ratio == other->ratio ? 0 : ratio < other->ratio ? -1 : 1;
    }
};
bool Shader::updateUniformData(RenderState &_state, QSGMaterial *_new, QSGMaterial *_old)
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
    const auto *material = static_cast<const Material *>(_new);
    const float parameters[]{material->phase, material->ratio, material->pass};
    if (!_old || std::memcmp(data->constData() + 68, parameters, sizeof(parameters))) {
        std::memcpy(data->data() + 68, parameters, sizeof(parameters));
        changed = true;
    }
    return changed;
}
const QSGGeometry::AttributeSet &attributes()
{
    static const QSGGeometry::Attribute attrs[]{
        QSGGeometry::Attribute::create(0, 2, QSGGeometry::FloatType, true),
        QSGGeometry::Attribute::create(1, 2, QSGGeometry::FloatType),
        QSGGeometry::Attribute::create(2, 1, QSGGeometry::FloatType)};
    static const QSGGeometry::AttributeSet set{3, sizeof(Vertex), attrs};
    return set;
}
} // namespace
class DashedGuidePass : public QSGGeometryNode
{
public:
    explicit DashedGuidePass(bool _white) : m_geometry(attributes(), 0)
    {
        m_geometry.setDrawingMode(QSGGeometry::DrawTriangles);
        m_geometry.setVertexDataPattern(QSGGeometry::StaticPattern);
        setGeometry(&m_geometry);
        setMaterial(new Material(_white));
        setFlag(OwnsMaterial);
    }
    void setMesh(const QVector<Vertex> &_vertices)
    {
        m_geometry.allocate(int(_vertices.size()));
        std::memcpy(m_geometry.vertexData(), _vertices.constData(),
                    size_t(_vertices.size()) * sizeof(Vertex));
        m_geometry.markVertexDataDirty();
        markDirty(DirtyGeometry);
    }
    bool setParameters(float _phase, float _ratio)
    {
        auto *m = static_cast<Material *>(material());
        if (m->phase == _phase && m->ratio == _ratio)
            return false;
        m->phase = _phase;
        m->ratio = _ratio;
        markDirty(DirtyMaterial);
        return true;
    }

private:
    QSGGeometry m_geometry;
};
DashedGuideNode::DashedGuideNode(QQuickWindow *_owner)
    : m_owner(_owner), m_black(new DashedGuidePass(false)), m_white(new DashedGuidePass(true))
{
    // Canvas paints the entire black dash pass before the white pass. Preserve
    // that global order at self-intersections, rather than alternating per edge.
    appendChildNode(m_black);
    appendChildNode(m_white);
}
bool DashedGuideNode::setMesh(const QVector<PhotoCrap::Render::SelectionVertex> &_vertices,
                              quint64 _generation, const QPolygonF &_closure)
{
    if (m_generation == _generation)
        return false;
    m_generation = _generation;
    QVector<Vertex> vertices;
    vertices.reserve(_vertices.size());
    for (const auto &v : _vertices)
        vertices.append({v.x, v.y, v.distance, v.side, 1e6f});
    if (vertices.size() >= 6 && _closure.size() == 3) {
        const auto start     = _closure[0];
        const auto direction = [](QPointF _p) { return _p / std::hypot(_p.x(), _p.y()); };
        const auto first     = direction(_closure[1] - start);
        const auto last      = direction(start - _closure[2]);
        const auto center    = [&](int _a, int _b) {
            return QPointF((vertices[_a].x + vertices[_b].x) * .5,
                           (vertices[_a].y + vertices[_b].y) * .5);
        };
        const int  n           = int(vertices.size());
        const auto normalFirst = QPointF(-first.y(), first.x());
        const auto normalLast  = QPointF(-last.y(), last.x());
        // Qt's dashed stroker emits an open subpath for each dash, including
        // at the closing vertex. Preserve its two independent butt caps.
        const auto extend = [&](int _offset, bool _beginning) {
            const auto along  = _beginning ? first : last;
            const auto normal = _beginning ? normalFirst : normalLast;
            const int plus = _offset + (_beginning ? 0 : 2), minus = _offset + (_beginning ? 1 : 5);
            const float distance = (vertices[plus].distance + vertices[minus].distance) * .5f;
            const auto  edge     = start + along * (_beginning ? -.5 : .5);
            for (int j = 0; j < 6; ++j) {
                auto      &v = vertices[_offset + j];
                const bool replace =
                    _beginning ? (j == 0 || j == 1 || j == 4) : (j == 2 || j == 3 || j == 5);
                if (replace) {
                    const auto p = edge + normal * double(v.side);
                    v.x          = float(p.x());
                    v.y          = float(p.y());
                    v.distance   = distance + (_beginning ? -.5f : .5f);
                }
                const auto p = QPointF(v.x, v.y) - start;
                v.cap        = float(QPointF::dotProduct(p, along)) * (_beginning ? 1.f : -1.f);
            }
        };
        if (QLineF(center(0, 1), start).length() < .001)
            extend(0, true);
        if (QLineF(center(n - 3, n - 1), start).length() < .001)
            extend(n - 6, false);
    }
    m_black->setMesh(vertices);
    m_white->setMesh(vertices);
    return true;
}
bool DashedGuideNode::setParameters(qreal _phase, qreal _ratio)
{
    const float phase = float(std::fmod(_phase, 10.) * 1.5), ratio = float(_ratio);
    const bool  black = m_black->setParameters(phase, ratio);
    return m_white->setParameters(phase, ratio) || black;
}
