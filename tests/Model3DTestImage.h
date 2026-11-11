#pragma once

#include <QBuffer>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <bit>

namespace Model3DTest
{
inline void appendWord(QByteArray &_bytes, quint32 _value)
{
    const auto value = qToLittleEndian(_value);
    _bytes.append(reinterpret_cast<const char *>(&value), sizeof(value));
}

enum class Pose { Original, Skinned, Deformed };

inline QByteArray modelFixture(Pose _pose = Pose::Original)
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
    root["bufferViews"] = views;
    root["buffers"]     = QJsonArray{QJsonObject{{"byteLength", binary.size()}}};
    auto json           = QJsonDocument(root).toJson(QJsonDocument::Compact);
    while (json.size() % 4)
        json.append(' ');
    QByteArray glb("glTF");
    appendWord(glb, 2);
    appendWord(glb, 28 + json.size() + binary.size());
    appendWord(glb, json.size());
    glb += "JSON";
    glb += json;
    appendWord(glb, binary.size());
    glb.append("BIN\0", 4);
    glb += binary;
    return glb;
}
} // namespace Model3DTest
