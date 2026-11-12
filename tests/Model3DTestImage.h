#pragma once

#include <QBuffer>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <bit>
#include <utility>

namespace Model3DTest
{
inline void appendWord(QByteArray &_bytes, quint32 _value)
{
    const auto value = qToLittleEndian(_value);
    _bytes.append(reinterpret_cast<const char *>(&value), sizeof(value));
}

inline QByteArray packGlb(QJsonObject _root, const QByteArray &_binary)
{
    _root["buffers"] = QJsonArray{QJsonObject{{"byteLength", _binary.size()}}};
    auto json        = QJsonDocument(_root).toJson(QJsonDocument::Compact);
    while (json.size() % 4)
        json.append(' ');
    QByteArray glb("glTF");
    appendWord(glb, 2);
    appendWord(glb, 28 + json.size() + _binary.size());
    appendWord(glb, json.size());
    glb += "JSON";
    glb += json;
    appendWord(glb, _binary.size());
    glb.append("BIN\0", 4);
    glb += _binary;
    return glb;
}

enum class Pose { Original, Skinned, Deformed };

inline QByteArray modelFixture(Pose _pose = Pose::Original, int _faceCount = 1,
                               bool _vertexColors = false)
{
    QByteArray  binary;
    const float thirdX = _pose == Pose::Deformed ? -.5f : -.75f;
    for (float value :
         {-1.f, -1.f, 0.f, 1.f, -1.f, 0.f, thirdX, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f})
        appendWord(binary, std::bit_cast<quint32>(value));
    QImage image(4, 4, QImage::Format_ARGB32);
    image.fill(QColor(208, 92, 30));
    QByteArray texture;
    QBuffer    buffer(&texture);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG"))
        return {};
    binary += texture;
    while (binary.size() % 4)
        binary.append('\0');
    auto root = QJsonDocument::fromJson(R"({
        "asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
        "nodes":[{"mesh":0}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"material":0}]}],
        "materials":[{"doubleSided":true,"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
        "textures":[{"source":0}],"images":[{"bufferView":2,"mimeType":"image/png"}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
                     {"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"}]
    })")
                    .object();
    auto views =
        QJsonArray{QJsonObject{{"buffer", 0}, {"byteLength", 36}},
                   QJsonObject{{"buffer", 0}, {"byteOffset", 36}, {"byteLength", 24}},
                   QJsonObject{{"buffer", 0}, {"byteOffset", 60}, {"byteLength", texture.size()}}};
    if (_pose == Pose::Skinned) {
        root["nodes"] = QJsonDocument::fromJson(R"([
            {"children":[1,2]}, {"mesh":0,"skin":0,"rotation":[0,1,0,0]},
            {"children":[3]}, {"translation":[1,0,0]}
        ])")
                            .array();
        root["skins"] =
            QJsonDocument::fromJson(R"([{"joints":[2,3],"inverseBindMatrices":4}])").array();
        root["meshes"] = QJsonDocument::fromJson(R"([{"primitives":[{"attributes":{
            "POSITION":0,"TEXCOORD_0":1,"JOINTS_0":2,"WEIGHTS_0":3},"material":0}]}])")
                             .array();
        auto accessors = root["accessors"].toArray();
        for (const auto &accessor : QJsonDocument::fromJson(R"([
            {"bufferView":3,"componentType":5121,"count":3,"type":"VEC4"},
            {"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"},
            {"bufferView":5,"componentType":5126,"count":2,"type":"MAT4"}
        ])")
                                        .array())
            accessors.append(accessor);
        root["accessors"] = accessors;
        views.append(QJsonObject{{"buffer", 0}, {"byteOffset", binary.size()}, {"byteLength", 12}});
        binary.append("\0\0\0\0\0\0\0\0\0\1\0\0", 12);
        views.append(QJsonObject{{"buffer", 0}, {"byteOffset", binary.size()}, {"byteLength", 48}});
        for (float v : {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, .5f, .5f, 0.f, 0.f})
            appendWord(binary, std::bit_cast<quint32>(v));
        views.append(
            QJsonObject{{"buffer", 0}, {"byteOffset", binary.size()}, {"byteLength", 128}});
        for (float translation : {0.f, -.5f})
            for (float v : {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, translation,
                            0.f, 0.f, 1.f})
                appendWord(binary, std::bit_cast<quint32>(v));
    }
    if (_faceCount > 1) {
        auto mesh            = root["meshes"].toArray().first().toObject();
        auto primitive       = mesh["primitives"].toArray().first().toObject();
        auto accessors       = root["accessors"].toArray();
        primitive["indices"] = accessors.size();
        accessors.append(QJsonObject{{"bufferView", views.size()},
                                     {"componentType", 5121},
                                     {"count", _faceCount * 3},
                                     {"type", "SCALAR"}});
        mesh["primitives"] = QJsonArray{primitive};
        root["meshes"]     = QJsonArray{mesh};
        root["accessors"]  = accessors;
        views.append(QJsonObject{
            {"buffer", 0}, {"byteOffset", binary.size()}, {"byteLength", _faceCount * 3}});
        for (int face = 0; face < _faceCount; ++face)
            binary.append("\0\1\2", 3);
        while (binary.size() % 4)
            binary.append('\0');
    }
    if (_vertexColors) {
        auto mesh             = root["meshes"].toArray().first().toObject();
        auto primitive        = mesh["primitives"].toArray().first().toObject();
        auto attributes       = primitive["attributes"].toObject();
        auto accessors        = root["accessors"].toArray();
        attributes["COLOR_0"] = accessors.size();
        accessors.append(QJsonObject{
            {"bufferView", views.size()}, {"componentType", 5126}, {"count", 3}, {"type", "VEC4"}});
        primitive["attributes"] = attributes;
        mesh["primitives"]      = QJsonArray{primitive};
        root["meshes"]          = QJsonArray{mesh};
        root["accessors"]       = accessors;
        views.append(QJsonObject{{"buffer", 0}, {"byteOffset", binary.size()}, {"byteLength", 48}});
        for (float v : {1.f, .5f, .25f, 1.f, .25f, 1.f, .5f, 1.f, .5f, .25f, 1.f, .5f})
            appendWord(binary, std::bit_cast<quint32>(v));
    }
    root["bufferViews"] = views;
    return packGlb(std::move(root), binary);
}

