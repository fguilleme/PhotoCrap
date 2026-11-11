#pragma once

#include "Model3D.h"
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <array>

namespace PhotoCrap::Model3D
{
struct Vertex {
    QVector3D position, normal;
    QVector2D uv;
    QVector4D color{1, 1, 1, 1};
};

struct Material {
    QVector4D color{1, 1, 1, 1};
    QImage    texture;
    bool      doubleSided = false, unlit = false, alphaMask = false;
    float     alphaCutoff = .5f;
    enum class Wrap { repeat, clamp, mirror };
    Wrap u = Wrap::repeat, v = Wrap::repeat;
};

struct Primitive {
    QVector<Vertex>                 vertices;
    QVector<std::array<quint32, 3>> triangles;
    Material                        material;
    bool                            vertexColors = false;
};

struct Asset {
    QByteArray         bytes;
    qsizetype          bytesInMemory = 0;
    QVector<Primitive> primitives;
};
} // namespace PhotoCrap::Model3D