inline QByteArray denseGrid(int _cells)
{
    const auto fixture = modelFixture();
    if (fixture.isEmpty())
        return {};
    const auto jsonSize = qFromLittleEndian<quint32>(fixture.constData() + 12);
    auto       root     = QJsonDocument::fromJson(fixture.mid(20, jsonSize)).object();
    const auto texture  = root["bufferViews"].toArray().at(2).toObject();
    QByteArray binary =
        fixture.mid(28 + jsonSize + texture["byteOffset"].toInt(), texture["byteLength"].toInt());
    while (binary.size() % 4)
        binary.append('\0');
    const auto vertexStart = binary.size();
    for (int y = 0; y <= _cells; ++y)
        for (int x = 0; x <= _cells; ++x) {
            const float u = float(x) / _cells, v = float(y) / _cells;
            const float a = u - .5f, b = v - .5f;
            for (const float value : {a, b, .25f * (a * a + b * b), u, v})
                appendWord(binary, std::bit_cast<quint32>(value));
        }
    const auto indexStart = binary.size();
    for (int y = 0; y < _cells; ++y)
        for (int x = 0; x < _cells; ++x) {
            const int i = y * (_cells + 1) + x;
            for (const int index :
                 {i, i + 1, i + _cells + 1, i + 1, i + _cells + 2, i + _cells + 1})
                appendWord(binary, index);
        }
    const int vertices = (_cells + 1) * (_cells + 1);
    root["meshes"]     = QJsonDocument::fromJson(R"([{"primitives":[{
        "attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}])")
                         .array();
    root["accessors"] = QJsonArray{
        QJsonObject{
            {"bufferView", 0}, {"componentType", 5126}, {"count", vertices}, {"type", "VEC3"}},
        QJsonObject{{"bufferView", 0},
                    {"byteOffset", 12},
                    {"componentType", 5126},
                    {"count", vertices},
                    {"type", "VEC2"}},
        QJsonObject{{"bufferView", 1},
                    {"componentType", 5125},
                    {"count", _cells * _cells * 6},
                    {"type", "SCALAR"}}};
    root["bufferViews"] = QJsonArray{
        QJsonObject{{"buffer", 0},
                    {"byteOffset", vertexStart},
                    {"byteLength", indexStart - vertexStart},
                    {"byteStride", 20}},
        QJsonObject{
            {"buffer", 0}, {"byteOffset", indexStart}, {"byteLength", binary.size() - indexStart}},
        QJsonObject{{"buffer", 0}, {"byteLength", texture["byteLength"]}}};
    return packGlb(std::move(root), binary);
}
} // namespace Model3DTest
